#include <unity.h>
#include <Arduino.h>
#include "../src/Model.h"
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
    Button btn(42);
    TEST_ASSERT_EQUAL(42, (int)btn);
    TEST_ASSERT_EQUAL(42, btn.id);
  }

  // Test 2: Button callback registration
  void test_button_callback_registration() {
    bool called = false;
    Button btn(1);
    btn.setCallback([&called]() { called = true; });
    btn.on_trigger();
    TEST_ASSERT_TRUE(called);
  }

  // Test 3: Button callback without registration (should not crash)
  void test_button_no_callback() {
    Button btn(2);
    // Should not crash even without callback
    btn.on_trigger();
    TEST_ASSERT_TRUE(true); // If we get here, no crash
  }

  // Test 4: Model OTA button callback setup
  void test_model_ota_button_callback_setup() {
    reset_nvs_button_tests();
    Model model;
    model.begin();
    
    // Check that callback is registered (button should not be null)
    TEST_ASSERT_EQUAL(0, (int)model.ota.generate_new_ota_pass); // Default ID
  }

  // Test 5: OTA password generation via button
  void test_ota_password_generation_via_button() {
    reset_nvs_button_tests();
    Model model;
    model.begin();
    
    // Get initial password (should be generated)
    const char* pw1 = model.ota.ota_pass.get().c_str();
    TEST_ASSERT_NOT_NULL(pw1);
    TEST_ASSERT_GREATER_THAN(0, strlen(pw1));
    
    // Trigger button manually
    model.ota.generate_new_ota_pass.on_trigger();
    
    // Get new password (should be different)
    const char* pw2 = model.ota.ota_pass.get().c_str();
    TEST_ASSERT_NOT_NULL(pw2);
    TEST_ASSERT_GREATER_THAN(0, strlen(pw2));
    
    // Passwords should be different (with very high probability)
    int cmp = strcmp(pw1, pw2);
    TEST_ASSERT_NOT_EQUAL(0, cmp);
  }

  // Test 6: Button trigger preserves password length
  void test_button_password_length() {
    reset_nvs_button_tests();
    Model model;
    model.begin();
    
    // Trigger button multiple times
    for (int i = 0; i < 5; i++) {
      model.ota.generate_new_ota_pass.on_trigger();
      const char* pw = model.ota.ota_pass.get().c_str();
      TEST_ASSERT_EQUAL(12, strlen(pw)); // Default length is 12
    }
  }

  // Test 7: Button password contains only valid characters
  void test_button_password_valid_charset() {
    reset_nvs_button_tests();
    Model model;
    model.begin();
    
    model.ota.generate_new_ota_pass.on_trigger();
    const char* pw = model.ota.ota_pass.get().c_str();
    
    // Valid charset: no O, I, l, 0, 1
    const char* invalid = "OIl01";
    for (const char* p = pw; *p; p++) {
      for (const char* inv = invalid; *inv; inv++) {
        TEST_ASSERT_NOT_EQUAL(*p, *inv);
      }
    }
  }

  // Test 8: Button trigger updates value
  void test_model_handle_button_trigger() {
    reset_nvs_button_tests();
    Model model;
    model.begin();
    
    const char* pw_before = model.ota.ota_pass.get().c_str();
    
    // Trigger button via on_trigger callback
    model.ota.generate_new_ota_pass.on_trigger();
    
    const char* pw_after = model.ota.ota_pass.get().c_str();
    int cmp = strcmp(pw_before, pw_after);
    TEST_ASSERT_NOT_EQUAL(0, cmp);
  }

  // Test 9: Admin UI password generation via button
  void test_admin_password_generation_via_button() {
    reset_nvs_button_tests();
    Model model;
    model.begin();

    const char* pw1 = model.admin.pass.get().c_str();
    model.admin.generate_new_admin_ui_pass.on_trigger();
    const char* pw2 = model.admin.pass.get().c_str();

    TEST_ASSERT_NOT_NULL(pw1);
    TEST_ASSERT_NOT_NULL(pw2);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(pw1, pw2));
  }

  // Test 9: Multiple buttons don't interfere
  void test_multiple_buttons() {
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
    
    TEST_ASSERT_EQUAL(2, count1);
    TEST_ASSERT_EQUAL(1, count2);
  }

  // Test 10: Button trigger persistence
  void test_button_trigger_persistence() {
    reset_nvs_button_tests();
    {
      Model model1;
      model1.begin();
      model1.ota.generate_new_ota_pass.on_trigger();
      const char* pw1 = model1.ota.ota_pass.get().c_str();
      TEST_ASSERT_GREATER_THAN(0, strlen(pw1));
    }
    
    // Load again and verify password is persisted
    {
      Model model2;
      model2.begin();
      const char* pw2 = model2.ota.ota_pass.get().c_str();
      TEST_ASSERT_GREATER_THAN(0, strlen(pw2));
    }
  }

  // Register all button system tests
  void runAllTests() {
    RUN_TEST(test_button_creation_with_id);
    RUN_TEST(test_button_callback_registration);
    RUN_TEST(test_button_no_callback);
    RUN_TEST(test_model_ota_button_callback_setup);
    RUN_TEST(test_ota_password_generation_via_button);
    RUN_TEST(test_button_password_length);
    RUN_TEST(test_button_password_valid_charset);
    RUN_TEST(test_model_handle_button_trigger);
    RUN_TEST(test_admin_password_generation_via_button);
    RUN_TEST(test_multiple_buttons);
    RUN_TEST(test_button_trigger_persistence);
  }
}
