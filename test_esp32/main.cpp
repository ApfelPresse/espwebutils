#include <Arduino.h>
#include <nvs_flash.h>
#include "../src/Logger.h"
#include "test_helpers.h"
#include "model_type_test/test_model.h"
#include "model_type_test/test_list.h"
#include "model_type_test/test_var_modes.h"
#include "model_type_test/test_serializer.h"
#include "model_type_test/test_point_ring_buffer.h"
#include "model_type_test/test_graph_var_sync.h"
#include "model_type_test/test_wifi_integration.h"
#include "button_system_test.h"

// Forward declarations for button and password tests
namespace ButtonSystemTest { void runAllTests(); }
namespace ModelPasswordTest { void runAllTests(); }

void clearAllPreferences() {
  Serial.println("[CLEANUP] Clearing all NVS partitions...");
  esp_err_t err = nvs_flash_erase();
  if (err == ESP_OK) {
    err = nvs_flash_init();
    if (err == ESP_OK) {
      Serial.println("[CLEANUP] NVS cleared and reinitialized successfully");
    } else {
      Serial.printf("[CLEANUP] NVS init failed: %d\n", err);
    }
  } else {
    Serial.printf("[CLEANUP] NVS erase failed: %d\n", err);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for serial to be ready
  
  // Set log level - change this to see more or less output
  // LogLevel::TRACE - All debug details
  // LogLevel::DEBUG - Debug information
  // LogLevel::INFO  - General information (default)
  // LogLevel::WARN  - Warnings only
  // LogLevel::ERROR - Errors only
  Logger::setLevel(LogLevel::INFO);
  
  LOG_INFO("========================================");
  LOG_INFO("ESP32 Test Suite Starting...");
  LOG_INFO_F("Log Level: %s", Logger::levelToString(Logger::getLevel()));
  LOG_INFO("========================================");
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32 Test Suite Starting...");
  Serial.println("========================================\n");

  // Clear all preferences before tests
  clearAllPreferences();
  delay(100);

  // Run test suites
  SerializerTest::runAllTests();
  ModelTypeTest::runAllTests();
  ListTest::runAllTests();
  VarModesTest::runAllTests();
  PointRingBufferTest::runAllTests();
  GraphVarSyncTest::runAllTests();
  ButtonSystemTest::runAllTests();
  ModelPasswordTest::runAllTests();
  // WiFi integration tests  
  WiFiIntegrationTest::simulateWiFiScan();
  WiFiIntegrationTest::testWiFiSettingsPersistence();
  WiFiIntegrationTest::testWiFiWebSocketSerialization();
  WiFiIntegrationTest::testWiFiSettingsUpdate();
  WiFiIntegrationTest::testAvailableNetworksReadOnly();
  WiFiIntegrationTest::testAvailableNetworksSerialization();
  WiFiIntegrationTest::testWiFiModelIntegration();

  // Clear all preferences after tests
  Serial.println("\n[CLEANUP] Final cleanup...");
  clearAllPreferences();

  Serial.println("\n========================================");
  Serial.print("Tests: ");
  Serial.print(globalTestStats.passed + globalTestStats.failed);
  Serial.print(" | Passed: ");
  Serial.print(globalTestStats.passed);
  Serial.print(" | Failed: ");
  Serial.println(globalTestStats.failed);
  
  if (globalTestStats.failed == 0) {
    Serial.println("✓ ALL TESTS PASSED");
  } else {
    Serial.println("❌ SOME TESTS FAILED");
  }
  Serial.println("========================================\n");
}

void loop() {
  // Tests run only once in setup
  delay(1000);
}
