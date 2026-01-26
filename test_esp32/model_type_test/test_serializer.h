#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../../src/model/ModelVar.h"

namespace SerializerTest {

// Simple test struct with direct fields (no Var wrapper)
struct DirectFields {
  StringBuffer<32> name;
  StringBuffer<64> password;
  
  typedef fj::Schema<DirectFields,
                     fj::Field<DirectFields, StringBuffer<32>>,
                     fj::Field<DirectFields, StringBuffer<64>>>
      SchemaType;
  
  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<DirectFields>(
        fj::Field<DirectFields, StringBuffer<32>>{"name", &DirectFields::name},
        fj::Field<DirectFields, StringBuffer<64>>{"password", &DirectFields::password});
    return s;
  }
};

// Test struct with Var<> wrapped fields
struct VarFields {
  fj::VarWsPrefsRw<StringBuffer<32>> name;
  fj::VarMetaPrefsRw<StringBuffer<64>> password;
  
  typedef fj::Schema<VarFields,
                     fj::Field<VarFields, decltype(name)>,
                     fj::Field<VarFields, decltype(password)>>
      SchemaType;
  
  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<VarFields>(
        fj::Field<VarFields, decltype(name)>{"name", &VarFields::name},
        fj::Field<VarFields, decltype(password)>{"password", &VarFields::password});
    return s;
  }
};

// Struct to verify TypeAdapter::write_prefs uses Prefs path (not WS meta)
struct SecretPrefsStruct {
  fj::VarWsPrefsRw<StringBuffer<32>> ssid;
  fj::VarMetaPrefsRw<StringBuffer<64>> pass;

  typedef fj::Schema<SecretPrefsStruct,
                     fj::Field<SecretPrefsStruct, decltype(ssid)>,
                     fj::Field<SecretPrefsStruct, decltype(pass)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<SecretPrefsStruct>(
        fj::Field<SecretPrefsStruct, decltype(ssid)>{"ssid", &SecretPrefsStruct::ssid},
        fj::Field<SecretPrefsStruct, decltype(pass)>{"pass", &SecretPrefsStruct::pass});
    return s;
  }
};

// Strict vs tolerant behavior regression guard
struct StrictVsTolerantFields {
  fj::VarWsPrefsRw<int> a;
  fj::VarWsPrefsRw<int> b;

  typedef fj::Schema<StrictVsTolerantFields,
                     fj::Field<StrictVsTolerantFields, decltype(a)>,
                     fj::Field<StrictVsTolerantFields, decltype(b)>>
      SchemaType;

  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<StrictVsTolerantFields>(
        fj::Field<StrictVsTolerantFields, decltype(a)>{"a", &StrictVsTolerantFields::a},
        fj::Field<StrictVsTolerantFields, decltype(b)>{"b", &StrictVsTolerantFields::b});
    return s;
  }
};

void testStrictVsTolerantMissingKey() {
  TEST_START("Serializer strict vs tolerant (missing key)");

  StrictVsTolerantFields obj;
  obj.a = 1;
  obj.b = 2;

  // Missing 'b'
  const char* jsonStr = R"({"a":10})";
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  CUSTOM_ASSERT(!err, "JSON parse should succeed");

  // Tolerant: should return true and update only 'a'
  bool tol = fj::readFieldsTolerant(obj, StrictVsTolerantFields::schema(), doc.as<JsonObject>());
  CUSTOM_ASSERT(tol, "readFieldsTolerant should succeed");
  CUSTOM_ASSERT(obj.a.get() == 10, "Tolerant should update present key a");
  CUSTOM_ASSERT(obj.b.get() == 2, "Tolerant should not change missing key b");

  // Strict: should fail when any schema key is missing
  StrictVsTolerantFields obj2;
  obj2.a = 1;
  obj2.b = 2;
  bool strict = fj::readFieldsStrict(obj2, StrictVsTolerantFields::schema(), doc.as<JsonObject>());
  CUSTOM_ASSERT(!strict, "readFieldsStrict should fail with missing key");
  CUSTOM_ASSERT(obj2.a.get() == 10, "Strict currently still applies updates for present keys");
  CUSTOM_ASSERT(obj2.b.get() == 2, "Strict should keep missing key unchanged");

  TEST_END();
}

void testStrictIgnoresExtraKeys() {
  TEST_START("Serializer strict ignores extra keys");

  StrictVsTolerantFields obj;
  obj.a = 1;
  obj.b = 2;

  const char* jsonStr = R"({"a":3,"b":4,"extra":999})";
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  CUSTOM_ASSERT(!err, "JSON parse should succeed");

  bool strict = fj::readFieldsStrict(obj, StrictVsTolerantFields::schema(), doc.as<JsonObject>());
  CUSTOM_ASSERT(strict, "readFieldsStrict should succeed when all schema keys exist");
  CUSTOM_ASSERT(obj.a.get() == 3, "a updated");
  CUSTOM_ASSERT(obj.b.get() == 4, "b updated");

  TEST_END();
}

void testDirectFieldsSerialization() {
  TEST_START("Direct Fields Serialization/Deserialization");
  
  DirectFields obj;
  obj.name.set("TestUser");
  obj.password.set("TestPassword");
  
  // Serialize
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(obj, DirectFields::schema(), root);
  
  String json;
  serializeJson(root, json);
  TEST_DEBUG(String("Direct fields JSON: ") + json);
  
  CUSTOM_ASSERT(json.indexOf("TestUser") > 0, "Should contain TestUser");
  CUSTOM_ASSERT(json.indexOf("TestPassword") > 0, "Should contain TestPassword");
  
  // Deserialize
  DirectFields loaded;
  StaticJsonDocument<256> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(loaded, DirectFields::schema(), loadDoc.as<JsonObject>());
  CUSTOM_ASSERT(success, "readFieldsTolerant should succeed");
  
  TEST_TRACE_F("Loaded name: '%s'", loaded.name.c_str());
  TEST_TRACE_F("Loaded password: '%s'", loaded.password.c_str());
  
  CUSTOM_ASSERT(strcmp(loaded.name.c_str(), "TestUser") == 0, "Loaded name should match");
  CUSTOM_ASSERT(strcmp(loaded.password.c_str(), "TestPassword") == 0, "Loaded password should match");
  
  TEST_END();
}

void testVarFieldsWrite() {
  TEST_START("Var<> Fields Write (Prefs Mode)");
  
  VarFields obj;
  obj.name = "VarUser";
  obj.password = "VarPassword";
  
  // Serialize for Prefs (should extract raw values)
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(obj, VarFields::schema(), root);
  
  String json;
  serializeJson(root, json);
  TEST_DEBUG(String("Var fields Prefs JSON: ") + json);
  
  CUSTOM_ASSERT(json.indexOf("VarUser") > 0, "Should contain VarUser");
  CUSTOM_ASSERT(json.indexOf("VarPassword") > 0, "Should contain VarPassword");
  
  TEST_END();
}

void testVarFieldsRead() {
  TEST_START("Var<> Fields Read (Tolerant Mode)");
  
  const char* testJson = R"({"name":"LoadedUser","password":"LoadedPassword"})";
  
  VarFields obj;
  
  TEST_TRACE_F("Before read - name: '%s'", obj.name.get().c_str());
  TEST_TRACE_F("Before read - password: '%s'", obj.password.get().c_str());
  
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, testJson);
  CUSTOM_ASSERT(!err, "JSON parse should succeed");
  
  TEST_TRACE("Calling readFieldsTolerant...");
  bool success = fj::readFieldsTolerant(obj, VarFields::schema(), doc.as<JsonObject>());
  CUSTOM_ASSERT(success, "readFieldsTolerant should succeed");
  
  TEST_TRACE_F("After read - name: '%s'", obj.name.get().c_str());
  TEST_TRACE_F("After read - password: '%s'", obj.password.get().c_str());
  
  CUSTOM_ASSERT(strcmp(obj.name.get().c_str(), "LoadedUser") == 0, "Name should be LoadedUser");
  CUSTOM_ASSERT(strcmp(obj.password.get().c_str(), "LoadedPassword") == 0, "Password should be LoadedPassword");
  
  TEST_END();
}

void testVarFieldsRoundtrip() {
  TEST_START("Var<> Fields Roundtrip (Write->Read)");
  
  VarFields original;
  original.name = "RoundtripUser";
  original.password = "RoundtripPassword";
  
  // Write
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(original, VarFields::schema(), root);
  
  String json;
  serializeJson(root, json);
  TEST_DEBUG(String("Roundtrip JSON: ") + json);
  
  // Read
  VarFields loaded;
  StaticJsonDocument<256> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(loaded, VarFields::schema(), loadDoc.as<JsonObject>());
  CUSTOM_ASSERT(success, "Read should succeed");
  
  TEST_TRACE_F("Roundtrip loaded name: '%s'", loaded.name.get().c_str());
  TEST_TRACE_F("Roundtrip loaded password: '%s'", loaded.password.get().c_str());
  
  CUSTOM_ASSERT(strcmp(loaded.name.get().c_str(), "RoundtripUser") == 0, "Name should match after roundtrip");
  CUSTOM_ASSERT(strcmp(loaded.password.get().c_str(), "RoundtripPassword") == 0, "Password should match after roundtrip");
  
  TEST_END();
}

// Regression: ensure TypeAdapter::write_prefs uses Prefs path (writes actual value for Meta fields)
void testTypeAdapterWritePrefsUsesPrefsPath() {
  TEST_START("TypeAdapter::write_prefs uses Prefs path (secret persists)");

  SecretPrefsStruct obj;
  obj.ssid = "HomeWiFi";
  obj.pass = "SuperSecret123";

  // WS serialization (must NOT leak secret)
  StaticJsonDocument<384> wsDoc;
  JsonObject wsRoot = wsDoc.to<JsonObject>();
  fj::write_ws(obj, wsRoot);
  String wsJson; serializeJson(wsRoot, wsJson);
  TEST_DEBUG(String("WS JSON: ") + wsJson);
  CUSTOM_ASSERT(wsJson.indexOf("SuperSecret123") < 0, "WS MUST NOT contain secret value");
  CUSTOM_ASSERT(wsJson.indexOf("\"type\":\"secret\"") > 0, "WS should contain meta type");

  // Prefs serialization (must persist actual value)
  StaticJsonDocument<384> prefsDoc;
  JsonObject prefsRoot = prefsDoc.to<JsonObject>();
  fj::write_prefs(obj, prefsRoot);
  String prefsJson; serializeJson(prefsRoot, prefsJson);
  TEST_DEBUG(String("Prefs JSON: ") + prefsJson);
  CUSTOM_ASSERT(prefsJson.indexOf("SuperSecret123") > 0, "Prefs MUST contain secret value");
  CUSTOM_ASSERT(prefsJson.indexOf("\"type\":\"secret\"") < 0, "Prefs should NOT be meta-only");

  // Roundtrip load from Prefs JSON
  SecretPrefsStruct loaded;
  StaticJsonDocument<384> loadDoc;
  deserializeJson(loadDoc, prefsJson);
  bool ok = fj::readFieldsTolerant(loaded, SecretPrefsStruct::schema(), loadDoc.as<JsonObject>());
  CUSTOM_ASSERT(ok, "readFieldsTolerant should succeed");
  CUSTOM_ASSERT(strcmp(loaded.pass.get().c_str(), "SuperSecret123") == 0, "Secret value should roundtrip from Prefs");

  TEST_END();
}

void testIsVarTrait() {
  TEST_START("is_var Trait Detection");
  
  // Test that is_var correctly identifies Var types
  bool isVarStaticString = fj::detail::is_var<StringBuffer<32>>::value;
  bool isVarWrapped = fj::detail::is_var<fj::VarWsPrefsRw<StringBuffer<32>>>::value;
  
  TEST_TRACE_F("is_var<StringBuffer<32>>: %s", isVarStaticString ? "true" : "false");
  TEST_TRACE_F("is_var<VarWsPrefsRw<StringBuffer<32>>>: %s", isVarWrapped ? "true" : "false");
  
  CUSTOM_ASSERT(!isVarStaticString, "StaticString should NOT be detected as Var");
  CUSTOM_ASSERT(isVarWrapped, "VarWsPrefsRw should be detected as Var");
  
  TEST_END();
}

void testVarDirectAssignment() {
  TEST_START("Var Direct Assignment (not via get())");
  
  fj::VarWsPrefsRw<StringBuffer<32>> var;
  
  TEST_TRACE_F("Before: '%s'", var.get().c_str());
  
  // Test 1: Direct assignment via operator=
  var = "DirectAssign";
  TEST_TRACE_F("After var='DirectAssign': '%s'", var.get().c_str());
  CUSTOM_ASSERT(strcmp(var.get().c_str(), "DirectAssign") == 0, "Direct assignment should work");
  
  // Test 2: Assignment via set()
  var.set(StringBuffer<32>("ViaSet"));
  TEST_TRACE_F("After var.set(): '%s'", var.get().c_str());
  CUSTOM_ASSERT(strcmp(var.get().c_str(), "ViaSet") == 0, "set() should work");
  
  // Test 3: Get non-const reference and modify
  StringBuffer<32>& ref = var.get();
  ref.set("ViaRef");
  TEST_TRACE_F("After ref.set('ViaRef'): '%s'", var.get().c_str());
  CUSTOM_ASSERT(strcmp(var.get().c_str(), "ViaRef") == 0, "Modifying via non-const get() should work");
  
  TEST_END();
}

void runAllTests() {
  SUITE_START("SERIALIZER");
  testIsVarTrait();
  testVarDirectAssignment();
  testDirectFieldsSerialization();
  testVarFieldsWrite();
  testVarFieldsRead();
  testVarFieldsRoundtrip();
  testTypeAdapterWritePrefsUsesPrefsPath();
  testStrictVsTolerantMissingKey();
  testStrictIgnoresExtraKeys();
  SUITE_END("SERIALIZER");
}

} // namespace SerializerTest
