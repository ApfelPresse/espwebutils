#include <WiFiProvisioner.h>
#include <nvs_flash.h>
#include <Periodic.h>
#include <Logger.h>
#include "Model.h"

WiFiProvisioner wifi;
Model userModel;

void setup()
{
  Serial.begin(115200);
  
  delay(200);
  Serial.println();
  Serial.println("[DEBUG] Serial initialized (115200)");
  Serial.printf("[DEBUG] millis=%lu\n", millis());

  Logger::setLevel(LogLevel::INFO);
  Serial.printf("[DEBUG] Log level set to %s\n", Logger::levelToString(Logger::getLevel()));

  wifi.setApSsid("ESP-Setup");
  wifi.setMdnsHost("meinesp");

  wifi.onStatus([](const String &status)
                {
    Serial.println("[STATUS] " + status);
  });

  wifi.setUserModel(userModel);
  wifi.generateDefaultPage(userModel, "/", "Sensors", false, false, false);

  wifi.begin();

  Serial.println("\n============ PASSWÖRTER ============");
  Serial.printf("Admin UI Pass: %s\n", wifi.model.admin.pass.get().c_str());
  Serial.printf("OTA Pass:      %s\n", wifi.model.ota.ota_pass.get().c_str());
  Serial.println("====================================\n");

  Serial.println("============ USER MODEL ============");
  Serial.printf("UserModel test.value (initial): %d\n", userModel.test.value.get());
  userModel.test.value = 123;
  Serial.printf("UserModel test.value (updated): %d\n", userModel.test.value.get());
  Serial.println("====================================\n");
}

void loop()
{
  wifi.handleLoop();

  yield();
}