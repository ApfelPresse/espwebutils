#include "WiFiProvisioner.h"
#include <nvs_flash.h>
#include "Periodic.h"
#include "Model.h"

WiFiProvisioner wifi;

// static Periodic metrics(5000);

void setup()
{
  Serial.begin(115200);
  
  delay(200);
  Serial.println();
  Serial.println("[DEBUG] Serial initialized (115200)");
  Serial.printf("[DEBUG] millis=%lu\n", millis());

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