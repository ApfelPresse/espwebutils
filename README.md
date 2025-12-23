# ESP Web Utils

ESP Web Utils is a modular **ESP32 utility library** for PlatformIO / Arduino that combines
WiFi provisioning, OTA updates, time synchronization, live graphs, and a web UI
into a clean and easy-to-use framework.

The goal is to keep your `main.cpp` small while still offering powerful features
that are immediately visible in the browser.

---

## Features

### WiFi & Provisioning
- Automatic STA connection
- Fallback Access Point (AP) mode
- mDNS support (`<hostname>.local`)
- Status endpoint (`/status`)
- Reset device via Web UI

### OTA Updates
- OTA updates via `ArduinoOTA`
- Time-limited OTA window (default: 10 minutes)
- Password-protected OTA
- Auto-generated OTA password (stored in NVS)
- OTA configuration via Web UI
- Fully compatible with PlatformIO OTA upload

### Time Synchronization
- Automatic NTP sync after WiFi connection
- Timezone: **Berlin (CET / CEST)**
- Unix epoch seconds or milliseconds
- Safe fallback to `millis()` until time is valid

### Live Graphs (WebSocket)
- WebSocket endpoint: `/ws/graphs`
- Simple API:
  ```cpp
  pushData(graph, label, x, y);
  pushData(graph, label, y); // x = current time
  ```
- Ring buffer with **20 data points per series**
- Initial history sent on page load
- Real-time live updates
- Fully dynamic web UI (graphs appear automatically)

### Web UI
- Materialize CSS
- Modular pages
- Graph dashboard (`/graphs`)
- OTA / status / reset pages
- HTML/CSS/JS compiled into gzip-compressed webfiles

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

## Live Graphs – Firmware API

```cpp
pushData("Weather", "Temperature", 21.4);
pushData("Weather", "Humidity", 55.0);
pushData("System", "Heap", ESP.getFreeHeap());
```

## OTA with PlatformIO

```ini
[env:esp32s3OTA]
upload_protocol = espota
upload_port = 192.168.2.210
upload_flags =
  -p 3232
  -a <OTA_PASSWORD>
  --timeout=10
```