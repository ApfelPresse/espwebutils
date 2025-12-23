#include "WiFiProvisioner.h"

WiFiProvisioner wifi;

void setup()
{
  Serial.begin(115200);

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

  graphs.pushData("Wetter", "Temperatur", (double)millis(), 21.5);
  graphs.pushData("Wetter", "Luftfeuchte", (double)millis(), 55.0);
  graphs.pushData("System", "Heap", (double)millis(), (double)ESP.getFreeHeap());

  delay(50);
}