#include "WiFiProvisioner.h"
#include <nvs_flash.h>
#include "Periodic.h"
#include "Model.h"
#include "Logger.h"

WiFiProvisioner wifi;

void setup()
{
  Serial.begin(115200);
  
  delay(200);
  Serial.println();
  Serial.println("[DEBUG] Serial initialized (115200)");
  Serial.printf("[DEBUG] millis=%lu\n", millis());

  // Set log level globally BEFORE wifi.begin()
  Logger::setLevel(LogLevel::INFO);
  Serial.printf("[DEBUG] Log level set to %s\n", Logger::levelToString(Logger::getLevel()));

  wifi.setApSsid("ESP-Setup");
  wifi.setMdnsHost("meinesp");

  wifi.onStatus([](const String &status)
                {
    Serial.println("[STATUS] " + status);
  });

  wifi.begin();

  // Passwörter auf Serial ausgeben (nur für Entwicklung)
  Serial.println("\n============ PASSWÖRTER ============");
  Serial.printf("Admin UI Pass: %s\n", wifi.model.admin.pass.get().c_str());
  Serial.printf("OTA Pass:      %s\n", wifi.model.ota.ota_pass.get().c_str());
  Serial.println("====================================\n");
}

void loop()
{
  wifi.handleLoop();

  yield();
}