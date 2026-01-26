#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>

#include "model/ModelSerializer.h"
#include "model/ModelTypeTraits.h"

class ModelBase {
public:
  static const size_t JSON_CAPACITY = 2048;  // Large enough for graph data with 16+ points

  ModelBase(uint16_t port, const char* wsPath)
  : server_(port), ws_(wsPath) {}

  void begin() {
    LOG_TRACE("[Model] ModelBase::begin() - opening Preferences namespace 'model'");
    prefs_.begin("model", false);
    LOG_TRACE("[Model] Loading all topics from Preferences");
    loadOrInitAll();
    LOG_TRACE("[Model] All topics loaded, registering WebSocket handler");

    ws_.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                       AwsEventType t, void* a, uint8_t* d, size_t l) {
      this->onWsEvent(s, c, t, a, d, l);
    });
  }

  // Attach the model's WebSocket handler to an existing AsyncWebServer.
  // Call this once before server.begin().
  void attachTo(AsyncWebServer& server) {
    server.addHandler(&ws_);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(200, "text/plain", "WS ready at /ws");
    });
  }

  bool broadcastTopic(const char* topic) {
    Entry* e = find(topic);
    if (!e) return false;
    if (!e->ws_send) return true;
    String envelope = makeEnvelope(*e);
    LOG_TRACE_F("[WS] Broadcasting topic '%s' (%u bytes): %s", topic, envelope.length(), envelope.c_str());
    ws_.textAll(envelope);
    return true;
  }

  void broadcastAll() {
    LOG_TRACE_F("[WS] Broadcasting all %zu topics", entryCount_);
    for (size_t i = 0; i < entryCount_; ++i) {
      if (!entries_[i].ws_send) continue;
      ws_.textAll(makeEnvelope(entries_[i]));
    }
  }

  // Persist a topic by name (public wrapper)
  bool saveTopic(const char* topic) {
    Entry* e = find(topic);
    if (!e) return false;
    return saveEntry(*e);
  }

  void sendGraphPointXY(const char* graph, const char* label, uint64_t x, float y, bool synced) {
    LOG_DEBUG_F("[WS] Sending graph_point: graph=%s, label=%s, x=%llu, y=%.2f", graph, label, x, y);
    StaticJsonDocument<256> doc;
    doc["topic"] = "graph_point";
    JsonObject d = doc.createNestedObject("data");
    d["graph"] = graph;
    d["label"] = label;
    d["x"] = (uint64_t)x;
    d["y"] = y;
    d["synced"] = synced;

    String out;
    serializeJson(doc, out);
    LOG_TRACE_F("[WS] Graph point JSON: %s", out.c_str());
    ws_.textAll(out);
  }

  static void graphPushCbXY(const char* graph, const char* label, uint64_t x, float y, void* ctx) {
    LOG_TRACE_F("[CALLBACK] graphPushCbXY called: graph=%s, label=%s, x=%llu, y=%.2f, ctx=%p", 
                graph, label, x, y, ctx);
    if (!ctx) return;
    ModelBase* self = (ModelBase*)ctx;
    self->sendGraphPointXY(graph, label, x, y, true);
  }

protected:
  virtual void on_update(const char* topic) { (void)topic; }

  void sendGraphPoint(const char* graph, const char* label, int value) {
    StaticJsonDocument<256> doc;
    doc["topic"] = "graph_point";
    JsonObject d = doc.createNestedObject("data");
    d["graph"] = graph;
    d["label"] = label;
    d["value"] = value;

    String out;
    serializeJson(doc, out);
    ws_.textAll(out);
  }

  static void graphPushCb(const char* graph, const char* label, int value, void* ctx) {
    if (!ctx) return;
    ((ModelBase*)ctx)->sendGraphPoint(graph, label, value);
  }

  template <typename T>
  void registerTopic(const char* topic, T& obj) {
    addEntry<T>(topic, obj,
                fj::TypeAdapter<T>::defaultPersist(),
                fj::TypeAdapter<T>::defaultWsSend());
  }

  template <typename T>
  void registerTopic(const char* topic, T& obj, bool persist, bool wsSend) {
    addEntry<T>(topic, obj, persist, wsSend);
  }

private:
  struct Entry {
    const char* topic;
    void* objPtr;
    bool persist;
    bool ws_send;

    // WS vs Prefs writers
    void (*makeWsJson)(void* objPtr, JsonObject out);
    void (*makePrefsJson)(void* objPtr, JsonObject out);

    bool (*applyUpdate)(void* objPtr, const String& dataJson, bool strict);
  };

  static const size_t MAX_TOPICS = 16;
  Entry entries_[MAX_TOPICS];
  size_t entryCount_ = 0;

  AsyncWebServer server_;
  AsyncWebSocket ws_;
  Preferences prefs_;

  template <typename T>
  void addEntry(const char* topic, T& obj, bool persist, bool wsSend) {
    if (entryCount_ >= MAX_TOPICS) return;

    Entry& e = entries_[entryCount_++];
    e.topic = topic;
    e.objPtr = (void*)&obj;
    e.persist = persist;
    e.ws_send = wsSend;

    e.makeWsJson = &makeWsJsonImpl<T>;
    e.makePrefsJson = &makePrefsJsonImpl<T>;
    e.applyUpdate = &applyUpdateImpl<T>;
    // If the topic type exposes setSaveCallback(std::function<void()>), hook it to persist this entry on changes.
    {
      Entry* ep = &e;
      if (has_setSaveCallback<T>::value) {
        ((T*)&obj)->setSaveCallback([this, ep]() { this->saveEntry(*ep); });
      }
    }
  }

  Entry* find(const char* topic) {
    for (size_t i = 0; i < entryCount_; ++i) {
      if (strcmp(entries_[i].topic, topic) == 0) return &entries_[i];
    }
    return nullptr;
  }

  // SFINAE detector: does T::setSaveCallback(std::function<void()>) exist?
  template <typename U>
  struct has_setSaveCallback {
    template <typename V>
    static auto test(int) -> decltype(std::declval<V>().setSaveCallback(std::declval<std::function<void()>>() ), std::true_type());
    template <typename>
    static std::false_type test(...);
    using type = decltype(test<U>(0));
    static const bool value = type::value;
  };

  String makeEnvelope(Entry& e) {
    StaticJsonDocument<JSON_CAPACITY> doc;
    doc["topic"] = e.topic;

    JsonObject data = doc.createNestedObject("data");
    e.makeWsJson(e.objPtr, data);

    String out;
    serializeJson(doc, out);
    return out;
  }

  String makeDataOnlyJson(Entry& e) {
    LOG_TRACE_F("[ModelBase] makeDataOnlyJson starting for topic '%s'", e.topic);
    StaticJsonDocument<JSON_CAPACITY> doc;
    JsonObject data = doc.to<JsonObject>();

    LOG_TRACE_F("[ModelBase] Calling makePrefsJson for topic '%s'", e.topic);
    e.makePrefsJson(e.objPtr, data);
    LOG_TRACE_F("[ModelBase] makePrefsJson completed");

    String out;
    serializeJson(doc, out);
    LOG_TRACE_F("[ModelBase] makeDataOnlyJson result: %s", out.c_str());
    return out;
  }

  bool saveEntry(Entry& e) {
    if (!e.persist) {
      LOG_TRACE_F("[Prefs] Topic '%s' not persisted (persist=false)", e.topic);
      return true;
    }
    LOG_TRACE_F("[Prefs] saveEntry starting for topic '%s'", e.topic);
    String dataJson = makeDataOnlyJson(e);
    LOG_INFO_F("[Prefs] Saving topic '%s': %s", e.topic, dataJson.c_str());
    size_t written = prefs_.putString(e.topic, dataJson);
    LOG_INFO_F("[Prefs] Written %u bytes for topic '%s'", written, e.topic);
    if (written == 0) {
      LOG_WARN_F("[Prefs] FAILED to write topic '%s' - putString returned 0", e.topic);
    }
    LOG_TRACE_F("[Prefs] saveEntry completed for topic '%s', success=%s", e.topic, written > 0 ? "true" : "false");
    return written > 0;
  }

  bool loadEntry(Entry& e) {
    if (!e.persist) {
      LOG_TRACE_F("[Prefs] Topic '%s' not persisted (persist=false)", e.topic);
      return true;
    }

    if (!prefs_.isKey(e.topic)) {
      LOG_TRACE_F("[Prefs] Topic '%s' not found in Preferences, initializing", e.topic);
      return saveEntry(e);
    }

    String dataJson = prefs_.getString(e.topic, "");
    if (!dataJson.length()) {
      LOG_TRACE_F("[Prefs] Topic '%s' exists but empty", e.topic);
      return false;
    }

    LOG_TRACE_F("[Prefs] Loading topic '%s': %s", e.topic, dataJson.c_str());
    LOG_TRACE_F("[ModelBase] About to call e.applyUpdate for topic '%s'", e.topic);
    bool result = e.applyUpdate(e.objPtr, dataJson, false);
    LOG_TRACE_F("[ModelBase] applyUpdate completed for topic '%s', result=%s", e.topic, result ? "true" : "false");
    return result;
  }

  void loadOrInitAll() {
    for (size_t i = 0; i < entryCount_; ++i) {
      (void)loadEntry(entries_[i]);
    }
  }

  // -------- Writers: now use traits dispatch --------

  template <typename T>
  static void makeWsJsonImpl(void* objPtr, JsonObject out) {
    T& obj = *(T*)objPtr;
    fj::write_ws(obj, out);
  }

  template <typename T>
  static void makePrefsJsonImpl(void* objPtr, JsonObject out) {
    T& obj = *(T*)objPtr;
    fj::write_prefs(obj, out);
  }

  template <typename T>
  static bool applyUpdateImpl(void* objPtr, const String& dataJson, bool strict) {
    LOG_TRACE_F("[ModelBase::applyUpdateImpl] Parsing JSON: %s", dataJson.c_str());
    T& obj = *(T*)objPtr;

    StaticJsonDocument<JSON_CAPACITY> doc;
    if (deserializeJson(doc, dataJson)) {
      LOG_WARN_F("[ModelBase::applyUpdateImpl] JSON parse failed");
      return false;
    }

    LOG_TRACE_F("[ModelBase::applyUpdateImpl] JSON parsed successfully, calling TypeAdapter<T>::read");
    JsonObject root = doc.as<JsonObject>();
    bool result = fj::TypeAdapter<T>::read(obj, root, strict);
    LOG_TRACE_F("[ModelBase::applyUpdateImpl] TypeAdapter<T>::read returned: %s", result ? "true" : "false");
    return result;
  }

  void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
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
    for (size_t i = 0; i < len; i++)
      msg += (char)data[i];

    handleIncoming(client, msg);
  }

  void handleIncoming(AsyncWebSocketClient* client, const String& msg) {
    LOG_DEBUG_F("[WS] Incoming message: %.100s", msg.c_str());
    
    StaticJsonDocument<JSON_CAPACITY> doc;
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

  // Handle button trigger requests (override in derived classes)
  virtual void handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button) {
    LOG_WARN_F("[WS] Button trigger not implemented: topic=%s, button=%s", topic, button);
    client->text(R"({"ok":false,"error":"button_trigger_not_implemented"})");
  }
  
};
