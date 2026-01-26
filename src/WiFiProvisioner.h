 
#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <cstring>
#include <functional>

#include "webfiles.h"
#include "Model.h"
#include "OtaUpdate.h"
// #include "LiveGraphManager.h"
#include "TimeSync.h"
#include "AdminPage.h"
#include "Logger.h"
#include "Periodic.h"


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
  void setMdnsHost(const String &host) { _mdnsHost = host; _mdnsHostUserSet = true; }
  void setFallbackFile(const String &path) { _fallbackFile = path; }
  void setInfoMessage(const String &msg) { _infoMessage = msg; }

  void onStatus(StatusCallback cb) { _onStatus = cb; }

  void requireAdmin(bool en) { _requireAdmin = en; }

  //void pushData(const String &graph, const String &label, double y) { if (_graphs) _graphs->pushData(graph, label, y); }
  //void pushData(const String &graph, const String &label, double x, double y) { if (_graphs) _graphs->pushData(graph, label, x, y); }

  void begin()
  {
    LOG_INFO("========== WiFi Provisioner BEGIN ==========");
    
    // ===== STEP 1: Mount Filesystem =====
    LOG_INFO("[INIT] SCHRITT 1: LittleFS mounting...");
    _littleFsAvailable = LittleFS.begin();
    if (!_littleFsAvailable) {
      LOG_WARN("[INIT] LittleFS konnte nicht gestartet werden; benutze eingebettete Webfiles.");
      if (_onStatus) _onStatus("LittleFS nicht gemountet — benutze eingebettete Webfiles.");
    } else {
      LOG_INFO("[INIT] LittleFS erfolgreich gemountet.");
    }

    // ===== STEP 2: Load Model & Preferences =====
    LOG_INFO("[INIT] SCHRITT 2: Model und Preferences laden...");
    model.begin();  // Calls ModelBase::begin() and ensurePasswords()
    const char *modelMdns = model.mdns.mdns_domain;
    if (!_mdnsHostUserSet && modelMdns && modelMdns[0] != '\0') {
      _mdnsHost = modelMdns;
      LOG_INFO_F("[INIT] mDNS Hostname aus Model übernommen: %s", _mdnsHost.c_str());
    } else if (_mdnsHostUserSet) {
      std::strncpy(model.mdns.mdns_domain, _mdnsHost.c_str(), MDNSSettings::MDNS_LEN - 1);
      model.mdns.mdns_domain[MDNSSettings::MDNS_LEN - 1] = '\0';
      model.saveTopic("mdns");
      LOG_INFO_F("[INIT] mDNS Hostname aus setMdnsHost() übernommen und ins Model gespeichert: %s", _mdnsHost.c_str());
    }
    LOG_INFO_F("[INIT] Credentials geladen - SSID: '%s'", model.wifi.ssid.get().c_str());
    LOG_INFO_F("[INIT] Admin-Passwort: %s", model.admin.pass);

    // ===== STEP 3: Try STA Mode Connection =====
    LOG_INFO("[INIT] SCHRITT 3: Versuche STA-Modus (WLAN-Verbindung)...");
    _staMode = true;  // Mark as STA before attempting connection
    
    if (_connectToWiFi()) {
      LOG_INFO("[INIT] ✓ WiFi-Verbindung erfolgreich!");
      _startMdns();
      timeSync.begin("CET-1CEST,M3.5.0/2,M10.5.0/3");
      ota.load();
      ota.onStatus([this](const String &s) { if (_onStatus) _onStatus(s); });
      ota.setHostname(_mdnsHost);
      ota.beginIfNeeded(_mdnsHost);
      LOG_INFO("[INIT] OTA Update aktiviert");
    } else {
      LOG_WARN("[INIT] ✗ WiFi-Verbindung fehlgeschlagen! Starte AP-Modus...");
      _staMode = false;
      _startAccessPoint();
    }

    // ===== STEP 4: Register Web Routes =====
    LOG_INFO("[INIT] SCHRITT 4: Web-Routen registrieren...");
    _registerRoutes();
    
    // ===== STEP 5: Setup WiFi Callbacks =====
    LOG_INFO("[INIT] SCHRITT 5: WiFi-Callbacks konfigurieren...");
    model.onWifiUpdate = [this]() {
      LOG_WARN("[WiFi-UPDATE] Neue Credentials empfangen via WebSocket!");
      LOG_WARN("[WiFi-UPDATE] Starte Restart in 2 Sekunden...");
      this->_pendingRestart = true;
      this->_restartTime = millis() + 2000;
    };
    
    // ===== STEP 6: WiFi Scan nur im AP-Modus =====
    if (_staMode) {
      LOG_INFO("[INIT] SCHRITT 6: STA-Modus aktiv -> überspringe WiFi-Scan");
      model.wifi.available_networks.get().clear();  // kein Scan notwendig
    } else {
      LOG_INFO("[INIT] SCHRITT 6: Starte asynchronen WiFi-Scan (AP-Modus)...");
      WiFi.scanNetworks(true);
    }
    
    LOG_INFO("========== WiFi Provisioner READY ==========");
    if (_staMode) {
      LOG_INFO_F("[INIT] Status: STA-Modus aktiv, erreichbar unter: %s.local", _mdnsHost.c_str());
    } else {
      LOG_INFO_F("[INIT] Status: AP-Modus aktiv, SSID: '%s'", _apSsid.c_str());
    }
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
    // ===== CHECK 1: Pending Restart =====
    if (_pendingRestart && millis() >= _restartTime) {
      LOG_WARN("[LOOP] ⚠ PENDING RESTART AKTIVIERT!");
      LOG_WARN("[LOOP] Warte auf Preferences flush...");
      delay(RESTART_DELAY_MS);
      LOG_WARN("[LOOP] Starte ESP neu...");
      ESP.restart();
    }
    
    // ===== CHECK 2: DNS Loop (nur im AP-Modus) =====
    handleDnsLoop();

    // ===== CHECK 3: OTA Handle =====
    if (millis() - lastOta >= 50) {
      lastOta = millis();
      ota.handle();
    }

    // ===== CHECK 4: WiFi Scan Results =====
    int n = WiFi.scanComplete();
    if (n >= 0) {
      LOG_DEBUG_F("[LOOP] WiFi-Scan abgeschlossen: %d Netzwerke gefunden", n);
      model.wifi.available_networks.get().clear();
      for (int i = 0; i < n && i < WifiSettings::MAX_NETWORKS; ++i) {
        StringBuffer<WifiSettings::SSID_LEN> ssid;
        ssid.set(WiFi.SSID(i).c_str());
        model.wifi.available_networks.get().add(ssid);
        LOG_TRACE_F("[LOOP] Netzwerk %d: %s (RSSI: %d dBm)", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
      WiFi.scanDelete();
      LOG_DEBUG("[LOOP] Broadcast: Netzwerkliste aktualisiert via WebSocket");
      model.broadcastAll();
    }

    // ===== CHECK 5: Heap Logging =====
    if (_heapLogger.ready()) {
      uint32_t freeHeap = ESP.getFreeHeap();
      LOG_DEBUG_F("[HEAP] Pushing heap data: %u bytes", freeHeap);
      model.admin.admin_log.get().push((float)freeHeap);
    }

    yield();
  }

private:
  // ============= CONFIGURATION CONSTANTS (für Tests anpassbar) =============
  static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // 15s Timeout für WiFi Verbindung
  static const unsigned long RESTART_DELAY_MS = 500;            // Verzögerung vor Restart
  
  // ============= MEMBER VARIABLES =============
  String _apSsid, _apPass, _mdnsHost, _fallbackFile, _infoMessage;
  bool _mdnsHostUserSet = false;
  bool _requireAdmin = true;
  bool _staMode;
  bool _littleFsAvailable = false;
  StatusCallback _onStatus = nullptr;
  uint32_t lastOta = 0;
  uint32_t lastCleanup = 0;
  bool _pendingRestart = false;
  unsigned long _restartTime = 0;
  Periodic _heapLogger = Periodic(5000);
    
  bool _connectToWiFi()
  {
    const char* ssid = model.wifi.ssid;
    const char* pass = model.wifi.pass;

    LOG_INFO_F("[STA] Verbindungsversuch zu WiFi-Netzwerk: '%s'", ssid ? ssid : "null");
    
    // Prüfe ob Credentials vorhanden sind
    if (!ssid || ssid[0] == '\0')
    {
      LOG_WARN("[STA] ✗ Keine WLAN-Zugangsdaten gespeichert!");
      if (_onStatus)
        _onStatus("Keine WLAN-Zugangsdaten gespeichert.");
      return false;
    }

    // Setze WiFi in STA-Modus
    WiFi.mode(WIFI_STA);
    LOG_DEBUG_F("[STA] WiFi-Modus: STA, SSID: '%s'", ssid);
    
    // Starte Verbindung
    WiFi.begin(ssid, pass);
    if (_onStatus)
      _onStatus(String("Verbinde mit WLAN: ") + ssid);
    
    LOG_INFO_F("[STA] WiFi.begin() aufgerufen, warte auf Verbindung...");
    
    // Warte auf Verbindung mit Timeout
    unsigned long start = millis();
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS)
    {
      yield();
      delay(100);
      attempt++;
      
      // Fortschrittsanzeige alle 2 Sekunden
      if (attempt % 20 == 0) {
        LOG_TRACE_F("[STA] Verbindungsversuch läuft... (%ld ms / %lu ms)", 
                    millis() - start, WIFI_CONNECT_TIMEOUT_MS);
      } else {
        LOG_TRACE(".");
      }
    }

    // Ergebnis prüfen
    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      LOG_INFO_F("[STA] ✓ ERFOLGREICH VERBUNDEN! IP: %s", ip.c_str());
      LOG_DEBUG_F("[STA] BSSID: %s, RSSI: %d dBm (Signalstärke)", 
                  WiFi.BSSIDstr().c_str(), WiFi.RSSI());
      
      if (_onStatus)
        _onStatus("WLAN verbunden:\n" + ip);
      
      return true;
    }
    else
    {
      LOG_WARN("[STA] ✗ VERBINDUNG FEHLGESCHLAGEN nach 15 Sekunden!");
      LOG_WARN_F("[STA] WiFi-Status: %d (Expected: 3=WL_CONNECTED)", WiFi.status());
      if (_onStatus)
        _onStatus("WLAN-Verbindung fehlgeschlagen.");
      return false;
    }
  }

  void _startMdns()
  {
    LOG_INFO("[mDNS] Starte mDNS Service...");
    if (MDNS.begin(_mdnsHost.c_str()))
    {
      MDNS.addService("arduino", "tcp", 3232);
      String msg = "Erreichbar unter:\n" + _mdnsHost + ".local";
      LOG_INFO_F("[mDNS] ✓ mDNS erfolgreich gestartet: %s.local", _mdnsHost.c_str());
      if (_onStatus)
        _onStatus(msg);
    }
    else
    {
      LOG_ERROR("[mDNS] ✗ mDNS konnte nicht gestartet werden!");
      if (_onStatus)
        _onStatus("Fehler: mDNS konnte nicht gestartet werden.");
    }
  }

  void _startAccessPoint()
  {
    LOG_WARN("[AP] Starte Access Point Modus!");
    LOG_INFO_F("[AP] SSID: '%s'", _apSsid.c_str());
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(8, 8, 8, 8), IPAddress(8, 8, 8, 8), IPAddress(255, 255, 255, 0));
    WiFi.softAP(_apSsid.c_str(), _apPass.c_str());

    dns.start(53, "*", IPAddress(8, 8, 8, 8));

    if (_onStatus)
      _onStatus("Starte Access Point: " + _apSsid);
    
    LOG_WARN_F("[AP] ✓ Access Point aktiv!");
    LOG_INFO_F("[AP] Verbinde dich mit SSID '%s' um Credentials zu übertragen", _apSsid.c_str());
    LOG_INFO("[AP] Rufe 192.168.4.1 im Browser auf");

    server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { _handleScan(request); });

    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {}, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
      _handleSave(request, data, len);
    });
  }

  void _registerRoutes()
  {
    if (_staMode)
    {
      LOG_DEBUG("[ROUTES] Registriere STA-Modus Routes (Admin Seite)");
      AdminPage::registerAdminRoutes(server, _requireAdmin, model);

      server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
                {
        DynamicJsonDocument doc(256);
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["mdns"] = String("http://") + WiFi.getHostname() + ".local/";
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); 
      });

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
        LOG_WARN("[RESET] Lösche alle WLAN-Daten und starte Neustart!");
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
        request->send(200, "text/plain", "WLAN-Daten gelöscht. Neustart...");
        delay(1000);
        ESP.restart(); 
      });

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
      LOG_DEBUG("[ROUTES] Registriere AP-Modus Routes (WiFi Setup Seite)");
      server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                { _serveFileWithFallback(request, _fallbackFile); });

      server.on("/info", HTTP_GET, [this](AsyncWebServerRequest *request)
                { request->send(200, "text/html", _infoMessage); });
    }

    server.on("/graphs", HTTP_GET, [this](AsyncWebServerRequest *request)
          { _serveFileWithFallback(request, "/graphs.html"); });

    // Attach Model WebSocket
    LOG_DEBUG("[ROUTES] Registriere Model WebSocket");
    model.attachTo(server);

    // Fallback Routes
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

    // Start HTTP Server
    LOG_INFO("[ROUTES] Starte HTTP Webserver auf Port 80");
    server.begin();
    LOG_INFO("[ROUTES] ✓ Alle Routes registriert");
  }

  // Route handlers (extracted for clarity)
  void _handleScan(AsyncWebServerRequest *request)
  {
    LOG_INFO("[SCAN] WiFi-Scan Anfrage erhalten");
    int n = WiFi.scanComplete();
    LOG_DEBUG_F("[SCAN] scanComplete() Status: %d (-2=nicht gestartet, -1=läuft noch, >=0=fertig)", n);
    
    if (n == -2) {
      LOG_INFO("[SCAN] Starte neuen Scan...");
      WiFi.scanNetworks(true);
    }
    if (n == -1) {
      LOG_DEBUG("[SCAN] Scan läuft noch... Sende leeres Array");
      request->send(200, "application/json", "[]");
      return;
    }

    LOG_INFO_F("[SCAN] ✓ Scan fertig: %d Netzwerke gefunden", n);
    
    // Update model with available networks
    model.wifi.available_networks.get().clear();
    for (int i = 0; i < n && i < WifiSettings::MAX_NETWORKS; ++i) {
      StringBuffer<WifiSettings::SSID_LEN> ssid;
      ssid.set(WiFi.SSID(i).c_str());
      model.wifi.available_networks.get().add(ssid);
      LOG_TRACE_F("[SCAN] Netzwerk %d: %s (RSSI: %d dBm)", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
    
    // Trigger WebSocket update
    LOG_DEBUG("[SCAN] Broadcast Netzwerkliste via WebSocket");
    model.broadcastAll();

    // Sende JSON Response
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
    LOG_INFO("[AP-SAVE] Neue Credentials empfangen!");
    
    DynamicJsonDocument doc(256);
    auto err = deserializeJson(doc, (const char*)data, len);
    
    if (err) {
      LOG_ERROR("[AP-SAVE] ✗ JSON Parsing Fehler!");
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
      return;
    }
    
    if (!doc.containsKey("ssid") || !doc.containsKey("pass")) {
      LOG_ERROR("[AP-SAVE] ✗ Fehlende Felder (ssid oder pass)");
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_fields\"}");
      return;
    }
    
    // Extrahiere neue Credentials
    const char* newSsid = doc["ssid"].as<const char*>();
    const char* newPass = doc["pass"].as<const char*>();
    
    LOG_WARN_F("[AP-SAVE] Neue SSID erhalten: '%s'", newSsid);
    LOG_WARN_F("[AP-SAVE] Neue Password Länge: %d", strlen(newPass));
    
    // Speichere in Model (wird automatisch in Preferences gespeichert)
    model.wifi.ssid = newSsid;
    model.wifi.pass = newPass;
    
    LOG_INFO("[AP-SAVE] Credentials in Preferences gespeichert!");
    LOG_WARN("[AP-SAVE] ⚠ Sende OK Response und starte Restart in 1 Sekunde...");
    
    // Sende Response
    request->send(200, "application/json", "{\"ok\":true}\n");
    
    // Warte und restart
    delay(RESTART_DELAY_MS);
    LOG_WARN("[AP-SAVE] >>> RESTART JETZT!");
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

  // Password generation helper (delegates to Model)
  String _generatePassword(size_t len = 12) {
    return Model::generatePassword(len);
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
