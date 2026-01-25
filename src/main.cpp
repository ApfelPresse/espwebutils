#include "WiFiProvisioner.h"
#include <nvs_flash.h>
#include "Periodic.h"
#include "Model.h"
#include "Logger.h"

WiFiProvisioner wifi;

// static Periodic metrics(5000);

void setup()
{
  Serial.begin(115200);
  
  delay(200);
  Serial.println();
  Serial.println("[DEBUG] Serial initialized (115200)");
  Serial.printf("[DEBUG] millis=%lu\n", millis());

  // Set log level to TRACE globally BEFORE wifi.begin()
  Logger::setLevel(LogLevel::TRACE);
  Serial.println("[DEBUG] Log level set to TRACE");

  wifi.setApSsid("ESP-Setup");
  wifi.setMdnsHost("meinesp");

  wifi.onStatus([](const String &status)
                {
    Serial.println("[STATUS] " + status);
  });

  wifi.begin();
}

void loop()
{
  wifi.handleLoop();

  // if (metrics.ready()) {
  //   wifi.pushData("Wetter", "Temperatur", 21.5);
  //   wifi.pushData("Wetter", "Luftfeuchte", 55.0);
  //   wifi.pushData("System", "Heap", (double)ESP.getFreeHeap());
  // }

  yield();
}