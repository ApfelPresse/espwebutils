#pragma once

// Included by src/model/ModelBase.h

inline ModelBase::ModelBase(uint16_t port, const char* wsPath)
    : server_(port), ws_(wsPath) {}

inline void ModelBase::begin() {
  LOG_TRACE("[Model] ModelBase::begin() - opening Preferences namespace 'model'");
  prefs_.begin("model", false);
  LOG_TRACE("[Model] Loading all topics from Preferences");
  loadOrInitAll();
  LOG_TRACE("[Model] All topics loaded, registering WebSocket handler");

  ws_.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType t, void* a, uint8_t* d, size_t l) {
    this->onWsEvent(s, c, t, a, d, l);
  });
}

inline void ModelBase::attachTo(AsyncWebServer& server) {
  server.addHandler(&ws_);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "WS ready at /ws"); });
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

  String msg;
  msg.reserve(len + 1);
  for (size_t i = 0; i < len; i++) msg += (char)data[i];

  handleIncoming(client, msg);
}

inline void ModelBase::handleIncoming(AsyncWebSocketClient* client, const String& msg) {
  LOG_DEBUG_F("[WS] Incoming message: %.100s", msg.c_str());

  static StaticJsonDocument<JSON_CAPACITY> doc;  // reuse to avoid stack bloat
  doc.clear();
  if (deserializeJson(doc, msg)) {
    LOG_WARN("[WS] JSON deserialize failed");
    client->text(R"({"ok":false,"error":"invalid_json"})");
    return;
  }

  // Check if this is a button trigger request
  const char* action = doc["action"];
  if (action && strcmp(action, "button_trigger") == 0) {
    const char* topic = doc["topic"];
    const char* button = doc["button"];
    if (!topic || !button) {
      LOG_WARN("[WS] button_trigger: missing topic or button field");
      client->text(R"({"ok":false,"error":"missing_topic_or_button"})");
      return;
    }
    LOG_INFO_F("[WS] Button trigger request: topic=%s, button=%s", topic, button);
    handleButtonTrigger(client, topic, button);
    return;
  }

  const char* topic = doc["topic"];
  JsonVariant data = doc["data"];
  LOG_DEBUG_F("[WS] Parsed topic: %s", topic ? topic : "null");

  if (!topic || !data.is<JsonObject>()) {
    LOG_WARN("[WS] Missing topic or data is not object");
    client->text(R"({"ok":false,"error":"missing_topic_or_data"})");
    return;
  }

  Entry* e = find(topic);
  if (!e) {
    LOG_WARN_F("[WS] Unknown topic: %s", topic);
    client->text(R"({"ok":false,"error":"unknown_topic"})");
    return;
  }

  LOG_INFO_F("[WS] Applying update for topic: %s", topic);
  String dataStr;
  serializeJson(data, dataStr);
  LOG_TRACE_F("[WS] Data from WebSocket: %s", dataStr.c_str());

  bool ok = e->applyUpdate(e->objPtr, dataStr, false);
  if (!ok) {
    LOG_WARN_F("[WS] applyUpdate failed for topic: %s", topic);
    client->text(R"({"ok":false,"error":"apply_failed"})");
    return;
  }

  LOG_INFO_F("[WS] Update successful, saving topic: %s", topic);
  LOG_TRACE_F("[WS] Persisting changes to Preferences for: %s", topic);
  saveEntry(*e);
  LOG_TRACE("[WS] Preferences saved, calling on_update callback");
  on_update(topic);

  LOG_TRACE("[WS] Sending confirmation back to client");
  client->text(R"({"ok":true})");
  broadcastTopic(topic);
}

inline void ModelBase::handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button) {
  LOG_WARN_F("[WS] Button trigger not implemented: topic=%s, button=%s", topic, button);
  client->text(R"({"ok":false,"error":"button_trigger_not_implemented"})");
}
