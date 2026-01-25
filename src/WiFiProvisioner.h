 
#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <functional>

#include "webfiles.h"
#include "Model.h"
#include "OtaUpdate.h"
// #include "LiveGraphManager.h"
#include "TimeSync.h"
#include "AdminPage.h"
#include "Logger.h"


class WiFiProvisioner
{
public:
  using StatusCallback = std::function<void(const String &)>;

  AsyncWebServer server;
  DNSServer dns;
  OtaUpdate ota;
  TimeSync timeSync;
  Model model;

  WiFiProvisioner()
      : server(80),
        _apSsid("ESP-Setup"),
        _apPass(""),
        _mdnsHost("esp32"),
        _fallbackFile("/wifi.html"),
        _infoMessage("<h1>Bitte</h1> wählen Sie ein WLAN aus, um den ESP32 zu konfigurieren."),
        _staMode(false),
        _pendingRestart(false),
        _restartTime(0)
  {
    //_graphsWs = new AsyncWebSocket("/ws/graphs");
    //_graphs = new LiveGraphManager(*_graphsWs, 20);
  }

  void enableOtaUpdates(bool en = true) { ota.setEnabled(en); }
  void setOtaPassword(const String &pass = "") { ota.setPassword(pass); }
  String getOtaPassword() { return ota.getPassword(); }
  void setOtaPort(uint16_t port) { ota.setPort(port); }
  void setOtaWindowSeconds(uint32_t s) { ota.setWindowSeconds(s); }

  void setApSsid(const String &ssid) { _apSsid = ssid; }
  void setApPassword(const String &pass) { _apPass = pass; }
  void setMdnsHost(const String &host) { _mdnsHost = host; }
  void setFallbackFile(const String &path) { _fallbackFile = path; }
  void setInfoMessage(const String &msg) { _infoMessage = msg; }

  void onStatus(StatusCallback cb) { _onStatus = cb; }

  void requireAdmin(bool en) { _requireAdmin = en; }

  //void pushData(const String &graph, const String &label, double y) { if (_graphs) _graphs->pushData(graph, label, y); }
  //void pushData(const String &graph, const String &label, double x, double y) { if (_graphs) _graphs->pushData(graph, label, x, y); }

  void begin()
  {
    // try to mount LittleFS (optional: fallback to embedded webfiles)
    _littleFsAvailable = LittleFS.begin();
    if (!_littleFsAvailable) {
      LOG_WARN("LittleFS konnte nicht gestartet werden; benutze eingebettete Webfiles.");
      if (_onStatus) _onStatus("LittleFS nicht gemountet — benutze eingebettete Webfiles.");
    }

    // Initialize Model FIRST (loads Preferences before trying to connect)
    LOG_TRACE("[Init] Initializing Model and loading Preferences");
    model.begin();
    
    // DEBUG: Show what was loaded from Preferences
    LOG_TRACE_F("[Init] WiFi credentials loaded - SSID: '%s'", model.wifi.ssid.get().c_str());
    LOG_TRACE_F("[Init] WiFi credentials loaded - PASS: '%s'  <<< TRACE DEBUG ONLY (LENGTH: %d)", 
                model.wifi.pass.get().c_str(), strlen(model.wifi.pass.get().c_str()));
    
    // ensure admin password exists (so it's available regardless of STA/AP)
    _ensureAdminPassword();
    // print admin password for debugging (remove in production)
    const char* adminPw = model.admin.pass;
    LOG_INFO_F("[ADMIN] password: %s", adminPw);

    // Try to connect to previously stored WiFi credentials (now loaded from Preferences)
    if (_connectToWiFi()) {
      _staMode = true;
      _startMdns();

      // ensure admin pass exists
      _ensureAdminPassword();

      timeSync.begin("CET-1CEST,M3.5.0/2,M10.5.0/3");

      // if (_graphs) {
      //   _graphs->setNowProvider([this]() {
      //     if (timeSync.isValid())
      //       return timeSync.nowEpochSeconds();
      //     return (double)millis() / 1000.0;
      //   });
      // }

      ota.load();
      ota.onStatus([this](const String &s) { if (_onStatus) _onStatus(s); });
      ota.setHostname(_mdnsHost);
      ota.beginIfNeeded(_mdnsHost);
      LOG_INFO("[OTA] Passwort: " + ota.getPassword());
    } else {
      _staMode = false;
      _startAccessPoint();
    }

    _registerRoutes();
    
    // Initialize log level from model (default to INFO if not set)
    int level = model.wifi.log_level.get();
    if (level < 0 || level > 4) {
      level = 0; // INFO
      model.wifi.log_level = level;
    }
    Logger::setLevel(static_cast<LogLevel>(level));
    LOG_INFO_F("Log level initialized to: %s (%d)", Logger::levelToString(static_cast<LogLevel>(level)), level);
    
    // Register callback for WiFi updates - schedule restart instead of direct reconnect
    // to avoid watchdog timeout in WebSocket handler context
    model.onWifiUpdate = [this]() {
      LOG_INFO("[WiFi] WiFi credentials updated via WebSocket, scheduling restart in 2 seconds");
      this->_pendingRestart = true;
      this->_restartTime = millis() + 2000;
    };
    
    // Start initial WiFi scan to populate available_networks
    LOG_INFO("Starting initial WiFi scan...");
    WiFi.scanNetworks(true);
  }

  void handleDnsLoop()
  {
    if (WiFi.softAPgetStationNum() > 0)
    {
      dns.processNextRequest();
    }
  }

  void handleLoop()
  {
    // Handle pending restart (from WiFi credential update)
    if (_pendingRestart && millis() >= _restartTime) {
      LOG_INFO("[WiFi] Restarting ESP to apply new WiFi credentials...");
      LOG_INFO("[WiFi] Waiting for Preferences to flush...");
      delay(500);  // Give time for Preferences to write and final messages to flush
      ESP.restart();
    }
    
    handleDnsLoop();

    if (millis() - lastOta >= 50) {
      lastOta = millis();
      ota.handle();
    }

    // Check if WiFi scan completed and update model
    int n = WiFi.scanComplete();
    if (n >= 0) {
      LOG_INFO_F("[Loop] WiFi scan completed, found %d networks", n);
      model.wifi.available_networks.get().clear();
      for (int i = 0; i < n && i < WifiSettings::MAX_NETWORKS; ++i) {
        StringBuffer<WifiSettings::SSID_LEN> ssid;
        ssid.set(WiFi.SSID(i).c_str());
        model.wifi.available_networks.get().add(ssid);
        LOG_TRACE_F("[Loop] Added network: %s (RSSI: %d)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
      WiFi.scanDelete();
      LOG_DEBUG("[Loop] Broadcasting updated network list via WebSocket");
      model.broadcastAll();
    }

    // if (millis() - lastCleanup >= 1000)
    // {
    //   lastCleanup = millis();
    //   if (_graphs) _graphs->cleanup();
    // }

    yield();
  }

private:
  String _apSsid, _apPass, _mdnsHost, _fallbackFile, _infoMessage;
  // AsyncWebSocket* _graphsWs = nullptr;
  // LiveGraphManager* _graphs = nullptr;
  bool _requireAdmin = true;
  bool _staMode;
  bool _littleFsAvailable = false;
  StatusCallback _onStatus = nullptr;
  uint32_t lastOta = 0;
  uint32_t lastCleanup = 0;
  bool _pendingRestart = false;
  unsigned long _restartTime = 0;
    
  bool _connectToWiFi()
  {
    const char* ssid = model.wifi.ssid;
    const char* pass = model.wifi.pass;

    LOG_TRACE_F("[STA] _connectToWiFi called, SSID from model: %s", ssid ? ssid : "null");
    LOG_TRACE_F("[STA] Password length: %d, first 3 chars: %s***", 
                pass ? strlen(pass) : 0,
                (pass && strlen(pass) >= 3) ? String(String(pass).substring(0, 3)).c_str() : "EMPTY");
    
    if (!ssid || ssid[0] == '\0')
    {
      LOG_INFO("[STA] Keine Zugangsdaten gespeichert");
      if (_onStatus)
        _onStatus("Keine WLAN-Zugangsdaten gespeichert.");
      return false;
    }

    WiFi.mode(WIFI_STA);
    LOG_TRACE_F("[STA] Attempting WiFi.begin with SSID: %s", ssid);
    // DEBUG: Print full password only at TRACE level for troubleshooting
    LOG_TRACE_F("[STA] WiFi.begin(SSID='%s', PASS='%s')  <<< TRACE DEBUG ONLY", ssid, pass ? pass : "");
    WiFi.begin(ssid, pass);

    if (_onStatus)
      _onStatus(String("Verbinde mit WLAN: ") + ssid);
    LOG_INFO_F("[STA] Verbinde mit %s ...", ssid);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
    {
      yield();  // Yield to let other tasks run (async tcp, etc)
      delay(100);  // Shorter delay with yield to prevent watchdog timeout
      LOG_TRACE(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      LOG_INFO_F("[STA] Verbunden: %s", ip.c_str());
      LOG_TRACE_F("[STA] BSSID: %s, RSSI: %d", WiFi.BSSIDstr().c_str(), WiFi.RSSI());
      if (_onStatus)
        _onStatus("WLAN verbunden:\n" + ip);
      
      // If we're in AP mode (i.e., were provisioning), restart after successful connection
      if (!_staMode) {
        LOG_INFO("[WiFi] Successfully connected from AP mode, restarting ESP in STA mode...");
        delay(1000);  // Give time for any final messages
        ESP.restart();
      }
      
      return true;
    }

    LOG_WARN("[STA] Verbindung fehlgeschlagen");
    LOG_TRACE("[STA] WiFi status still not connected after 15s timeout");
    if (_onStatus)
      _onStatus("WLAN-Verbindung fehlgeschlagen.");
    return false;
  }

  void _startMdns()
  {
    if (MDNS.begin(_mdnsHost.c_str()))
    {
      MDNS.addService("arduino", "tcp", 3232); // OTA Service annoncieren
      String msg = "Erreichbar unter:\n" + _mdnsHost + ".local";
      LOG_INFO(msg);
      if (_onStatus)
        _onStatus(msg);
    }
    else
    {
      LOG_ERROR("mDNS konnte nicht gestartet werden");
      if (_onStatus)
        _onStatus("Fehler: mDNS konnte nicht gestartet werden.");
    }
  }

  void _startAccessPoint()
  {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(8, 8, 8, 8), IPAddress(8, 8, 8, 8), IPAddress(255, 255, 255, 0));
    WiFi.softAP(_apSsid.c_str(), _apPass.c_str());

    dns.start(53, "*", IPAddress(8, 8, 8, 8));

    if (_onStatus)
      _onStatus("Starte Access Point: " + _apSsid);
    LOG_INFO("Webserver gestartet (AP-Modus)");

    server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { _handleScan(request); });

    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {}, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
      _handleSave(request, data, len);
    });

    // Generic set/get API
    // deprecated use model meachnic
    // server.on("/set", HTTP_POST, [this](AsyncWebServerRequest *request) {}, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
    //   _handleSet(request, data, len);
    // });

    // deprecated use model meachnic
    // server.on("/get", HTTP_GET, [this](AsyncWebServerRequest *request) { _handleGet(request); });
  }

  void _registerRoutes()
  {
    if (_staMode)
    {
      // Admin routes moved to separate module
      AdminPage::registerAdminRoutes(server, _requireAdmin, model);

      server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
                {
        DynamicJsonDocument doc(256);
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["mdns"] = String("http://") + WiFi.getHostname() + ".local/";
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); });

      server.on("/ota/status", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
        DynamicJsonDocument doc(384);

        doc["enabled"] = ota.isEnabled();
        doc["started"] = ota.isStarted();
        doc["host"] = ota.getHostname().length() ? ota.getHostname() : String(WiFi.getHostname());
        doc["port"] = ota.getPort();
        doc["windowSeconds"] = ota.getWindowSeconds();

        uint32_t rem = ota.getRemainingSeconds();
        if (ota.getWindowSeconds() == 0) {
          doc["remainingSeconds"] = nullptr;   // unbegrenzt
        } else {
          doc["remainingSeconds"] = rem;
        }

        doc["password"] = ota.isEnabled() ? ota.getPassword() : "";

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); });

      server.on("/ota/regenerate-password", HTTP_POST, [this](AsyncWebServerRequest *request)
                {
  DynamicJsonDocument doc(256);

  String newPass = ota.regeneratePassword();
  doc["ok"] = true;
  doc["password"] = newPass;

  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json); });

      server.on("/ota/extend", HTTP_POST, [this](AsyncWebServerRequest *request)
                {
  ota.restartWindow();

  DynamicJsonDocument doc(128);
  doc["ok"] = true;
if (ota.getWindowSeconds() == 0) {
  doc["remainingSeconds"] = nullptr;   // JSON null
} else {
  doc["remainingSeconds"] = ota.getRemainingSeconds();
}

  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json); });

      server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request)
                {
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
        request->send(200, "text/plain", "WLAN-Daten gelöscht. Neustart...");
        delay(1000);
        ESP.restart(); });

      server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
        if (_littleFsAvailable) {
          request->send(LittleFS, "/index.html", "text/html");
        } else {
          _serveFileWithFallback(request, "/admin.html");
        }
      });
    }
    else
    {
      server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                { _serveFileWithFallback(request, _fallbackFile); });

      server.on("/info", HTTP_GET, [this](AsyncWebServerRequest *request)
                { request->send(200, "text/html", _infoMessage); });
    }

    // if (_graphs) _graphs->begin();
    //if (_graphsWs) server.addHandler(_graphsWs);

    server.on("/graphs", HTTP_GET, [this](AsyncWebServerRequest *request)
          { _serveFileWithFallback(request, "/graphs.html"); });

    // Model already initialized in begin(), now just attach to server
    // Attach model WebSocket to main HTTP server before onNotFound/server.begin
    model.attachTo(server);

    // Register onNotFound handlers AFTER model.begin() so WebSocket routes are registered first
    if (_staMode)
    {
      server.onNotFound([this](AsyncWebServerRequest *request)
                        { _serveFileWithFallback(request, "/admin.html"); });
    }
    else
    {
      server.onNotFound([this](AsyncWebServerRequest *request)
                        { _serveFileWithFallback(request, _fallbackFile); });
    }

    // Start HTTP server once all handlers are registered
    server.begin();
  }

  // Route handlers (extracted for clarity)
  void _handleScan(AsyncWebServerRequest *request)
  {
    LOG_DEBUG("[Scan] Scan request received");
    int n = WiFi.scanComplete();
    LOG_DEBUG_F("[Scan] scanComplete returned: %d", n);
    
    if (n == -2) {
      LOG_DEBUG("[Scan] Starting async scan...");
      WiFi.scanNetworks(true);
    }
    if (n == -1) {
      LOG_DEBUG("[Scan] Scan in progress, returning empty array");
      request->send(200, "application/json", "[]");
      return;
    }

    LOG_INFO_F("[Scan] Found %d networks", n);
    
    // Update model with available networks
    model.wifi.available_networks.get().clear();
    for (int i = 0; i < n && i < WifiSettings::MAX_NETWORKS; ++i) {
      StringBuffer<WifiSettings::SSID_LEN> ssid;
      ssid.set(WiFi.SSID(i).c_str());
      model.wifi.available_networks.get().add(ssid);
      LOG_TRACE_F("[Scan] Added network: %s", WiFi.SSID(i).c_str());
    }
    // Trigger WebSocket update via model broadcast
    LOG_DEBUG("[Scan] Broadcasting model update via WebSocket");
    model.broadcastAll();

    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n; ++i) {
      JsonObject obj = arr.createNestedObject();
      obj["ssid"] = WiFi.SSID(i);
      obj["rssi"] = WiFi.RSSI(i);
      obj["bssid"] = WiFi.BSSIDstr(i);
    }
    WiFi.scanDelete();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  }

  void _handleSave(AsyncWebServerRequest *request, uint8_t *data, size_t len)
  {
    DynamicJsonDocument doc(256);
    auto err = deserializeJson(doc, (const char*)data, len);
    if (err) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
      return;
    }
    if (!doc.containsKey("ssid") || !doc.containsKey("pass")) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_fields\"}");
      return;
    }
    // update model and persist
    model.wifi.ssid = doc["ssid"].as<const char*>();
    model.wifi.pass = doc["pass"].as<const char*>();
    request->send(200, "application/json", "{\"ok\":true}\n");
    delay(1000);
    ESP.restart();
  }

  // void _handleSet(AsyncWebServerRequest *request, uint8_t *data, size_t len)
  // {
  //   if (!_isAdminAuthorized(request)) {
  //     request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //     return;
  //   }

  //   DynamicJsonDocument doc(512);
  //   auto err = deserializeJson(doc, (const char*)data, len);
  //   if (err) {
  //     request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
  //     return;
  //   }

  //   // expected: { "ns": "namespace", "key": "key", "value": "..." }
  //   if (!doc.containsKey("ns") || !doc.containsKey("key") || !doc.containsKey("value")) {
  //     request->send(400, "application/json", "{\"ok\":false,\"error\":\"ns_key_value_required\"}");
  //     return;
  //   }
  //   String ns = doc["ns"].as<String>();
  //   String key = doc["key"].as<String>();
  //   String value = doc["value"].as<String>();
  //   model.setPref(ns.c_str(), key.c_str(), value);
  //   request->send(200, "application/json", "{\"ok\":true}\n");
  // }

  // void _handleGet(AsyncWebServerRequest *request)
  // {
  //   // protect endpoint
  //   if (!_isAdminAuthorized(request)) {
  //     request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //     return;
  //   }

  //   // expects query params ns and key
  //   if (!request->hasParam("ns") || !request->hasParam("key")) {
  //     request->send(400, "application/json", "{\"ok\":false,\"error\":\"ns_key_required\"}");
  //     return;
  //   }
  //   String ns = request->getParam("ns")->value();
  //   String key = request->getParam("key")->value();
  //   String val = model.getPref(ns.c_str(), key.c_str(), "");
  //   DynamicJsonDocument doc(256);
  //   doc["ok"] = true;
  //   doc["value"] = val;
  //   String json;
  //   serializeJson(doc, json);
  //   request->send(200, "application/json", json);
  // }

  void _serveFileWithFallback(AsyncWebServerRequest *request, const String &fallbackPath)
  {
    const String uri = request->url();
    Serial.printf("[HTTP] request: %s\n", uri.c_str());
    const WebFile *match = _findWebFile(uri);
    const WebFile *fallback = _findWebFile(fallbackPath);

    const WebFile *fileToSend = match ? match : fallback;
    if (fileToSend)
    {
      Serial.printf("[HTTP] serving: %s (match=%s)\n", fileToSend->path, match ? "yes" : "no");
      String contentType = _getContentType(fileToSend->path);
      AsyncWebServerResponse *response = request->beginResponse(200, contentType, fileToSend->data, fileToSend->size);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }
    else
    {
      Serial.printf("[WARN] Kein Match für %s, fallback: %s\n", uri.c_str(), fallbackPath.c_str());
      request->send(404, "text/plain", "Not Found");
    }
  }

  String _getContentType(const String &path)
  {
    if (path.endsWith(".html"))
      return "text/html";
    if (path.endsWith(".css"))
      return "text/css";
    if (path.endsWith(".js"))
      return "application/javascript";
    if (path.endsWith(".woff2"))
      return "font/woff2";
    if (path.endsWith(".ico"))
      return "image/x-icon";
    return "text/plain";
  }

  // Find embedded webfile by path, or nullptr
  const WebFile *_findWebFile(const String &path)
  {
    for (size_t i = 0; i < webFilesCount; ++i)
    {
      if (String(webFiles[i].path) == path)
        return &webFiles[i];
    }
    return nullptr;
  }

  // Admin password helpers
  void _ensureAdminPassword()
  {
    const char* pw = model.admin.pass;
    if (!pw || pw[0] == '\0')
    {
      String newPw = _generatePassword(12);
      model.admin.pass = newPw.c_str();
      LOG_INFO("[ADMIN] Generated password: " + newPw);
      if (_onStatus) _onStatus("Admin password generated.");
    }
    else
    {
      LOG_INFO("[ADMIN] Password exists.");
      LOG_INFO_F("[ADMIN] Password: %s", pw);
    }
  }

  String _regenerateAdminPassword()
  {
    String pw = _generatePassword(12);
    model.admin.pass = pw.c_str();
    return pw;
  }

  String _generatePassword(size_t len)
  {
    static const char *chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789"; // no ambiguous
    String out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i)
    {
      uint32_t r = esp_random();
      out += chars[r % strlen(chars)];
    }
    return out;
  }

  // Check whether the incoming request provides correct admin credentials.
  // Accepts either a header `X-Admin-Pass` or a query param `pw`.
  bool _isAdminAuthorized(AsyncWebServerRequest *request)
  {
    if (!_requireAdmin) return true;

    const char* stored = model.admin.pass;

    if (!stored || stored[0] == '\0') return false;

    // check cookie-based session first
    if (request->hasHeader("Cookie")) {
      const AsyncWebHeader* hc = request->getHeader("Cookie");
      if (hc) {
        String cookies = hc->value();
        int idx = cookies.indexOf("admin_session=");
        if (idx >= 0) {
          int start = idx + strlen("admin_session=");
          int end = cookies.indexOf(';', start);
          String token = (end > 0) ? cookies.substring(start, end) : cookies.substring(start);
          const char* storedToken = model.admin.session;
          if (storedToken && storedToken[0] != '\0' && token == storedToken) return true;
        }
      }
    }

    // check header
    if (request->hasHeader("X-Admin-Pass")) {
      const AsyncWebHeader* h = request->getHeader("X-Admin-Pass");
      if (h && h->value() == String(stored)) return true;
    }

    // check query param ?pw=...
    if (request->hasParam("pw")) {
      const AsyncWebParameter* p = request->getParam("pw");
      if (p && p->value() == String(stored)) return true;
    }

    return false;
  }
};
