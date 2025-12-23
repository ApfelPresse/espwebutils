# ESP Web Utils

ESP Web Utils is a modular **ESP32 utility library** for PlatformIO / Arduino that combines
WiFi provisioning, OTA updates, time synchronization, live graphs, and a web UI
into a clean and easy-to-use framework.

The goal is to keep your `main.cpp` small while still offering powerful features
that are immediately visible in the browser.

---

## Minimal Example

```cpp
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
```

---

## OTA with PlatformIO

```ini
[env:esp32s3OTA]
upload_protocol = espota
upload_port = meinesp.local
upload_flags =
  -p 3232
  -a <OTA_PASSWORD>
  --timeout=10
```