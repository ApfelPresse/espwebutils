#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/ModelTypePrimitive.h"
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
  TEST_ASSERT(strcmp(settings.name.get().c_str(), "TestUser") == 0, "Name should be 'TestUser'");
  TEST_ASSERT(strcmp(settings.password.get().c_str(), "SecretPass123") == 0, "Password should be 'SecretPass123'");
  
  // Serialize to JSON
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.println("Serialized: " + json);
  
  // Save to preferences
  prefs.begin(TEST_NS, false);
  prefs.putString("settings", json);
  prefs.end();
  
  // Create new instance and load
  TestSettings loadedSettings;
  prefs.begin(TEST_NS, true);
  String loadedJson = prefs.getString("settings", "");
  prefs.end();
  
  TEST_ASSERT(loadedJson.length() > 0, "Loaded JSON should not be empty");
  
  // Deserialize
  StaticJsonDocument<512> loadedDoc;
  DeserializationError err = deserializeJson(loadedDoc, loadedJson);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
  }
  JsonObject loadedRoot = loadedDoc.as<JsonObject>();
  
  Serial.print("JSON to deserialize: ");
  Serial.println(loadedJson);
  Serial.print("JSON has 'name': ");
  Serial.println(loadedRoot.containsKey("name") ? "YES" : "NO");
  Serial.print("JSON has 'password': ");
  Serial.println(loadedRoot.containsKey("password") ? "YES" : "NO");
  if (loadedRoot.containsKey("name")) {
    Serial.print("  name value in JSON: '");
    Serial.print(loadedRoot["name"].as<const char*>());
    Serial.println("'");
  }
  if (loadedRoot.containsKey("password")) {
    Serial.print("  password value in JSON: '");
    Serial.print(loadedRoot["password"].as<const char*>());
    Serial.println("'");
  }
  
  Serial.println("Before readFieldsTolerant:");
  Serial.print("  loadedSettings.name = '");
  Serial.print(loadedSettings.name.get().c_str());
  Serial.println("'");
  Serial.print("  loadedSettings.password = '");
  Serial.print(loadedSettings.password.get().c_str());
  Serial.println("'");
  
  bool readSuccess = fj::readFieldsTolerant(loadedSettings, TestSettings::schema(), loadedRoot);
  TEST_ASSERT(readSuccess, "Reading should succeed");
  
  Serial.println("After readFieldsTolerant:");
  Serial.print("  loadedSettings.name = '");
  Serial.print(loadedSettings.name.get().c_str());
  Serial.println("'");
  Serial.print("  loadedSettings.password = '");
  Serial.print(loadedSettings.password.get().c_str());
  Serial.println("'");
  
  // Verify loaded values
  TEST_ASSERT(strcmp(loadedSettings.name.get().c_str(), "TestUser") == 0, "Loaded name should match");
  TEST_ASSERT(strcmp(loadedSettings.password.get().c_str(), "SecretPass123") == 0, "Loaded password should match");
  
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
  TEST_ASSERT(strcmp(namePtr, "Alice") == 0, "Implicit conversion should work");
  
  // Test c_str() method
  TEST_ASSERT(strcmp(settings.name.c_str(), "Alice") == 0, "c_str() should work");
  
  TEST_END();
}

void testVarAssignment() {
  TEST_START("Var Assignment Operations");
  
  TestSettings settings;
  
  // Test const char* assignment
  settings.name = "Bob";
  TEST_ASSERT(strcmp(settings.name.get().c_str(), "Bob") == 0, "Assignment from const char* should work");
  
  // Test String assignment
  String testStr = "Charlie";
  settings.name = testStr;
  TEST_ASSERT(strcmp(settings.name.get().c_str(), "Charlie") == 0, "Assignment from String should work");
  
  // Test copy from another Var
  TestSettings settings2;
  settings2.name = settings.name.get();
  TEST_ASSERT(strcmp(settings2.name.get().c_str(), "Charlie") == 0, "Copy should work");
  
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
  Serial.println("WS Serialized: " + json);
  
  // Check that password value is NOT in JSON
  TEST_ASSERT(json.indexOf("SuperSecret") == -1, "Secret value should NOT appear in WS output");
  TEST_ASSERT(json.indexOf("\"type\":\"secret\"") > 0, "Type should be indicated");
  
  // Now serialize for Prefs (SHOULD include value)
  StaticJsonDocument<512> docPrefs;
  JsonObject rootPrefs = docPrefs.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), rootPrefs);
  
  String jsonPrefs;
  serializeJson(rootPrefs, jsonPrefs);
  Serial.println("Prefs Serialized: " + jsonPrefs);
  
  // Check that password value IS in Prefs JSON
  TEST_ASSERT(jsonPrefs.indexOf("SuperSecret") > 0, "Secret value SHOULD appear in Prefs output");
  
  TEST_END();
}

void runAllTests() {
  Serial.println("\n===== MODEL TYPE TESTS =====\n");
  
  testStaticStringPersistence();
  testVarImplicitConversion();
  testVarAssignment();
  testSecretNeverLeaks();
  
  Serial.println("\n===== MODEL TYPE TESTS COMPLETE =====\n");
}

} // namespace ModelTypeTest
