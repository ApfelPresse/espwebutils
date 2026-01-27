#include <Arduino.h>
#include "../src/AdminModel.h"
#include "test_helpers.h"

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
        TEST_START("generatePassword returns valid password");
        String pw = AdminModel::generatePassword(12);
        CUSTOM_ASSERT(pw.length() == 12, "Password length is 12");
        CUSTOM_ASSERT(pw.length() > 0, "Password is non-empty");
        TEST_END();
    }

    // Test: Password generation creates different passwords
    void test_generate_password_creates_different_passwords() {
        TEST_START("generatePassword returns different passwords");
        String pw1 = AdminModel::generatePassword(12);
        String pw2 = AdminModel::generatePassword(12);

        CUSTOM_ASSERT(pw1 != pw2, "Two generated passwords should differ");
        TEST_END();
    }

    // Test: ensurePasswords generates admin password when empty
    void test_ensure_passwords_generates_admin_password() {
        TEST_START("ensurePasswords generates admin password");
        reset_nvs();
        
        AdminModel model;
        model.begin();
        
        const char* adminPw = model.admin.pass.get().c_str();
        CUSTOM_ASSERT(adminPw != nullptr, "Admin password pointer not null");
        CUSTOM_ASSERT(strlen(adminPw) > 0, "Admin password is non-empty");
        CUSTOM_ASSERT(strlen(adminPw) == 12, "Admin password length is 12");
        TEST_END();
    }

    // Test: ensurePasswords generates OTA password when empty
    void test_ensure_passwords_generates_ota_password() {
        TEST_START("ensurePasswords generates OTA password");
        reset_nvs();
        
        AdminModel model;
        model.begin();
        
        const char* otaPw = model.ota.ota_pass.get().c_str();
        CUSTOM_ASSERT(otaPw != nullptr, "OTA password pointer not null");
        CUSTOM_ASSERT(strlen(otaPw) > 0, "OTA password is non-empty");
        CUSTOM_ASSERT(strlen(otaPw) == 12, "OTA password length is 12");
        TEST_END();
    }

    // Test: ensurePasswords does not overwrite existing admin password
    void test_ensure_passwords_preserves_existing_admin_password() {
        TEST_START("ensurePasswords preserves admin password");
        reset_nvs();
        
        AdminModel model1;
        model1.begin();
        String firstAdminPw = String(model1.admin.pass.get().c_str());
        
        // Create new model instance - should load from preferences
        AdminModel model2;
        model2.begin();
        String secondAdminPw = String(model2.admin.pass.get().c_str());

        CUSTOM_ASSERT(firstAdminPw == secondAdminPw, "Admin password preserved across instances");
        TEST_END();
    }

    // Test: ensurePasswords does not overwrite existing OTA password
    void test_ensure_passwords_preserves_existing_ota_password() {
        TEST_START("ensurePasswords preserves OTA password");
        reset_nvs();
        
        AdminModel model1;
        model1.begin();
        String firstOtaPw = String(model1.ota.ota_pass.get().c_str());
        
        // Create new model instance - should load from preferences
        AdminModel model2;
        model2.begin();
        String secondOtaPw = String(model2.ota.ota_pass.get().c_str());

        CUSTOM_ASSERT(firstOtaPw == secondOtaPw, "OTA password preserved across instances");
        TEST_END();
    }

    // Test: Manual password setting is preserved
    void test_manual_password_setting_preserved() {
        TEST_START("Manual password setting is preserved");
        reset_nvs();
        
        AdminModel model1;
        model1.begin();
        
        // Manually set passwords
        model1.admin.pass.set("MyCustomAdminPass");
        model1.ota.ota_pass.set("MyCustomOTAPass");
        model1.saveTopic("admin");
        model1.saveTopic("ota");
        
        // Create new instance and verify
        AdminModel model2;
        model2.begin();

        CUSTOM_ASSERT(String(model2.admin.pass.get().c_str()) == "MyCustomAdminPass", "Admin password preserved");
        CUSTOM_ASSERT(String(model2.ota.ota_pass.get().c_str()) == "MyCustomOTAPass", "OTA password preserved");
        TEST_END();
    }

    void runAllTests() {
        SUITE_START("MODEL PASSWORD");
        test_generate_password_creates_valid_password();
        test_generate_password_creates_different_passwords();
        test_ensure_passwords_generates_admin_password();
        test_ensure_passwords_generates_ota_password();
        test_ensure_passwords_preserves_existing_admin_password();
        test_ensure_passwords_preserves_existing_ota_password();
        test_manual_password_setting_preserved();
        SUITE_END("MODEL PASSWORD");
    }
}
