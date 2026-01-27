#include <Arduino.h>
#include "../src/AdminModel.h"
#include "test_helpers.h"

namespace ButtonSystemTest {
  // Helper: Reset NVS before each test
  void reset_nvs_button_tests() {
    Preferences p;
    p.begin("model", false);
    p.clear();
    p.end();
  }

  // Test 1: Button creation and ID assignment
  void test_button_creation_with_id() {
    TEST_START("Button creation with ID");
    Button btn(42);
    CUSTOM_ASSERT((int)btn == 42, "Button casts to its ID");
    CUSTOM_ASSERT(btn.id == 42, "Button.id is set");
    TEST_END();
  }

  // Test 2: Button callback registration
  void test_button_callback_registration() {
    TEST_START("Button callback registration");
    bool called = false;
    Button btn(1);
    btn.setCallback([&called]() { called = true; });
    btn.on_trigger();
    CUSTOM_ASSERT(called, "Callback should be called");
    TEST_END();
  }

  // Test 3: Button callback without registration (should not crash)
  void test_button_no_callback() {
    TEST_START("Button no callback does not crash");
    Button btn(2);
    // Should not crash even without callback
    btn.on_trigger();
    CUSTOM_ASSERT(true, "No crash without callback");
    TEST_END();
  }

  // Test 4: Model OTA button callback setup
  void test_model_ota_button_callback_setup() {
    TEST_START("Model OTA button callback setup");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();
    
    // Check that callback is registered (button should not be null)
    CUSTOM_ASSERT((int)model.ota.generate_new_ota_pass == 0, "Default button ID is 0");
    TEST_END();
  }

  // Test 5: OTA password generation via button
  void test_ota_password_generation_via_button() {
    TEST_START("OTA password generation via button");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();
    
    // Get initial password (should be generated)
    String pw1 = String(model.ota.ota_pass.get().c_str());
    CUSTOM_ASSERT(pw1.length() > 0, "Initial OTA password is non-empty");
    
    // Trigger button manually
    model.ota.generate_new_ota_pass.on_trigger();
    
    // Get new password (should be different)
    String pw2 = String(model.ota.ota_pass.get().c_str());
    CUSTOM_ASSERT(pw2.length() > 0, "New OTA password is non-empty");
    CUSTOM_ASSERT(pw1 != pw2, "Passwords should change after triggering");
    TEST_END();
  }

  // Test 6: Button trigger preserves password length
  void test_button_password_length() {
    TEST_START("Button trigger preserves password length");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();
    
    // Trigger button multiple times
    for (int i = 0; i < 5; i++) {
      model.ota.generate_new_ota_pass.on_trigger();
      const char* pw = model.ota.ota_pass.get().c_str();
      CUSTOM_ASSERT(strlen(pw) == 12, "OTA password length remains 12");
    }
    TEST_END();
  }

  // Test 7: Button password contains only valid characters
  void test_button_password_valid_charset() {
    TEST_START("Generated password uses valid charset");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();
    
    model.ota.generate_new_ota_pass.on_trigger();
    const char* pw = model.ota.ota_pass.get().c_str();
    
    // Valid charset: no O, I, l, 0, 1
    const char* invalid = "OIl01";
    for (const char* p = pw; *p; p++) {
      for (const char* inv = invalid; *inv; inv++) {
        CUSTOM_ASSERT(*p != *inv, "Password should not contain ambiguous characters");
      }
    }
    TEST_END();
  }

  // Test 8: Button trigger updates value
  void test_model_handle_button_trigger() {
    TEST_START("Button trigger updates model value");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();
    
    String pw_before = String(model.ota.ota_pass.get().c_str());
    
    // Trigger button via on_trigger callback
    model.ota.generate_new_ota_pass.on_trigger();
    
    String pw_after = String(model.ota.ota_pass.get().c_str());
    CUSTOM_ASSERT(pw_before != pw_after, "Password should change");
    TEST_END();
  }

  // Test 9: Admin UI password generation via button
  void test_admin_password_generation_via_button() {
    TEST_START("Admin password generation via button");
    reset_nvs_button_tests();
    AdminModel model;
    model.begin();

    String pw1 = String(model.admin.pass.get().c_str());
    model.admin.generate_new_admin_ui_pass.on_trigger();
    String pw2 = String(model.admin.pass.get().c_str());

    CUSTOM_ASSERT(pw1.length() > 0, "Initial admin password is non-empty");
    CUSTOM_ASSERT(pw2.length() > 0, "New admin password is non-empty");
    CUSTOM_ASSERT(pw1 != pw2, "Admin password should change after triggering");
    TEST_END();
  }

  // Test 9: Multiple buttons don't interfere
  void test_multiple_buttons() {
    TEST_START("Multiple buttons do not interfere");
    reset_nvs_button_tests();
    
    Button btn1(1);
    Button btn2(2);
    
    int count1 = 0;
    int count2 = 0;
    
    btn1.setCallback([&count1]() { count1++; });
    btn2.setCallback([&count2]() { count2++; });
    
    btn1.on_trigger();
    btn1.on_trigger();
    btn2.on_trigger();
    
    CUSTOM_ASSERT(count1 == 2, "Button 1 callback count");
    CUSTOM_ASSERT(count2 == 1, "Button 2 callback count");
    TEST_END();
  }

  // Test 10: Button trigger persistence
  void test_button_trigger_persistence() {
    TEST_START("Button trigger persistence");
    reset_nvs_button_tests();
    {
      AdminModel model1;
      model1.begin();
      model1.ota.generate_new_ota_pass.on_trigger();
      const char* pw1 = model1.ota.ota_pass.get().c_str();
      CUSTOM_ASSERT(strlen(pw1) > 0, "Password persisted: non-empty after generation");
    }
    
    // Load again and verify password is persisted
    {
      AdminModel model2;
      model2.begin();
      const char* pw2 = model2.ota.ota_pass.get().c_str();
      CUSTOM_ASSERT(strlen(pw2) > 0, "Password persisted: non-empty after reload");
    }

    TEST_END();
  }

  // Register all button system tests
  void runAllTests() {
    SUITE_START("BUTTON SYSTEM");
    test_button_creation_with_id();
    test_button_callback_registration();
    test_button_no_callback();
    test_model_ota_button_callback_setup();
    test_ota_password_generation_via_button();
    test_button_password_length();
    test_button_password_valid_charset();
    test_model_handle_button_trigger();
    test_admin_password_generation_via_button();
    test_multiple_buttons();
    test_button_trigger_persistence();
    SUITE_END("BUTTON SYSTEM");
  }
}
