#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "webfiles.h"
#include "Model.h"

namespace AdminPage {

static const WebFile* _findWebFile(const String &path)
{
  for (size_t i = 0; i < webFilesCount; ++i) {
    if (String(webFiles[i].path) == path) return &webFiles[i];
  }
  return nullptr;
}

static void _serveEmbeddedFile(AsyncWebServerRequest *request, const String &path)
{
  const WebFile *wf = _findWebFile(path);
  if (!wf) { request->send(404, "text/plain", "Not Found"); return; }

  String contentType = "text/html";
  if (path.endsWith(".js"))  contentType = "application/javascript";
  if (path.endsWith(".css")) contentType = "text/css";
  if (path.endsWith(".json")) contentType = "application/json";
  if (path.endsWith(".svg")) contentType = "image/svg+xml";
  if (path.endsWith(".png")) contentType = "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";

  AsyncWebServerResponse *resp = request->beginResponse(200, contentType, wf->data, wf->size);
  resp->addHeader("Content-Encoding", "gzip");
  request->send(resp);
}

static String _generatePassword(size_t len)
{
  static const char *chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
  String out;
  out.reserve(len);
  size_t n = strlen(chars);

  for (size_t i = 0; i < len; ++i) {
    uint32_t r = esp_random();
    out += chars[r % n];
  }
  return out;
}

static void _sendBasicAuthChallenge(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *resp = request->beginResponse(401, "text/plain", "Authentication required");
  resp->addHeader("WWW-Authenticate", "Basic realm=\"Admin\"");
  request->send(resp);
}

static String _b64DecodeBasic(const String &in)
{
  static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  out.reserve(in.length());

  int val = 0, valb = -8;
  for (size_t i = 0; i < in.length(); ++i) {
    unsigned char c = (unsigned char)in[i];
    if (c == '=') break;
    const char *p = strchr(b64, (char)c);
    if (!p) continue;
    val = (val << 6) + (int)(p - b64);
    valb += 6;
    if (valb >= 0) {
      out += char((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
  return out;
}

static bool _isAdminAuthorized(AsyncWebServerRequest *request, bool requireAdmin, Model &model)
{
  if (!requireAdmin) return true;

  const char* stored = model.admin.pass;
  if (!stored || stored[0] == '\0') return false;

  if (!request->hasHeader("Authorization")) return false;

  const AsyncWebHeader* ah = request->getHeader("Authorization");
  if (!ah) return false;

  String v = ah->value();
  if (!v.startsWith("Basic ")) return false;

  String payload = v.substring(6);
  payload.trim();

  String creds = _b64DecodeBasic(payload);
  int idx = creds.indexOf(':');
  if (idx <= 0) return false;

  String user = creds.substring(0, idx);
  String pass = creds.substring(idx + 1);

  return (user == "admin" && pass == stored);
}

static bool _requireAdminOrChallenge(AsyncWebServerRequest *request, bool requireAdmin, Model &model)
{
  if (_isAdminAuthorized(request, requireAdmin, model)) return true;
  _sendBasicAuthChallenge(request);
  return false;
}

static void registerAdminRoutes(AsyncWebServer &server, bool requireAdmin, Model &model)
{
  {
      const char* pw = model.admin.pass;
      if (!pw || pw[0] == '\0') {
        String gen = _generatePassword(12);
        model.admin.pass = gen.c_str();
        Serial.println("[ADMIN] Generated password: " + gen);
      }
  }

  static const char *default_ui_config = R"json({
    "buttons": [
      { "id": "reset", "label": "Zurücksetzen", "method": "POST", "path": "/reset", "confirm": true },
      { "id": "ota_extend", "label": "OTA +10min", "method": "POST", "path": "/ota/extend", "confirm": false }
    ]
  })json";

  server.on("/admin/ui-config", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    const char* cfg = model.admin_ui.config;
    if (!cfg || cfg[0] == '\0') {
      request->send(200, "application/json", default_ui_config);
    } else {
      request->send(200, "application/json", cfg);
    }
  });

  server.on(
    "/admin/ui-config",
    HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [requireAdmin, &model](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
      if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

      String s((const char*)data, len);
      s.trim();
      if (s.length() == 0 || (s[0] != '{' && s[0] != '[')) {
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
        return;
      }
        model.admin_ui.config = s.c_str();
      request->send(200, "application/json", "{\"ok\":true}");
    }
  );

  server.on("/admin", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;
    request->redirect("/wifi");
  });

  server.on("/wifi", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    if (LittleFS.exists("/admin.html")) {
      request->send(LittleFS, "/admin.html", "text/html");
    } else {
      _serveEmbeddedFile(request, "/admin.html");
    }
  });

  server.on("/admin.js", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    if (LittleFS.exists("/admin.js")) request->send(LittleFS, "/admin.js", "application/javascript");
    else _serveEmbeddedFile(request, "/admin.js");
  });

  server.on("/admin.css", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    if (LittleFS.exists("/admin.css")) request->send(LittleFS, "/admin.css", "text/css");
    else _serveEmbeddedFile(request, "/admin.css");
  });

  server.on("/admin/password", HTTP_GET, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    DynamicJsonDocument doc(128);
    doc["username"] = "admin";
    doc["password"] = (const char*)model.admin.pass;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/admin/password/regenerate", HTTP_POST, [requireAdmin, &model](AsyncWebServerRequest *request) {
    if (!_requireAdminOrChallenge(request, requireAdmin, model)) return;

    String newPw = _generatePassword(12);
      model.admin.pass = newPw.c_str();

    DynamicJsonDocument doc(128);
    doc["username"] = "admin";
    doc["password"] = newPw;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);

    Serial.println("[ADMIN] Regenerated password: " + newPw);
  });
}

}