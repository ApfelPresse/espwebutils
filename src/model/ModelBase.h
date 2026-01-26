#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>

#include "model/ModelSerializer.h"
#include "model/types/ModelTypeTraits.h"

class ModelBase {
public:
  static const size_t JSON_CAPACITY = 2048;  // Large enough for graph data with 16+ points

  ModelBase(uint16_t port, const char* wsPath);

  void begin();

  // Attach the model's WebSocket handler to an existing AsyncWebServer.
  // Call this once before server.begin().
  void attachTo(AsyncWebServer& server);

  bool broadcastTopic(const char* topic);

  void broadcastAll();

  // Persist a topic by name (public wrapper)
  bool saveTopic(const char* topic);

  void sendGraphPointXY(const char* graph, const char* label, uint64_t x, float y, bool synced);

  static void graphPushCbXY(const char* graph, const char* label, uint64_t x, float y, void* ctx);

protected:
  virtual void on_update(const char* topic) { (void)topic; }

  void sendGraphPoint(const char* graph, const char* label, int value);

  static void graphPushCb(const char* graph, const char* label, int value, void* ctx);

  template <typename T>
  void registerTopic(const char* topic, T& obj);

  template <typename T>
  void registerTopic(const char* topic, T& obj, bool persist, bool wsSend);

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
  void addEntry(const char* topic, T& obj, bool persist, bool wsSend);

  Entry* find(const char* topic);

  // SFINAE detector: does T::setSaveCallback(std::function<void()>) exist?
  template <typename U>
  struct has_setSaveCallback;

  String makeEnvelope(Entry& e);
  String makeDataOnlyJson(Entry& e);

  bool saveEntry(Entry& e);
  bool loadEntry(Entry& e);
  void loadOrInitAll();

  // -------- Writers: now use traits dispatch --------

  template <typename T>
  static void makeWsJsonImpl(void* objPtr, JsonObject out);

  template <typename T>
  static void makePrefsJsonImpl(void* objPtr, JsonObject out);

  template <typename T>
  static bool applyUpdateImpl(void* objPtr, const String& dataJson, bool strict);

  void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleIncoming(AsyncWebSocketClient* client, const String& msg);

  // Handle button trigger requests (override in derived classes)
  virtual void handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button);
  
};

// Implementation split into focused headers for readability.
// Keep this file as the stable public entrypoint.

#include "base/Graphs.h"
#include "base/Topics.h"
#include "base/Envelope.h"
#include "base/PrefsStore.h"
#include "base/TopicWriters.h"
#include "base/WsHandler.h"
