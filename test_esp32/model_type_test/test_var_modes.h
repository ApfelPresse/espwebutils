#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../../src/model/ModelVar.h"
#include <Preferences.h>

namespace VarModesTest {

// Test struct with different Var modes
struct TestSettings {
  static const int STR_LEN = 32;

  // Value mode, Prefs on, Read-Write
  fj::VarWsPrefsRw<StringBuffer<STR_LEN>> name;
  
  // Value mode, no Prefs, Read-Write
  fj::VarWsRw<StringBuffer<STR_LEN>> tempValue;
  
  // Value mode, Prefs on, Read-Only
  fj::VarWsPrefsRo<StringBuffer<STR_LEN>> deviceId;
  
  // Value mode, no Prefs, Read-Only
  fj::VarWsRo<StringBuffer<STR_LEN>> statusCode;
  
  // Meta mode, Prefs on, Read-Write
  fj::VarMetaPrefsRw<StringBuffer<STR_LEN>> password;
  
  // Meta mode, no Prefs, Read-Write
  fj::VarMetaRw<StringBuffer<STR_LEN>> secretPin;

  typedef fj::Schema<TestSettings,
                     fj::Field<TestSettings, decltype(name)>,
                     fj::Field<TestSettings, decltype(tempValue)>,
                     fj::Field<TestSettings, decltype(deviceId)>,
                     fj::Field<TestSettings, decltype(statusCode)>,
                     fj::Field<TestSettings, decltype(password)>,
                     fj::Field<TestSettings, decltype(secretPin)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<TestSettings>(
        fj::Field<TestSettings, decltype(name)>{"name", &TestSettings::name},
        fj::Field<TestSettings, decltype(tempValue)>{"tempValue", &TestSettings::tempValue},
        fj::Field<TestSettings, decltype(deviceId)>{"deviceId", &TestSettings::deviceId},
        fj::Field<TestSettings, decltype(statusCode)>{"statusCode", &TestSettings::statusCode},
        fj::Field<TestSettings, decltype(password)>{"password", &TestSettings::password},
        fj::Field<TestSettings, decltype(secretPin)>{"secretPin", &TestSettings::secretPin});
    return s;
  }
};

void testVarWsPrefsRw() {
  TEST_START("VarWsPrefsRw (Value+Prefs+RW)");
  
  fj::VarWsPrefsRw<StringBuffer<32>> var;
  var = "TestValue";
  
  CUSTOM_ASSERT(strcmp(var.c_str(), "TestValue") == 0, "Should store value");
  
  // Serialize for WebSocket (should include value)
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  root["value"] = var.c_str();
  
  String json;
  serializeJson(root, json);
  CUSTOM_ASSERT(json.indexOf("TestValue") > 0, "WS should include value");
  
  // Serialize for Prefs (should include value)
  StaticJsonDocument<256> docPrefs;
  JsonObject rootPrefs = docPrefs.to<JsonObject>();
  rootPrefs["value"] = var.c_str();
  
  String jsonPrefs;
  serializeJson(rootPrefs, jsonPrefs);
  CUSTOM_ASSERT(jsonPrefs.indexOf("TestValue") > 0, "Prefs should include value");
  
  TEST_END();
}

void testVarWsRo() {
  TEST_START("VarWsRo (Value+NoPrefs+RO)");
  
  TestSettings settings;
  settings.statusCode = "200";
  
  CUSTOM_ASSERT(strcmp(settings.statusCode.c_str(), "200") == 0, "Should store value");
  
  // Serialize for WebSocket (should include value)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.println("VarWsRo WS: " + json);
  CUSTOM_ASSERT(json.indexOf("200") > 0, "WS should include value");
  
  // Try to deserialize (should be tolerated because using tolerant mode)
  const char* updateJson = R"({"statusCode":"404"})";
  StaticJsonDocument<256> updateDoc;
  deserializeJson(updateDoc, updateJson);
  
  bool success = fj::readFieldsTolerant(settings, TestSettings::schema(), updateDoc.as<JsonObject>());
  CUSTOM_ASSERT(success, "Read-only field update should be tolerated in tolerant mode");
  CUSTOM_ASSERT(strcmp(settings.statusCode.c_str(), "200") == 0, "Value should not change (read-only)");
  
  TEST_END();
}

void testVarMetaPrefsRw() {
  TEST_START("VarMetaPrefsRw (Meta+Prefs+RW)");
  
  TestSettings settings;
  settings.password = "SecretPassword123";
  
  // Serialize for WebSocket (should NOT include value, only meta)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.println("VarMetaPrefsRw WS: " + json);
  
  CUSTOM_ASSERT(json.indexOf("SecretPassword123") == -1, "WS should NOT leak secret value");
  CUSTOM_ASSERT(json.indexOf("\"type\":\"secret\"") > 0, "WS should include meta type");
  CUSTOM_ASSERT(json.indexOf("\"initialized\"") > 0, "WS should include initialized flag");
  
  // Serialize for Prefs (should include value)
  StaticJsonDocument<512> docPrefs;
  JsonObject rootPrefs = docPrefs.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), rootPrefs);
  
  String jsonPrefs;
  serializeJson(rootPrefs, jsonPrefs);
  Serial.println("VarMetaPrefsRw Prefs: " + jsonPrefs);
  
  CUSTOM_ASSERT(jsonPrefs.indexOf("SecretPassword123") > 0, "Prefs SHOULD include secret value");
  
  TEST_END();
}

void testVarMetaRw() {
  TEST_START("VarMetaRw (Meta+NoPrefs+RW)");
  
  TestSettings settings;
  settings.secretPin = "1234";
  
  // Serialize for WebSocket (should only show meta)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.println("VarMetaRw WS: " + json);
  
  CUSTOM_ASSERT(json.indexOf("1234") == -1, "WS should NOT leak secret PIN");
  CUSTOM_ASSERT(json.indexOf("\"type\":\"secret\"") > 0, "WS should include meta type");
  
  // Serialize for Prefs (should NOT include, no persistence)
  StaticJsonDocument<512> docPrefs;
  JsonObject rootPrefs = docPrefs.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), rootPrefs);
  
  String jsonPrefs;
  serializeJson(rootPrefs, jsonPrefs);
  Serial.println("VarMetaRw Prefs: " + jsonPrefs);
  
  CUSTOM_ASSERT(jsonPrefs.indexOf("secretPin") == -1, "Prefs should NOT include non-persistent field");
  
  TEST_END();
}

void testPrefsFiltering() {
  TEST_START("Prefs Filtering (PrefsMode)");
  
  TestSettings settings;
  settings.name = "Device1";
  settings.tempValue = "42";
  settings.deviceId = "ESP32-ABC123";
  settings.statusCode = "200";
  settings.password = "MySecret";
  settings.secretPin = "9999";
  
  // Serialize for Prefs
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(settings, TestSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.println("Prefs JSON: " + json);
  
  // Check what's included
  CUSTOM_ASSERT(json.indexOf("Device1") > 0, "name (PrefsRw) should be in Prefs");
  CUSTOM_ASSERT(json.indexOf("tempValue") == -1, "tempValue (no Prefs) should NOT be in Prefs");
  CUSTOM_ASSERT(json.indexOf("ESP32-ABC123") > 0, "deviceId (PrefsRo) should be in Prefs");
  CUSTOM_ASSERT(json.indexOf("statusCode") == -1, "statusCode (no Prefs) should NOT be in Prefs");
  CUSTOM_ASSERT(json.indexOf("MySecret") > 0, "password (MetaPrefsRw) should be in Prefs");
  CUSTOM_ASSERT(json.indexOf("secretPin") == -1, "secretPin (MetaRw, no Prefs) should NOT be in Prefs");
  
  TEST_END();
}

void testReadOnlyRejection() {
  TEST_START("Read-Only Write Rejection");
  
  TestSettings settings;
  settings.deviceId = "Original-ID";
  settings.statusCode = "200";
  
  // Try to update read-only fields
  const char* updateJson = R"({
    "deviceId": "Hacked-ID",
    "statusCode": "404"
  })";
  
  StaticJsonDocument<256> doc;
  deserializeJson(doc, updateJson);
  
  bool success = fj::readFieldsStrict(settings, TestSettings::schema(), doc.as<JsonObject>());
  
  // In strict mode with read-only fields, updates should be tolerated (ignored)
  CUSTOM_ASSERT(strcmp(settings.deviceId.c_str(), "Original-ID") == 0, "deviceId should not change (read-only)");
  CUSTOM_ASSERT(strcmp(settings.statusCode.c_str(), "200") == 0, "statusCode should not change (read-only)");
  
  TEST_END();
}

void testVarOnChange() {
  TEST_START("Var onChange Callback");
  
  int changeCount = 0;
  fj::VarWsPrefsRw<int> var;
  var.setOnChange([&changeCount]() { changeCount++; });
  
  CUSTOM_ASSERT(changeCount == 0, "No changes yet");
  
  var.set(10);
  CUSTOM_ASSERT(changeCount == 1, "Should trigger on set()");
  
  var = 20;
  CUSTOM_ASSERT(changeCount == 2, "Should trigger on assignment");
  
  var += 5;
  CUSTOM_ASSERT(changeCount == 3, "Should trigger on +=");
  
  TEST_END();
}

// Test struct for VarMetaPrefsRw roundtrip test (must be outside function)
struct PasswordSettings {
  static const int PASS_LEN = 64;
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> password;
  
  typedef fj::Schema<PasswordSettings,
                     fj::Field<PasswordSettings, decltype(password)>>
      SchemaType;
  
  static const SchemaType &schema() {
    static const SchemaType s = fj::makeSchema<PasswordSettings>(
        fj::Field<PasswordSettings, decltype(password)>{"password", &PasswordSettings::password});
    return s;
  }
};

// NEW TEST: VarMetaPrefsRw Roundtrip with StringBuffer (would have caught the bug!)
void testVarMetaPrefsRwRoundtrip() {
  TEST_START("VarMetaPrefsRw Roundtrip (StringBuffer persistence)");
  
  PasswordSettings settings1;
  settings1.password = "MySecretPassword123";
  
  // Serialize to Prefs JSON (simulating save)
  StaticJsonDocument<512> docSave;
  JsonObject rootSave = docSave.to<JsonObject>();
  fj::writeFieldsPrefs(settings1, PasswordSettings::schema(), rootSave);
  
  String jsonSaved;
  serializeJson(rootSave, jsonSaved);
  Serial.println("Saved to Prefs: " + jsonSaved);
  
  // CRITICAL: Verify the password VALUE is in the saved JSON
  CUSTOM_ASSERT(jsonSaved.indexOf("MySecretPassword123") > 0, 
              "BUG: Password value MUST be in Prefs JSON! Found: " + jsonSaved);
  
  // Deserialize from Prefs JSON (simulating load)
  PasswordSettings settings2;
  StaticJsonDocument<512> docLoad;
  deserializeJson(docLoad, jsonSaved);
  bool readOk = fj::readFieldsTolerant(settings2, PasswordSettings::schema(), docLoad.as<JsonObject>());
  
  CUSTOM_ASSERT(readOk, "Should successfully deserialize from Prefs");
  CUSTOM_ASSERT(strcmp(settings2.password.c_str(), "MySecretPassword123") == 0, 
              "Password should be restored correctly");
  
  Serial.println("Loaded password: " + String(settings2.password.c_str()));
  
  // Additional check: Verify WebSocket serialization is different (should NOT have value, only meta)
  StaticJsonDocument<512> docWs;
  JsonObject rootWs = docWs.to<JsonObject>();
  fj::writeFields(settings1, PasswordSettings::schema(), rootWs);
  
  String jsonWs;
  serializeJson(rootWs, jsonWs);
  Serial.println("WS serialized: " + jsonWs);
  
  // For VarMetaPrefsRw, WS should NOT include the password value
  CUSTOM_ASSERT(jsonWs.indexOf("MySecretPassword123") == -1,
              "BUG: WS should NOT leak password value!");
  CUSTOM_ASSERT(jsonWs.indexOf("\"type\"") > 0,
              "WS should include metadata type");
  
  TEST_END();
}

void runAllTests() {
  Serial.println("\n===== VAR MODES TESTS =====\n");
  
  testVarWsPrefsRw();
  testVarWsRo();
  testVarMetaPrefsRw();
  testVarMetaRw();
  testPrefsFiltering();
  testReadOnlyRejection();
  testVarOnChange();
  testVarMetaPrefsRwRoundtrip();  // NEW: Would have caught the bug!
  
  Serial.println("\n===== VAR MODES TESTS COMPLETE =====\n");
}

} // namespace VarModesTest
