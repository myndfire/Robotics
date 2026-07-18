# CameraWebServer

WiFi-connected MJPEG camera web server with captive portal setup. Composes `CameraController` + `WiFiManager` + `WebServer`. Provides snapshot capture and live MJPEG streaming.

## Dependencies

| Library | Source |
|---|---|
| CameraController | `firmware/device_drivers/` |
| WiFiManager | External (`tzapu/WiFiManager`) |

## Setup

```bash
cd services/CameraWebServer
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

External project:

```ini
lib_extra_dirs =
    /path/to/Robotics/firmware/device_drivers
    /path/to/Robotics/firmware/services
```

## Hardware

**ESP32-S3 CAM Dev Kit** — 8 MB PSRAM OPI, 8 MB Flash QIO, OV2640/OV5640 camera. USB-C for power + programming + serial.

## API

```cpp
#include <CameraWebServer.h>

CameraWebServer server;

void setup() {
    Serial.begin(921600);
    server.begin();
}
```

### Methods

| Method | Returns | Description |
|---|---|---|
| `begin()` | void | Initialize camera, WiFi (captive portal on first boot), HTTP server, start FreeRTOS camera + web tasks |

### FreeRTOS Tasks

- **camera task** (Core 0, ~10fps): captures frames into depth-1 queue
- **web task** (Core 1): handles HTTP requests, serves dashboard + MJPEG stream

### HTTP Endpoints

| URL | Content |
|---|---|
| `http://<ip>/` | Dashboard with auto-refreshing snapshot |
| `http://<ip>/camera/capture` | Single JPEG still capture |
| `http://<ip>/camera/stream` | MJPEG live stream (multipart/x-mixed-replace) |
| `http://<ip>/wifi/reset` | Clear saved WiFi credentials and reboot |

## First Boot

1. Board creates AP **"ESP32-S3-CAM-Setup"**
2. Connect to the AP from phone/laptop
3. Captive portal opens — enter WiFi credentials
4. Board reboots and connects to your network
5. Serial monitor shows IP address

## Expected Behavior

### Serial (921600 baud)

```
CameraWebServer ready:
  http://192.168.1.42
  Endpoints: /  /camera/capture  /camera/stream  /wifi/reset
```

Default camera settings: QVGA (320×240), JPEG quality 12, VFlip enabled, Auto WB, 20 MHz XCLK.
