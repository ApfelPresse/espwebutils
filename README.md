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

---

## Configuration

### WebSocket JSON Buffer Size

By default, the library uses **2048 bytes** for WebSocket JSON buffers (sufficient for graphs with 16+ points).

If you need to send larger JSON documents (e.g., with more graph points or bigger payloads), define `MODEL_JSON_CAPACITY` before including the ModelBase header:

```cpp
// In your main.cpp, BEFORE #include "Model.h" or similar:
#define MODEL_JSON_CAPACITY 4096  // or 8192 for very large payloads

#include "Model.h"  // or your derived model header
```

Or in `platformio.ini`:

```ini
[env:esp32]
build_flags =
  -D MODEL_JSON_CAPACITY=4096
```

**Note:** Larger buffer sizes consume more heap memory, so choose the smallest size that works for your use case.