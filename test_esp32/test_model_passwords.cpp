#include <Arduino.h>
#include <unity.h>
#include "../src/Model.h"
#include "../src/Logger.h"

namespace ModelPasswordTest {
    // Test helpers
    void reset_nvs() {
        Preferences prefs;
        prefs.begin("model", false);
        prefs.clear();
        prefs.end();
    }

    // Test: Password generation creates non-empty string
    void test_generate_password_creates_valid_password() {
        String pw = Model::generatePassword(12);
        TEST_ASSERT_EQUAL(12, pw.length());
        TEST_ASSERT_TRUE(pw.length() > 0);
    }

    // Test: Password generation creates different passwords
    void test_generate_password_creates_different_passwords() {
        String pw1 = Model::generatePassword(12);
        String pw2 = Model::generatePassword(12);
        
        int cmp = strcmp(pw1.c_str(), pw2.c_str());
        TEST_ASSERT_NOT_EQUAL(0, cmp);
    }

    // Test: ensurePasswords generates admin password when empty
    void test_ensure_passwords_generates_admin_password() {
        reset_nvs();
        
        Model model;
        model.begin();
        
        const char* adminPw = model.admin.pass.get().c_str();
        TEST_ASSERT_NOT_NULL(adminPw);
        TEST_ASSERT_TRUE(strlen(adminPw) > 0);
        TEST_ASSERT_EQUAL(12, strlen(adminPw));
    }

    // Test: ensurePasswords generates OTA password when empty
    void test_ensure_passwords_generates_ota_password() {
        reset_nvs();
        
        Model model;
        model.begin();
        
        const char* otaPw = model.ota.ota_pass.get().c_str();
        TEST_ASSERT_NOT_NULL(otaPw);
        TEST_ASSERT_TRUE(strlen(otaPw) > 0);
        TEST_ASSERT_EQUAL(12, strlen(otaPw));
    }

    // Test: ensurePasswords does not overwrite existing admin password
    void test_ensure_passwords_preserves_existing_admin_password() {
        reset_nvs();
        
        Model model1;
        model1.begin();
        String firstAdminPw = String(model1.admin.pass.get().c_str());
        
        // Create new model instance - should load from preferences
        Model model2;
        model2.begin();
        String secondAdminPw = String(model2.admin.pass.get().c_str());
        
        TEST_ASSERT_EQUAL_STRING(firstAdminPw.c_str(), secondAdminPw.c_str());
    }

    // Test: ensurePasswords does not overwrite existing OTA password
    void test_ensure_passwords_preserves_existing_ota_password() {
        reset_nvs();
        
        Model model1;
        model1.begin();
        String firstOtaPw = String(model1.ota.ota_pass.get().c_str());
        
        // Create new model instance - should load from preferences
        Model model2;
        model2.begin();
        String secondOtaPw = String(model2.ota.ota_pass.get().c_str());
        
        TEST_ASSERT_EQUAL_STRING(firstOtaPw.c_str(), secondOtaPw.c_str());
    }

    // Test: Manual password setting is preserved
    void test_manual_password_setting_preserved() {
        reset_nvs();
        
        Model model1;
        model1.begin();
        
        // Manually set passwords
        model1.admin.pass.set("MyCustomAdminPass");
        model1.ota.ota_pass.set("MyCustomOTAPass");
        model1.saveTopic("admin");
        model1.saveTopic("ota");
        
        // Create new instance and verify
        Model model2;
        model2.begin();
        
        TEST_ASSERT_EQUAL_STRING("MyCustomAdminPass", model2.admin.pass.get().c_str());
        TEST_ASSERT_EQUAL_STRING("MyCustomOTAPass", model2.ota.ota_pass.get().c_str());
    }

    void runAllTests() {
        RUN_TEST(test_generate_password_creates_valid_password);
        RUN_TEST(test_generate_password_creates_different_passwords);
        RUN_TEST(test_ensure_passwords_generates_admin_password);
        RUN_TEST(test_ensure_passwords_generates_ota_password);
        RUN_TEST(test_ensure_passwords_preserves_existing_admin_password);
        RUN_TEST(test_ensure_passwords_preserves_existing_ota_password);
        RUN_TEST(test_manual_password_setting_preserved);
    }
}
