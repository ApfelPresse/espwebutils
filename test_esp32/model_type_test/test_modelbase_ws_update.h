#pragma once
#include "../test_helpers.h"

#include <ArduinoJson.h>

#include "../../src/model/ModelBase.h"
#include "../../src/model/ModelVar.h"
#include "../../src/model/types/ModelTypePrimitive.h"

#include <Preferences.h>

namespace ModelBaseWsUpdateTest {

class TestModelBase : public ModelBase {
public:
  using ModelBase::ModelBase;
  using ModelBase::registerTopic;
};

struct SettingsTopic {
  fj::VarWsRw<int> counter;

  typedef fj::Schema<SettingsTopic,
                     fj::Field<SettingsTopic, decltype(counter)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<SettingsTopic>(
        fj::Field<SettingsTopic, decltype(counter)>{"counter", &SettingsTopic::counter});
    return s;
  }
};

void test_ws_envelope_applies_update_without_prefs() {
  TEST_START("ModelBase WS handleIncoming applies update");

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 1;

  // persist=false avoids needing Preferences::begin() / NVS.
  model.registerTopic("settings", settings, false, false);

  const char* msg = R"({"topic":"settings","data":{"counter":42}})";
  bool ok = model.testHandleWsMessage(msg, strlen(msg));

  CUSTOM_ASSERT(ok, "WS message should apply successfully");
  CUSTOM_ASSERT(settings.counter.get() == 42, "Counter should update to 42");

  TEST_END();
}

static void clearModelNamespace_() {
  Preferences prefs;
  prefs.begin("model", false);
  prefs.clear();
  prefs.end();
}

void test_ws_envelope_persists_when_enabled() {
  TEST_START("ModelBase WS handleIncoming persists when enabled");

  clearModelNamespace_();

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 1;

  model.registerTopic("settings", settings, true, false);
  model.begin();

  const char* msg = R"({"topic":"settings","data":{"counter":77}})";
  bool ok = model.testHandleWsMessage(msg, strlen(msg));
  CUSTOM_ASSERT(ok, "WS message should apply successfully");
  CUSTOM_ASSERT(settings.counter.get() == 77, "Counter should update to 77");

  Preferences prefs;
  prefs.begin("model", true);
  String saved = prefs.getString("settings", "");
  prefs.end();

  CUSTOM_ASSERT(saved.length() > 0, "Prefs should contain 'settings' JSON");

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, saved);
  CUSTOM_ASSERT(!err, "Saved prefs JSON should parse");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 77, "Prefs should store counter.value=77");

  TEST_END();
}

void test_ws_unknown_topic_returns_false() {
  TEST_START("ModelBase WS handleIncoming unknown topic");

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 1;
  model.registerTopic("settings", settings, false, false);

  const char* msg = R"({"topic":"does_not_exist","data":{"counter":2}})";
  bool ok = model.testHandleWsMessage(msg, strlen(msg));

  CUSTOM_ASSERT(!ok, "Unknown topic should return false");
  CUSTOM_ASSERT(settings.counter.get() == 1, "Counter should remain unchanged");

  TEST_END();
}

void runAllTests() {
  SUITE_START("MODELBASE WS UPDATE");
  test_ws_envelope_applies_update_without_prefs();
  test_ws_envelope_persists_when_enabled();
  test_ws_unknown_topic_returns_false();
  SUITE_END("MODELBASE WS UPDATE");
}

} // namespace ModelBaseWsUpdateTest
