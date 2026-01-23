#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/ModelTypeStaticString.h"
#include "../../src/model/ModelVar.h"

namespace SerializerTest {

// Simple test struct with direct fields (no Var wrapper)
struct DirectFields {
  StaticString<32> name;
  StaticString<64> password;
  
  typedef fj::Schema<DirectFields,
                     fj::Field<DirectFields, StaticString<32>>,
                     fj::Field<DirectFields, StaticString<64>>>
      SchemaType;
  
  static const SchemaType& schema() {
    static const SchemaType s = fj::makeSchema<DirectFields>(
        fj::Field<DirectFields, StaticString<32>>{"name", &DirectFields::name},
        fj::Field<DirectFields, StaticString<64>>{"password", &DirectFields::password});
    return s;
  }
};

// Test struct with Var<> wrapped fields
struct VarFields {
  fj::VarWsPrefsRw<StaticString<32>> name;
  fj::VarMetaPrefsRw<StaticString<64>> password;
  
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
  Serial.print("Direct fields JSON: ");
  Serial.println(json);
  
  TEST_ASSERT(json.indexOf("TestUser") > 0, "Should contain TestUser");
  TEST_ASSERT(json.indexOf("TestPassword") > 0, "Should contain TestPassword");
  
  // Deserialize
  DirectFields loaded;
  StaticJsonDocument<256> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(loaded, DirectFields::schema(), loadDoc.as<JsonObject>());
  TEST_ASSERT(success, "readFieldsTolerant should succeed");
  
  Serial.print("Loaded name: '");
  Serial.print(loaded.name.c_str());
  Serial.println("'");
  Serial.print("Loaded password: '");
  Serial.print(loaded.password.c_str());
  Serial.println("'");
  
  TEST_ASSERT(strcmp(loaded.name.c_str(), "TestUser") == 0, "Loaded name should match");
  TEST_ASSERT(strcmp(loaded.password.c_str(), "TestPassword") == 0, "Loaded password should match");
  
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
  Serial.print("Var fields Prefs JSON: ");
  Serial.println(json);
  
  TEST_ASSERT(json.indexOf("VarUser") > 0, "Should contain VarUser");
  TEST_ASSERT(json.indexOf("VarPassword") > 0, "Should contain VarPassword");
  
  TEST_END();
}

void testVarFieldsRead() {
  TEST_START("Var<> Fields Read (Tolerant Mode)");
  
  const char* testJson = R"({"name":"LoadedUser","password":"LoadedPassword"})";
  
  VarFields obj;
  
  Serial.print("Before read - name: '");
  Serial.print(obj.name.get().c_str());
  Serial.println("'");
  Serial.print("Before read - password: '");
  Serial.print(obj.password.get().c_str());
  Serial.println("'");
  
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, testJson);
  TEST_ASSERT(!err, "JSON parse should succeed");
  
  Serial.println("Calling readFieldsTolerant...");
  bool success = fj::readFieldsTolerant(obj, VarFields::schema(), doc.as<JsonObject>());
  TEST_ASSERT(success, "readFieldsTolerant should succeed");
  
  Serial.print("After read - name: '");
  Serial.print(obj.name.get().c_str());
  Serial.println("'");
  Serial.print("After read - password: '");
  Serial.print(obj.password.get().c_str());
  Serial.println("'");
  
  TEST_ASSERT(strcmp(obj.name.get().c_str(), "LoadedUser") == 0, "Name should be LoadedUser");
  TEST_ASSERT(strcmp(obj.password.get().c_str(), "LoadedPassword") == 0, "Password should be LoadedPassword");
  
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
  Serial.print("Roundtrip JSON: ");
  Serial.println(json);
  
  // Read
  VarFields loaded;
  StaticJsonDocument<256> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(loaded, VarFields::schema(), loadDoc.as<JsonObject>());
  TEST_ASSERT(success, "Read should succeed");
  
  Serial.print("Roundtrip loaded name: '");
  Serial.print(loaded.name.get().c_str());
  Serial.println("'");
  Serial.print("Roundtrip loaded password: '");
  Serial.print(loaded.password.get().c_str());
  Serial.println("'");
  
  TEST_ASSERT(strcmp(loaded.name.get().c_str(), "RoundtripUser") == 0, "Name should match after roundtrip");
  TEST_ASSERT(strcmp(loaded.password.get().c_str(), "RoundtripPassword") == 0, "Password should match after roundtrip");
  
  TEST_END();
}

void testIsVarTrait() {
  TEST_START("is_var Trait Detection");
  
  // Test that is_var correctly identifies Var types
  bool isVarStaticString = fj::detail::is_var<StaticString<32>>::value;
  bool isVarWrapped = fj::detail::is_var<fj::VarWsPrefsRw<StaticString<32>>>::value;
  
  Serial.print("is_var<StaticString<32>>: ");
  Serial.println(isVarStaticString ? "TRUE" : "FALSE");
  Serial.print("is_var<VarWsPrefsRw<StaticString<32>>>: ");
  Serial.println(isVarWrapped ? "TRUE" : "FALSE");
  
  TEST_ASSERT(!isVarStaticString, "StaticString should NOT be detected as Var");
  TEST_ASSERT(isVarWrapped, "VarWsPrefsRw should be detected as Var");
  
  TEST_END();
}

void testVarDirectAssignment() {
  TEST_START("Var Direct Assignment (not via get())");
  
  fj::VarWsPrefsRw<StaticString<32>> var;
  
  Serial.print("Before: '");
  Serial.print(var.get().c_str());
  Serial.println("'");
  
  // Test 1: Direct assignment via operator=
  var = "DirectAssign";
  Serial.print("After var='DirectAssign': '");
  Serial.print(var.get().c_str());
  Serial.println("'");
  TEST_ASSERT(strcmp(var.get().c_str(), "DirectAssign") == 0, "Direct assignment should work");
  
  // Test 2: Assignment via set()
  var.set(StaticString<32>("ViaSet"));
  Serial.print("After var.set(): '");
  Serial.print(var.get().c_str());
  Serial.println("'");
  TEST_ASSERT(strcmp(var.get().c_str(), "ViaSet") == 0, "set() should work");
  
  // Test 3: Get non-const reference and modify
  StaticString<32>& ref = var.get();
  ref.set("ViaRef");
  Serial.print("After ref.set('ViaRef'): '");
  Serial.print(var.get().c_str());
  Serial.println("'");
  TEST_ASSERT(strcmp(var.get().c_str(), "ViaRef") == 0, "Modifying via non-const get() should work");
  
  TEST_END();
}

void runAllTests() {
  Serial.println("\n===== SERIALIZER TESTS =====\n");
  testIsVarTrait();
  testVarDirectAssignment();
  testDirectFieldsSerialization();
  testVarFieldsWrite();
  testVarFieldsRead();
  testVarFieldsRoundtrip();
  Serial.println("\n===== SERIALIZER TESTS COMPLETE =====\n");
}

} // namespace SerializerTest
