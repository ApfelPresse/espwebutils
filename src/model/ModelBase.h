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
  static const size_t JSON_CAPACITY = 512;

  ModelBase(uint16_t port, const char* wsPath)
  : server_(port), ws_(wsPath) {}

  void begin() {
    prefs_.begin("model", false);
    loadOrInitAll();

    ws_.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                       AwsEventType t, void* a, uint8_t* d, size_t l) {
      this->onWsEvent(s, c, t, a, d, l);
    });

    server_.addHandler(&ws_);
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(200, "text/plain", "WS ready at /ws");
    });

    server_.begin();
  }

  bool broadcastTopic(const char* topic) {
    Entry* e = find(topic);
    if (!e) return false;
    if (!e->ws_send) return true;
    ws_.textAll(makeEnvelope(*e));
    return true;
  }

  void broadcastAll() {
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
    ws_.textAll(out);
  }

  static void graphPushCbXY(const char* graph, const char* label, uint64_t x, float y, void* ctx) {
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
    StaticJsonDocument<JSON_CAPACITY> doc;
    JsonObject data = doc.to<JsonObject>();

    e.makePrefsJson(e.objPtr, data);

    String out;
    serializeJson(doc, out);
    return out;
  }

  bool saveEntry(Entry& e) {
    if (!e.persist) return true;
    return prefs_.putString(e.topic, makeDataOnlyJson(e)) > 0;
  }

  bool loadEntry(Entry& e) {
    if (!e.persist) return true;

    if (!prefs_.isKey(e.topic))
      return saveEntry(e);

    String dataJson = prefs_.getString(e.topic, "");
    if (!dataJson.length())
      return false;

    return e.applyUpdate(e.objPtr, dataJson, false);
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
    T& obj = *(T*)objPtr;

    StaticJsonDocument<JSON_CAPACITY> doc;
    if (deserializeJson(doc, dataJson))
      return false;

    JsonObject root = doc.as<JsonObject>();
    return fj::TypeAdapter<T>::read(obj, root, strict);
  }

  void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      broadcastAll();
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
    StaticJsonDocument<JSON_CAPACITY> doc;
    if (deserializeJson(doc, msg)) {
      client->text(R"({"ok":false,"error":"invalid_json"})");
      return;
    }

    const char* topic = doc["topic"];
    JsonVariant data = doc["data"];
    if (!topic || !data.is<JsonObject>()) {
      client->text(R"({"ok":false,"error":"missing_topic_or_data"})");
      return;
    }

    Entry* e = find(topic);
    if (!e) {
      client->text(R"({"ok":false,"error":"unknown_topic"})");
      return;
    }

    String dataStr;
    serializeJson(data, dataStr);

    bool ok = e->applyUpdate(e->objPtr, dataStr, false);
    if (!ok) {
      client->text(R"({"ok":false,"error":"apply_failed"})");
      return;
    }

    saveEntry(*e);
    on_update(topic);

    client->text(R"({"ok":true})");
    broadcastTopic(topic);
  }
  
};
