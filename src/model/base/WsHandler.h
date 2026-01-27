#pragma once

// Included by src/model/ModelBase.h

#ifndef MODEL_HEAP_DIAG
#define MODEL_HEAP_DIAG 0
#endif

#if MODEL_HEAP_DIAG && defined(ESP32)
#include <esp_heap_caps.h>
static inline void modelHeapDiag_(const char* tag) {
  const uint32_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const uint32_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  LOG_TRACE_F("[ModelHeap] %s free8=%u largest8=%u", tag ? tag : "(null)", free8, largest8);
}
#else
static inline void modelHeapDiag_(const char*) {}
#endif

inline ModelBase::ModelBase(uint16_t port, const char* wsPath)
  : ModelBase(port, wsPath, "model") {}

inline ModelBase::ModelBase(uint16_t port, const char* wsPath, const char* prefsNamespace)
  : wsPath_(wsPath ? wsPath : "/ws"),
    prefsNamespace_(prefsNamespace ? prefsNamespace : "model"),
    server_(port),
  ws_(wsPath_) {}

inline void ModelBase::begin() {
  LOG_TRACE_F("[Model] ModelBase::begin() - opening Preferences namespace '%s'", prefsNamespace_ ? prefsNamespace_ : "(null)");
  prefs_.begin(prefsNamespace_ ? prefsNamespace_ : "model", false);
  LOG_TRACE("[Model] Loading all topics from Preferences");
  suppressAutoSideEffects_ = true;
  loadOrInitAll();
  suppressAutoSideEffects_ = false;
  LOG_TRACE("[Model] All topics loaded, registering WebSocket handler");

  ws_.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType t, void* a, uint8_t* d, size_t l) {
    this->onWsEvent(s, c, t, a, d, l);
  });
}

inline void ModelBase::attachTo(AsyncWebServer& server) {
  attachTo(server, true);
}

inline void ModelBase::attachTo(AsyncWebServer& server, bool addRootRoute) {
  server.addHandler(&ws_);
  if (!addRootRoute) return;

  const char* path = wsPath_ ? wsPath_ : "/ws";
  server.on("/", HTTP_GET, [path](AsyncWebServerRequest* req) {
    String msg = "WS ready at ";
    msg += path;
    req->send(200, "text/plain", msg);
  });
}

inline bool ModelBase::broadcastTopic(const char* topic) {
  Entry* e = find(topic);
  if (!e) return false;
  if (!e->ws_send) return true;
  String envelope = makeEnvelope(*e);
  LOG_TRACE_F("[WS] Broadcasting topic '%s' (%u bytes): %s", topic, envelope.length(), envelope.c_str());
  ws_.textAll(envelope);
  return true;
}

inline void ModelBase::broadcastAll() {
  LOG_TRACE_F("[WS] Broadcasting all %zu topics", entryCount_);
  for (size_t i = 0; i < entryCount_; ++i) {
    if (!entries_[i].ws_send) continue;
    ws_.textAll(makeEnvelope(entries_[i]));
  }
}

inline void ModelBase::onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
                                size_t len) {
  if (type == WS_EVT_CONNECT) {
    LOG_TRACE_F("[WS] Client connected (id=%u), sending initial state", client->id());
    broadcastAll();
    return;
  }
  if (type == WS_EVT_DISCONNECT) {
    LOG_TRACE_F("[WS] Client disconnected (id=%u)", client->id());
    return;
  }
  if (type != WS_EVT_DATA) return;

  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len) return;
  if (info->opcode != WS_TEXT) return;

  (void)handleIncoming(client, (const char*)data, len);
}

inline bool ModelBase::handleIncoming(AsyncWebSocketClient* client, const char* msg, size_t len) {
  if (!msg || len == 0) {
    LOG_WARN("[WS] Incoming message is empty");
    if (client) client->text(R"({"ok":false,"error":"empty_message"})");
    return false;
  }

  modelHeapDiag_("ws_before_parse");

  char preview[101];
  size_t n = len > 100 ? 100 : len;
  memcpy(preview, msg, n);
  preview[n] = '\0';
  LOG_DEBUG_F("[WS] Incoming message (%u bytes): %.100s", (unsigned)len, preview);

  static StaticJsonDocument<JSON_CAPACITY> doc;  // reuse to avoid stack bloat
  doc.clear();
  if (deserializeJson(doc, msg, len)) {
    LOG_WARN("[WS] JSON deserialize failed");
    if (client) client->text(R"({"ok":false,"error":"invalid_json"})");
    return false;
  }

  modelHeapDiag_("ws_after_parse");

  // Check if this is a button trigger request
  const char* action = doc["action"];
  if (action && strcmp(action, "button_trigger") == 0) {
    const char* topic = doc["topic"];
    const char* button = doc["button"];
    if (!topic || !button) {
      LOG_WARN("[WS] button_trigger: missing topic or button field");
      if (client) client->text(R"({"ok":false,"error":"missing_topic_or_button"})");
      return false;
    }
    LOG_INFO_F("[WS] Button trigger request: topic=%s, button=%s", topic, button);
    handleButtonTrigger(client, topic, button);
    return true;
  }

  const char* topic = doc["topic"];
  JsonVariant data = doc["data"];
  LOG_DEBUG_F("[WS] Parsed topic: %s", topic ? topic : "null");

  if (!topic || !data.is<JsonObject>()) {
    LOG_WARN("[WS] Missing topic or data is not object");
    if (client) client->text(R"({"ok":false,"error":"missing_topic_or_data"})");
    return false;
  }

  Entry* e = find(topic);
  if (!e) {
    LOG_WARN_F("[WS] Unknown topic: %s", topic);
    if (client) client->text(R"({"ok":false,"error":"unknown_topic"})");
    return false;
  }

  LOG_INFO_F("[WS] Applying update for topic: %s", topic);
  if (Logger::shouldLog(LogLevel::TRACE)) {
    String dataStr;
    dataStr.reserve(measureJson(data) + 1);
    serializeJson(data, dataStr);
    LOG_TRACE_F("[WS] Data from WebSocket: %s", dataStr.c_str());
  }

  suppressAutoSideEffects_ = true;
  bool ok = e->applyUpdateJson(e->objPtr, data.as<JsonObject>(), false);
  suppressAutoSideEffects_ = false;
  if (!ok) {
    LOG_WARN_F("[WS] applyUpdate failed for topic: %s", topic);
    if (client) client->text(R"({"ok":false,"error":"apply_failed"})");
    return false;
  }

  modelHeapDiag_("ws_after_apply");

  LOG_INFO_F("[WS] Update successful, saving topic: %s", topic);
  LOG_TRACE_F("[WS] Persisting changes to Preferences for: %s", topic);
  saveEntry(*e);
  LOG_TRACE("[WS] Preferences saved, calling on_update callback");
  on_update(topic);

  modelHeapDiag_("ws_after_save");

  LOG_TRACE("[WS] Sending confirmation back to client");
  if (client) client->text(R"({"ok":true})");
  broadcastTopic(topic);

  return true;
}

inline void ModelBase::handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button) {
  LOG_WARN_F("[WS] Button trigger not implemented: topic=%s, button=%s", topic, button);
  if (client) client->text(R"({"ok":false,"error":"button_trigger_not_implemented"})");
}

#ifdef TEST_BUILD
inline bool ModelBase::testHandleWsMessage(const char* msg, size_t len) {
  return handleIncoming(nullptr, msg, len);
}
#endif
