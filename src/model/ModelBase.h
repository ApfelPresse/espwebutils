#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>
#include <type_traits>
#include <functional>

#include "model/ModelSerializer.h"
#include "model/types/ModelTypeTraits.h"

// Define MODEL_JSON_CAPACITY before including this header to customize the WebSocket JSON buffer size.
// Default: 2048 bytes (sufficient for graph data with 16+ points)
// Example: #define MODEL_JSON_CAPACITY 4096
#ifndef MODEL_JSON_CAPACITY
#define MODEL_JSON_CAPACITY 2048
#endif

class ModelBase {
public:
  static const size_t JSON_CAPACITY = MODEL_JSON_CAPACITY;  // Large enough for graph data with 16+ points (configurable via MODEL_JSON_CAPACITY)

  ModelBase(uint16_t port, const char* wsPath);

  // Preferences namespace defaults to "model" if not specified.
  ModelBase(uint16_t port, const char* wsPath, const char* prefsNamespace);

  void begin();

  // Attach the model's WebSocket handler to an existing AsyncWebServer.
  // Call this once before server.begin().
  void attachTo(AsyncWebServer& server);

  // If addRootRoute is true, also installs a simple GET "/" probe route.
  void attachTo(AsyncWebServer& server, bool addRootRoute);

  const char* wsPath() const { return wsPath_; }

  bool broadcastTopic(const char* topic);

  void broadcastAll();

  // Persist a topic by name (public wrapper)
  bool saveTopic(const char* topic);

  void sendGraphPointXY(const char* graph, const char* label, uint64_t x, float y, bool synced);

  static void graphPushCbXY(const char* graph, const char* label, uint64_t x, float y, void* ctx);

#ifdef TEST_BUILD
  // Test hook: allows exercising the WS message parsing/update path without needing a real AsyncWebSocketClient.
  // Returns true if the message was parsed and applied successfully.
  bool testHandleWsMessage(const char* msg, size_t len);
#endif

protected:
  virtual void on_update(const char* topic) { (void)topic; }

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

    // WS update path: apply already-parsed data object (avoids serializeJson + deserializeJson churn)
    bool (*applyUpdateJson)(void* objPtr, JsonObject data, bool strict);
  };

  static const size_t MAX_TOPICS = 16;
  Entry entries_[MAX_TOPICS];
  size_t entryCount_ = 0;

  const char* wsPath_ = "/ws";
  const char* prefsNamespace_ = "model";
  bool suppressAutoSideEffects_ = false;
  AsyncWebServer server_;
  AsyncWebSocket ws_;
  Preferences prefs_;

  template <typename T>
  void addEntry(const char* topic, T& obj, bool persist, bool wsSend);

  Entry* find(const char* topic);

  // SFINAE detector: does T::setSaveCallback(std::function<void()>) exist?
  template <typename U>
  struct has_setSaveCallback;

  template <typename T>
  typename std::enable_if<has_setSaveCallback<T>::value, void>::type maybeAttachSaveCallback(T& obj, Entry* ep);

  template <typename T>
  typename std::enable_if<!has_setSaveCallback<T>::value, void>::type maybeAttachSaveCallback(T&, Entry*);

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

  template <typename T>
  static bool applyUpdateJsonImpl(void* objPtr, JsonObject data, bool strict);

  void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  bool handleIncoming(AsyncWebSocketClient* client, const char* msg, size_t len);

  // Handle button trigger requests (override in derived classes)
  virtual void handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button);
  
};

// Implementation split into focused headers for readability.
// Keep this file as the stable public entrypoint.

#include "base/GraphWs.h"
#include "base/Topics.h"
#include "base/Envelope.h"
#include "base/PrefsStore.h"
#include "base/TopicWriters.h"
#include "base/WsHandler.h"
