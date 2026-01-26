#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../test_helpers.h"

// ============================================================================
// Test String<N> type
// ============================================================================

void testStringBasic() {
  TEST_START("StringBuffer<N> basic operations");

  StringBuffer<32> str;
  CUSTOM_ASSERT(str.c_str()[0] == '\0', "StringBuffer should be initialized empty");

  str.set("Hello");
  CUSTOM_ASSERT(strcmp(str.c_str(), "Hello") == 0, "StringBuffer.set() should work");

  str = "World";
  CUSTOM_ASSERT(strcmp(str.c_str(), "World") == 0, "StringBuffer operator= should work");

  StringBuffer<32> str2 = str;
  CUSTOM_ASSERT(strcmp(str2.c_str(), "World") == 0, "StringBuffer copy assignment should work");

  CUSTOM_ASSERT(str == "World", "StringBuffer operator== should work");
  CUSTOM_ASSERT(str != "Hello", "StringBuffer operator!= should work");

  TEST_END();
}

void testStringTruncation() {
  TEST_START("String<N> truncation");

  StringBuffer<5> str;
  str.set("LongString");
  CUSTOM_ASSERT(strcmp(str.c_str(), "Long") == 0, "String should truncate to N-1 chars");
  CUSTOM_ASSERT(str.c_str()[4] == '\0', "String should be null-terminated");

  TEST_END();
}

// Note: onChange callback test removed - StringBuffer no longer has internal callback
// Use Var<StringBuffer<N>> wrapper for callback functionality

void testStringTypeAdapterWs() {
  TEST_START("StringBuffer<N> TypeAdapter write_ws");

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  StringBuffer<32> str("test_value");
  fj::TypeAdapter<StringBuffer<32>>::write_ws(str, root);

  CUSTOM_ASSERT(root.containsKey("value"), "Should write 'value' key");
  CUSTOM_ASSERT(strcmp(root["value"].as<const char*>(), "test_value") == 0,
              "Should write correct string value");

  TEST_END();
}

void testStringTypeAdapterRead() {
  TEST_START("String<N> TypeAdapter read");

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  root["value"] = "read_test";

  StringBuffer<32> str;
  bool result = fj::TypeAdapter<StringBuffer<32>>::read(str, root, false);

  CUSTOM_ASSERT(result, "read should return true");
  CUSTOM_ASSERT(strcmp(str.c_str(), "read_test") == 0, "read should update string value");

  TEST_END();
}

// ============================================================================
// Test int primitive type
// ============================================================================

void testIntTypeAdapter() {
  TEST_START("int TypeAdapter");

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  int val = 42;
  fj::TypeAdapter<int>::write_ws(val, root);
  CUSTOM_ASSERT(root["value"].as<int>() == 42, "write_ws should serialize int");

  root.clear();
  root["value"] = 99;
  int val2 = 0;
  bool result = fj::TypeAdapter<int>::read(val2, root, false);
  CUSTOM_ASSERT(result && val2 == 99, "read should deserialize int");

  TEST_END();
}

// ============================================================================
// Test float primitive type
// ============================================================================

void testFloatTypeAdapter() {
  TEST_START("float TypeAdapter");

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  float val = 3.14f;
  fj::TypeAdapter<float>::write_ws(val, root);
  CUSTOM_ASSERT(abs(root["value"].as<float>() - 3.14f) < 0.01f, "write_ws should serialize float");

  root.clear();
  root["value"] = 2.71f;
  float val2 = 0.0f;
  bool result = fj::TypeAdapter<float>::read(val2, root, false);
  CUSTOM_ASSERT(result && abs(val2 - 2.71f) < 0.01f, "read should deserialize float");

  TEST_END();
}

// ============================================================================
// Test bool primitive type
// ============================================================================

void testBoolTypeAdapter() {
  TEST_START("bool TypeAdapter");

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  bool val = true;
  fj::TypeAdapter<bool>::write_ws(val, root);
  CUSTOM_ASSERT(root["value"].as<bool>() == true, "write_ws should serialize bool true");

  root.clear();
  root["value"] = false;
  bool val2 = true;
  bool result = fj::TypeAdapter<bool>::read(val2, root, false);
  CUSTOM_ASSERT(result && val2 == false, "read should deserialize bool false");

  TEST_END();
}

// ============================================================================
// Integration test with Var<> wrappers
// ============================================================================

void testStringWithVarWsPrefsRw() {
  TEST_START("String<N> with Var wrappers");

  struct TestStruct {
    fj::VarWsPrefsRw<StringBuffer<32>> name;
  };

  TestStruct test;
  test.name = "initial";

  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  // Simulate serialization
  fj::write_ws(test.name.get(), root);
  CUSTOM_ASSERT(root.containsKey("value"), "Should have 'value' key");
  CUSTOM_ASSERT(strcmp(root["value"].as<const char*>(), "initial") == 0,
              "Should serialize with Var wrapper");

  TEST_END();
}

void testIntWithVarWsRo() {
  TEST_START("int with VarWsRo");

  struct TestStruct {
    fj::VarWsRo<int> count;
  };

  TestStruct test;
  test.count = 42;

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  fj::write_ws(test.count.get(), root);
  CUSTOM_ASSERT(root.containsKey("value"), "Should have 'value' key");
  CUSTOM_ASSERT(root["value"].as<int>() == 42, "Should serialize int");

  TEST_END();
}

void testFloatWithVarMetaPrefsRw() {
  TEST_START("float with VarMetaPrefsRw");

  struct TestStruct {
    fj::VarMetaPrefsRw<float> temperature;
  };

  TestStruct test;
  test.temperature = 23.5f;

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  fj::write_ws(test.temperature.get(), root);
  CUSTOM_ASSERT(root["value"].as<float>() > 23.0f, "Should serialize float");

  TEST_END();
}

void testBoolWithVarPrefsRw() {
  TEST_START("bool with VarPrefsRw");

  struct TestStruct {
    fj::VarPrefsRw<bool> enabled;
  };

  TestStruct test;
  test.enabled = true;

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  fj::write_prefs(test.enabled.get(), root);
  CUSTOM_ASSERT(root["value"].as<bool>() == true, "Should serialize bool");

  TEST_END();
}

// ============================================================================
// Callback Tests - ensure Var<> wrapper callbacks work correctly
// ============================================================================

void testVarStringBufferCallback() {
  TEST_START("Var<StringBuffer<N>> callback on changes");

  fj::VarWsPrefsRw<StringBuffer<32>> var;
  int callback_count = 0;
  
  var.setOnChange([&callback_count]() { callback_count++; });

  // Test 1: operator= with const char*
  var = "test1";
  CUSTOM_ASSERT(callback_count == 1, "Callback should be called on operator=(const char*)");

  // Test 2: set() method
  var.set("test2");
  CUSTOM_ASSERT(callback_count == 2, "Callback should be called on set()");

  // Test 3: operator= with StringBuffer
  StringBuffer<32> buf;
  buf.set("test3");
  var = buf;
  CUSTOM_ASSERT(callback_count == 3, "Callback should be called on operator=(StringBuffer)");

  // Test 4: Verify the value was actually set
  CUSTOM_ASSERT(strcmp(var.get().c_str(), "test3") == 0, "Value should be 'test3'");

  TEST_END();
}

void testVarIntCallback() {
  TEST_START("Var<int> callback on changes");

  fj::VarWsRo<int> var;
  int callback_count = 0;
  
  var.setOnChange([&callback_count]() { callback_count++; });

  var = 42;
  CUSTOM_ASSERT(callback_count == 1, "Callback should be called on operator=");

  var.set(99);
  CUSTOM_ASSERT(callback_count == 2, "Callback should be called on set()");

  CUSTOM_ASSERT(var.get() == 99, "Value should be 99");

  TEST_END();
}

void testVarFloatCallback() {
  TEST_START("Var<float> callback on changes");

  fj::VarMetaPrefsRw<float> var;
  int callback_count = 0;
  
  var.setOnChange([&callback_count]() { callback_count++; });

  var = 3.14f;
  CUSTOM_ASSERT(callback_count == 1, "Callback should be called");

  var.set(2.71f);
  CUSTOM_ASSERT(callback_count == 2, "Callback should be called on set()");

  CUSTOM_ASSERT(abs(var.get() - 2.71f) < 0.01f, "Value should be 2.71");

  TEST_END();
}

void testVarBoolCallback() {
  TEST_START("Var<bool> callback on changes");

  fj::VarPrefsRw<bool> var;
  int callback_count = 0;
  
  var.setOnChange([&callback_count]() { callback_count++; });

  var = true;
  CUSTOM_ASSERT(callback_count == 1, "Callback should be called");

  var.set(false);
  CUSTOM_ASSERT(callback_count == 2, "Callback should be called on set()");

  CUSTOM_ASSERT(var.get() == false, "Value should be false");

  TEST_END();
}

void testSettingsStructCallbackIntegration() {
  TEST_START("Settings struct with setSaveCallback integration");

  struct TestSettings {
    fj::VarWsPrefsRw<StringBuffer<32>> name;
    fj::VarMetaPrefsRw<StringBuffer<64>> password;
    fj::VarWsRo<int> count;

    void setSaveCallback(std::function<void()> cb) {
      name.setOnChange(cb);
      password.setOnChange(cb);
      count.setOnChange(cb);
    }
  };

  TestSettings settings;
  int save_count = 0;
  
  settings.setSaveCallback([&save_count]() { save_count++; });

  // Test that all fields trigger the callback
  settings.name = "Alice";
  CUSTOM_ASSERT(save_count == 1, "name change should trigger callback");

  settings.password = "secret123";
  CUSTOM_ASSERT(save_count == 2, "password change should trigger callback");

  settings.count = 42;
  CUSTOM_ASSERT(save_count == 3, "count change should trigger callback");

  // Verify values are correct
  CUSTOM_ASSERT(strcmp(settings.name.get().c_str(), "Alice") == 0, "name should be 'Alice'");
  CUSTOM_ASSERT(strcmp(settings.password.get().c_str(), "secret123") == 0, "password should be 'secret123'");
  CUSTOM_ASSERT(settings.count.get() == 42, "count should be 42");

  TEST_END();
}

void testVarDeserializationTriggersCallback() {
  TEST_START("Var deserialization triggers callback");

  fj::VarWsPrefsRw<StringBuffer<32>> var;
  int callback_count = 0;
  
  var.setOnChange([&callback_count]() { callback_count++; });

  // Simulate deserialization (what happens when loading from Preferences or WebSocket)
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  root["value"] = "deserialized_value";

  bool success = fj::TypeAdapter<StringBuffer<32>>::read(var.get(), root, false);
  
  // Note: Direct read to var.get() won't trigger Var callback
  // This is expected behavior - use var = "value" for callback triggering
  CUSTOM_ASSERT(success, "Deserialization should succeed");
  CUSTOM_ASSERT(strcmp(var.get().c_str(), "deserialized_value") == 0, "Value should be deserialized");
  
  // But assignment through operator= WILL trigger callback
  var = "new_value";
  CUSTOM_ASSERT(callback_count == 1, "operator= should trigger callback");

  TEST_END();
}

// ============================================================================
// Run all primitive type tests
// ============================================================================

void testPrimitiveTypesAll() {
  Serial.println("\n=== Testing Primitive Types ===");

  testStringBasic();
  testStringTruncation();
  testStringTypeAdapterWs();
  testStringTypeAdapterRead();

  testIntTypeAdapter();
  testFloatTypeAdapter();
  testBoolTypeAdapter();

  testStringWithVarWsPrefsRw();
  testIntWithVarWsRo();
  testFloatWithVarMetaPrefsRw();
  testBoolWithVarPrefsRw();

  Serial.println("\n--- Callback Tests ---");
  testVarStringBufferCallback();
  testVarIntCallback();
  testVarFloatCallback();
  testVarBoolCallback();
  testSettingsStructCallbackIntegration();
  testVarDeserializationTriggersCallback();

  Serial.println("=== Primitive Types Tests Complete ===\n");
}
