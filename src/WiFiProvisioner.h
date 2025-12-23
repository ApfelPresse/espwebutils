#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <functional>

#include "webfiles.h"
#include "NVSHelper.h"
#include "OtaUpdate.h"
#include "LiveGraphManager.h"
#include "TimeSync.h"

AsyncWebSocket graphsWs{"/ws/graphs"};
LiveGraphManager graphs{graphsWs, 20};

class WiFiProvisioner
{
public:
  using StatusCallback = std::function<void(const String &)>;

  AsyncWebServer server;
  DNSServer dns;
  OtaUpdate ota;
  TimeSync timeSync;

  WiFiProvisioner()
      : server(80),
        _apSsid("ESP-Setup"),
        _apPass(""),
        _mdnsHost("esp32"),
        _fallbackFile("/wifi/wifi.html"),
        _infoMessage("<h1>Bitte</h1> wählen Sie ein WLAN aus, um den ESP32 zu konfigurieren."),
        _staMode(false)
  {
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

  void begin()
  {
    if (!LittleFS.begin())
    {
      Serial.println("[ERROR] LittleFS konnte nicht gestartet werden!");
      if (_onStatus)
        _onStatus("Fehler: LittleFS konnte nicht gestartet werden");
      return;
    }

    if (_connectToWiFi())
    {
      _staMode = true;
      _startMdns();

      timeSync.begin("CET-1CEST,M3.5.0/2,M10.5.0/3");

      graphs.setNowProvider([this]()
                            {
          if (timeSync.isValid()) return timeSync.nowEpochSeconds();
          return (double)millis() / 1000.0; });

      ota.load();
      ota.onStatus([this](const String &s)
                   { if (_onStatus) _onStatus(s); });
      ota.setHostname(_mdnsHost);
      ota.beginIfNeeded(_mdnsHost);
      Serial.println("[OTA] Passwort: " + ota.getPassword());
    }
    else
    {
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

    if (millis() - lastCleanup >= 1000)
    {
      lastCleanup = millis();
      graphs.cleanup();
    }

    yield();
  }

private:
  String _apSsid, _apPass, _mdnsHost, _fallbackFile, _infoMessage;
  bool _staMode;
  StatusCallback _onStatus = nullptr;
  uint32_t lastOta = 0;
  uint32_t lastCleanup = 0;
    
  bool _connectToWiFi()
  {
    String ssid = readPref("wifi", "ssid");
    String pass = readPref("wifi", "pass");

    if (ssid.isEmpty())
    {
      Serial.println("[STA] Keine Zugangsdaten gespeichert");
      if (_onStatus)
        _onStatus("Keine WLAN-Zugangsdaten gespeichert.");
      return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    if (_onStatus)
      _onStatus("Verbinde mit WLAN: " + ssid);
    Serial.printf("[STA] Verbinde mit %s ...\n", ssid.c_str());
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

    server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
      int n = WiFi.scanComplete();
      if (n == -2) WiFi.scanNetworks(true);
      if (n == -1) return request->send(200, "application/json", "[]");

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
      request->send(200, "application/json", json); });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t)
              {
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, (const char*)data, len)) {
          request->send(400, "text/plain", "Invalid JSON");
          return;
        }
        writePref("wifi", "ssid", doc["ssid"].as<String>());
        writePref("wifi", "pass", doc["pass"].as<String>());
        request->send(200, "text/plain", "OK");
        delay(1000);
        ESP.restart(); });
  }

  void _registerRoutes()
  {
    if (_staMode)
    {
      server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->redirect("/wifi"); });

      server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
                { _serveFileWithFallback(request, "/wifi/admin.html"); });

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

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(LittleFS, "/index.html", "text/html"); });
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

    graphs.begin();
    server.addHandler(&graphsWs);

    server.on("/graphs", HTTP_GET, [this](AsyncWebServerRequest *request)
              { _serveFileWithFallback(request, "/wifi/graphs.html"); });

    server.begin();
  }

  void _serveFileWithFallback(AsyncWebServerRequest *request, const String &fallbackPath)
  {
    const String uri = request->url();
    const WebFile *match = nullptr;
    const WebFile *fallback = nullptr;

    for (size_t i = 0; i < webFilesCount; ++i)
    {
      if (String(webFiles[i].path) == uri)
        match = &webFiles[i];
      if (String(webFiles[i].path) == fallbackPath)
        fallback = &webFiles[i];
    }

    const WebFile *fileToSend = match ? match : fallback;
    if (fileToSend)
    {
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
};
