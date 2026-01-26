#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../../src/model/ModelVar.h"
#include <Preferences.h>

namespace ModelTypeTest {

// Test Model Definition
struct TestSettings {
  static const int NAME_LEN = 32;
  static const int PASS_LEN = 64;

  fj::VarWsPrefsRw<StringBuffer<NAME_LEN>> name;
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> password;

  typedef fj::Schema<TestSettings,
                     fj::Field<TestSettings, decltype(name)>,
                     fj::Field<TestSettings, decltype(password)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<TestSettings>(
        fj::Field<TestSettings, decltype(name)>{"name", &TestSettings::name},
        fj::Field<TestSettings, decltype(password)>{"password", &TestSettings::password});
    return s;
  }
};

void testStaticStringPersistence() {
  const char* TEST_NS = "test_model";
  
  TEST_START("StaticString Persistence");
  
  Preferences prefs;
  prefs.begin(TEST_NS, false);
  prefs.clear();  // Start clean
  prefs.end();

  TestSettings settings;
  
  // Set values
  settings.name = "TestUser";
  settings.password = "SecretPass123";
  
  // Verify values are set
  CUSTOM_ASSERT(strcmp(settings.name.get().c_str(), "TestUser") == 0, "Name should be 'TestUser'");
  CUSTOM_ASSERT(strcmp(settings.password.get().c_str(), "SecretPass123") == 0, "Password should be 'SecretPass123'");
  
  // Serialize to JSON
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  TEST_DEBUG(String("Serialized: ") + json);
  
  // Save to preferences
  prefs.begin(TEST_NS, false);
  prefs.putString("settings", json);
  prefs.end();
  
  // Create new instance and load
  TestSettings loadedSettings;
  prefs.begin(TEST_NS, true);
  String loadedJson = prefs.getString("settings", "");
  prefs.end();
  
  CUSTOM_ASSERT(loadedJson.length() > 0, "Loaded JSON should not be empty");
  
  // Deserialize
  StaticJsonDocument<512> loadedDoc;
  DeserializationError err = deserializeJson(loadedDoc, loadedJson);
  if (err) {
    LOG_WARN_F("JSON parse error: %s", err.c_str());
  }
  JsonObject loadedRoot = loadedDoc.as<JsonObject>();
  
  TEST_TRACE_F("JSON to deserialize: %s", loadedJson.c_str());
  TEST_TRACE_F("JSON has 'name': %s", loadedRoot.containsKey("name") ? "yes" : "no");
  TEST_TRACE_F("JSON has 'password': %s", loadedRoot.containsKey("password") ? "yes" : "no");
  if (loadedRoot.containsKey("name")) {
    TEST_TRACE_F("name value in JSON: '%s'", loadedRoot["name"].as<const char*>());
  }
  if (loadedRoot.containsKey("password")) {
    TEST_TRACE_F("password value in JSON: '%s'", loadedRoot["password"].as<const char*>());
  }
  
  TEST_TRACE_F("Before readFieldsTolerant: name='%s'", loadedSettings.name.get().c_str());
  TEST_TRACE_F("Before readFieldsTolerant: password='%s'", loadedSettings.password.get().c_str());
  
  bool readSuccess = fj::readFieldsTolerant(loadedSettings, TestSettings::schema(), loadedRoot);
  CUSTOM_ASSERT(readSuccess, "Reading should succeed");
  
  TEST_TRACE_F("After readFieldsTolerant: name='%s'", loadedSettings.name.get().c_str());
  TEST_TRACE_F("After readFieldsTolerant: password='%s'", loadedSettings.password.get().c_str());
  
  // Verify loaded values
  CUSTOM_ASSERT(strcmp(loadedSettings.name.get().c_str(), "TestUser") == 0, "Loaded name should match");
  CUSTOM_ASSERT(strcmp(loadedSettings.password.get().c_str(), "SecretPass123") == 0, "Loaded password should match");
  
  // Cleanup
  prefs.begin(TEST_NS, false);
  prefs.clear();
  prefs.end();
  
  TEST_END();
}

void testVarImplicitConversion() {
  TEST_START("Var Implicit Conversion");
  
  TestSettings settings;
  settings.name = "Alice";
  
  // Test implicit conversion to const char*
  const char* namePtr = settings.name;
  CUSTOM_ASSERT(strcmp(namePtr, "Alice") == 0, "Implicit conversion should work");
  
  // Test c_str() method
  CUSTOM_ASSERT(strcmp(settings.name.c_str(), "Alice") == 0, "c_str() should work");
  
  TEST_END();
}

void testVarAssignment() {
  TEST_START("Var Assignment Operations");
  
  TestSettings settings;
  
  // Test const char* assignment
  settings.name = "Bob";
  CUSTOM_ASSERT(strcmp(settings.name.get().c_str(), "Bob") == 0, "Assignment from const char* should work");
  
  // Test String assignment
  String testStr = "Charlie";
  settings.name = testStr;
  CUSTOM_ASSERT(strcmp(settings.name.get().c_str(), "Charlie") == 0, "Assignment from String should work");
  
  // Test copy from another Var
  TestSettings settings2;
  settings2.name = settings.name.get();
  CUSTOM_ASSERT(strcmp(settings2.name.get().c_str(), "Charlie") == 0, "Copy should work");
  
  TEST_END();
}

void testSecretNeverLeaks() {
  TEST_START("VarMetaPrefsRw Never Leaks in WS");
  
  TestSettings settings;
  settings.password = "SuperSecret";
  
  // Serialize for WebSocket (should NOT include value)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  TEST_DEBUG(String("WS Serialized: ") + json);
  
  // Check that password value is NOT in JSON
  CUSTOM_ASSERT(json.indexOf("SuperSecret") == -1, "Secret value should NOT appear in WS output");
  CUSTOM_ASSERT(json.indexOf("\"type\":\"secret\"") > 0, "Type should be indicated");
  
  // Now serialize for Prefs (SHOULD include value)
  StaticJsonDocument<512> docPrefs;
  JsonObject rootPrefs = docPrefs.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), rootPrefs);
  
  String jsonPrefs;
  serializeJson(rootPrefs, jsonPrefs);
  TEST_DEBUG(String("Prefs Serialized: ") + jsonPrefs);
  
  // Check that password value IS in Prefs JSON
  CUSTOM_ASSERT(jsonPrefs.indexOf("SuperSecret") > 0, "Secret value SHOULD appear in Prefs output");
  
  TEST_END();
}

void runAllTests() {
  SUITE_START("MODEL TYPE");
  
  testStaticStringPersistence();
  testVarImplicitConversion();
  testVarAssignment();
  testSecretNeverLeaks();
  
  SUITE_END("MODEL TYPE");
}

} // namespace ModelTypeTest
