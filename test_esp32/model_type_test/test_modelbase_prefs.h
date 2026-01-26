#pragma once
#include "../test_helpers.h"

#include <Preferences.h>
#include <ArduinoJson.h>

#include "../../src/model/ModelBase.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/ModelVar.h"
#include "../../src/model/types/ModelTypePrimitive.h"

namespace ModelBasePrefsTest {

class TestModelBase : public ModelBase {
public:
  using ModelBase::ModelBase;
  using ModelBase::registerTopic;
};

struct SettingsTopic {
  fj::VarWsPrefsRw<int> counter;

  typedef fj::Schema<SettingsTopic,
                     fj::Field<SettingsTopic, decltype(counter)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<SettingsTopic>(
        fj::Field<SettingsTopic, decltype(counter)>{"counter", &SettingsTopic::counter});
    return s;
  }
};

// Topic type WITH setSaveCallback(): ModelBase should attach it, and changes should auto-persist.
struct AutoSaveTopic {
  fj::VarWsPrefsRw<int> counter;

  typedef fj::Schema<AutoSaveTopic,
                     fj::Field<AutoSaveTopic, decltype(counter)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<AutoSaveTopic>(
        fj::Field<AutoSaveTopic, decltype(counter)>{"counter", &AutoSaveTopic::counter});
    return s;
  }

  void setSaveCallback(std::function<void()> cb) {
    counter.setOnChange(cb);
  }
};

static void clearModelNamespace() {
  Preferences prefs;
  prefs.begin("model", false);
  prefs.clear();
  prefs.end();
}

void test_begin_initializes_missing_prefs_key() {
  TEST_START("ModelBase begin() initializes missing prefs");

  clearModelNamespace();

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 123;

  model.registerTopic("settings", settings, true, false);
  model.begin();

  Preferences prefs;
  prefs.begin("model", true);
  bool exists = prefs.isKey("settings");
  String saved = prefs.getString("settings", "");
  prefs.end();

  CUSTOM_ASSERT(exists, "Prefs key 'settings' should exist after begin()");
  CUSTOM_ASSERT(saved.length() > 0, "Prefs value for 'settings' should be non-empty JSON");

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, saved);
  CUSTOM_ASSERT(!err, "Saved JSON should parse");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 123, "Saved counter should match initial value");

  TEST_END();
}

void test_non_persistent_topic_is_not_saved() {
  TEST_START("ModelBase non-persistent topic not saved");

  clearModelNamespace();

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 7;

  model.registerTopic("temp", settings, false, false);
  model.begin();

  Preferences prefs;
  prefs.begin("model", true);
  bool exists = prefs.isKey("temp");
  prefs.end();

  CUSTOM_ASSERT(!exists, "Prefs key 'temp' should NOT exist for persist=false topic");

  TEST_END();
}

void test_saveTopic_unknown_returns_false() {
  TEST_START("ModelBase saveTopic(unknown) returns false");

  clearModelNamespace();

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 1;
  model.registerTopic("settings", settings, true, false);
  model.begin();

  bool ok = model.saveTopic("does_not_exist");
  CUSTOM_ASSERT(!ok, "saveTopic should return false for unknown topic");

  TEST_END();
}

void test_setSaveCallback_auto_persists_on_change() {
  TEST_START("ModelBase setSaveCallback auto-persists on change");

  clearModelNamespace();

  TestModelBase model(80, "/ws");
  AutoSaveTopic settings;
  settings.counter = 1;

  model.registerTopic("autosave", settings, true, false);
  model.begin();

  // Change the value; AutoSaveTopic routes setSaveCallback to Var::setOnChange,
  // so ModelBase should save the topic automatically.
  settings.counter = 42;

  Preferences prefs;
  prefs.begin("model", true);
  String saved = prefs.getString("autosave", "");
  prefs.end();

  CUSTOM_ASSERT(saved.length() > 0, "Prefs value for 'autosave' should exist");

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, saved);
  CUSTOM_ASSERT(!err, "Saved JSON should parse");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 42, "Auto-persisted counter should update to 42");

  TEST_END();
}

void test_without_setSaveCallback_does_not_auto_persist() {
  TEST_START("ModelBase without setSaveCallback does not auto-persist");

  clearModelNamespace();

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 10;

  model.registerTopic("manualsave", settings, true, false);
  model.begin();

  // Change value; without setSaveCallback, this should NOT auto-save.
  settings.counter = 11;

  Preferences prefs;
  prefs.begin("model", true);
  String saved = prefs.getString("manualsave", "");
  prefs.end();

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, saved);
  CUSTOM_ASSERT(!err, "Saved JSON should parse");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 10, "Counter should still be 10 until saveTopic() is called");

  bool ok = model.saveTopic("manualsave");
  CUSTOM_ASSERT(ok, "saveTopic should succeed");

  prefs.begin("model", true);
  String saved2 = prefs.getString("manualsave", "");
  prefs.end();

  doc.clear();
  err = deserializeJson(doc, saved2);
  CUSTOM_ASSERT(!err, "Saved JSON should parse after manual save");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 11, "Counter should be 11 after saveTopic()");

  TEST_END();
}

void test_corrupted_prefs_is_rewritten_with_defaults() {
  TEST_START("ModelBase rewrites corrupted prefs with defaults");

  clearModelNamespace();

  // Pre-seed corrupted JSON under the key the model will use.
  {
    Preferences prefs;
    prefs.begin("model", false);
    prefs.putString("settings", "{not valid json");
    prefs.end();
  }

  TestModelBase model(80, "/ws");
  SettingsTopic settings;
  settings.counter = 123;

  model.registerTopic("settings", settings, true, false);
  model.begin();

  // After begin(), corrupted prefs should have been replaced by valid JSON.
  Preferences prefs;
  prefs.begin("model", true);
  String saved = prefs.getString("settings", "");
  prefs.end();

  CUSTOM_ASSERT(saved.length() > 0, "Prefs value for 'settings' should exist after rewrite");

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, saved);
  CUSTOM_ASSERT(!err, "Rewritten JSON should parse");
  CUSTOM_ASSERT(doc["counter"]["value"].as<int>() == 123, "Rewritten counter should match default");

  TEST_END();
}

void runAllTests() {
  SUITE_START("MODELBASE PREFS");
  test_begin_initializes_missing_prefs_key();
  test_non_persistent_topic_is_not_saved();
  test_saveTopic_unknown_returns_false();
  test_setSaveCallback_auto_persists_on_change();
  test_without_setSaveCallback_does_not_auto_persist();
  test_corrupted_prefs_is_rewritten_with_defaults();
  SUITE_END("MODELBASE PREFS");
}

} // namespace ModelBasePrefsTest
