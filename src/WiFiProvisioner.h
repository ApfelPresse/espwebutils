 
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
        _staMode(false)
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
      Serial.println("[WARN] LittleFS konnte nicht gestartet werden; benutze eingebettete Webfiles.");
      if (_onStatus) _onStatus("LittleFS nicht gemountet — benutze eingebettete Webfiles.");
    }

    // ensure admin password exists (so it's available regardless of STA/AP)
    _ensureAdminPassword();
    // print admin password for debugging (remove in production)
    const char* adminPw = model.admin.pass;
    Serial.println(String("[ADMIN] password: ") + adminPw);

    // Try to connect to previously stored WiFi credentials
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
      Serial.println("[OTA] Passwort: " + ota.getPassword());
    } else {
      _staMode = false;
      _startAccessPoint();
    }

    _registerRoutes();
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
    handleDnsLoop();

    if (millis() - lastOta >= 50) {
      lastOta = millis();
      ota.handle();
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
    
  bool _connectToWiFi()
  {
    const char* ssid = model.wifi.ssid;
    const char* pass = model.wifi.pass;

    if (!ssid || ssid[0] == '\0')
    {
      Serial.println("[STA] Keine Zugangsdaten gespeichert");
      if (_onStatus)
        _onStatus("Keine WLAN-Zugangsdaten gespeichert.");
      return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    if (_onStatus)
      _onStatus(String("Verbinde mit WLAN: ") + ssid);
    Serial.printf("[STA] Verbinde mit %s ...\n", ssid);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      Serial.printf("[STA] Verbunden: %s\n", ip.c_str());
      if (_onStatus)
        _onStatus("WLAN verbunden:\n" + ip);
      return true;
    }

    Serial.println("[STA] Verbindung fehlgeschlagen");
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
      Serial.println("[INFO] " + msg);
      if (_onStatus)
        _onStatus(msg);
    }
    else
    {
      Serial.println("[ERROR] mDNS konnte nicht gestartet werden");
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
    Serial.println("[INFO] Webserver gestartet (AP-Modus)");

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
      // Serve embedded static files (css/js/fonts...) when no explicit STA route matches
      server.onNotFound([this](AsyncWebServerRequest *request)
                        { _serveFileWithFallback(request, "/admin.html"); });
    }
    else
    {
      server.onNotFound([this](AsyncWebServerRequest *request)
                        { _serveFileWithFallback(request, _fallbackFile); });

      server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                { _serveFileWithFallback(request, _fallbackFile); });

      server.on("/info", HTTP_GET, [this](AsyncWebServerRequest *request)
                { request->send(200, "text/html", _infoMessage); });
    }

    // if (_graphs) _graphs->begin();
    //if (_graphsWs) server.addHandler(_graphsWs);

    server.on("/graphs", HTTP_GET, [this](AsyncWebServerRequest *request)
          { _serveFileWithFallback(request, "/graphs.html"); });

    server.begin();
  }

  // Route handlers (extracted for clarity)
  void _handleScan(AsyncWebServerRequest *request)
  {
    int n = WiFi.scanComplete();
    if (n == -2) WiFi.scanNetworks(true);
    if (n == -1) {
      request->send(200, "application/json", "[]");
      return;
    }

    // Update model with available networks
    model.wifi.available_networks.clear();
    for (int i = 0; i < n && i < WifiSettings::MAX_NETWORKS; ++i) {
      StaticString<WifiSettings::SSID_LEN> ssid;
      ssid.set(WiFi.SSID(i).c_str());
      model.wifi.available_networks.add(ssid);
    }
    // Trigger WebSocket update via model broadcast
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
      Serial.println("[ADMIN] Generated password: " + newPw);
      if (_onStatus) _onStatus("Admin password generated.");
    }
    else
    {
      Serial.println("[ADMIN] Password exists.");
      Serial.println(String("[ADMIN] Password: ") + pw);
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
