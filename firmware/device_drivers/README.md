# firmware/device_drivers

Low-level PlatformIO driver libraries for the ESP32-S3. Each folder is a self-contained library with its own `library.json` manifest, header, and source files. These are the hardware abstraction layer — everything else in `firmware/` builds on top of these.

## Library Index

| Library | Description | External Dependencies | Internal Dependencies |
|---|---|---|---|
| [AdcController](AdcController/) | ESP32-S3 ADC wrapper — configurable resolution, attenuation, e-fuse calibrated mV, multi-sample averaging | — (Arduino only) | — |
| [CameraBluetoothServer](CameraBluetoothServer/) | BLE camera server — serves JPEG snapshots over BLE with chunked transfer, settings persistence, FreeRTOS pipeline | Arduino BLE stack, FreeRTOS | CameraController, StorageController |
| [CameraController](CameraController/) | esp_camera wrapper — 6 board presets, all sensor settings, frame capture with release/acquire, flash LED control | `esp_camera` (bundled) | — |
| [DisplayController](DisplayController/) | SSD1306 OLED wrapper — thread-safe, queue-based, multi-producer 4/8-line display with per-line scrolling and fonts | U8g2 (`olikraus/U8g2`), FreeRTOS | — |
| [LedRGBController](LedRGBController/) | WS2812/SK6812 NeoPixel driver — named colors, HSV, application-level semantics | Adafruit NeoPixel (`adafruit/Adafruit NeoPixel`) | — |
| [MoistureSensorController](MoistureSensorController/) | LM393 soil moisture sensor — two-point NVS-persisted calibration, hysteresis thresholds, mutex-protected setters | FreeRTOS | StorageController |
| [RelayController](RelayController/) | GPIO-driven relay/solenoid — AUTO (sensor-driven) and MANUAL (override) modes | — (Arduino only) | — |
| [StorageController](StorageController/) | NVS key-value CRUD store — type-safe get/put for int, float, ulong, bool, String; factory reset; free space monitoring | `Preferences.h` (Arduino), `nvs_flash.h` (ESP-IDF) | — |

## Inter-Library Dependency Graph

```
CameraBluetoothServer ──── depends on ──── CameraController
                        ──── depends on ──── StorageController

MoistureSensorController ── depends on ──── StorageController

DisplayController ────── depends on ──── U8g2 (external)

LedRGBController ─────── depends on ──── Adafruit NeoPixel (external)

AdcController, RelayController, StorageController — no internal dependencies
```

## Firmware Services

Drivers are composed into reusable firmware services in [../services/](../services/):

| Service | Composes |
|---|---|
| MoistureMonitor | MoistureSensorController + DisplayController + StorageController |
| TimedCycleController | RelayController + DisplayController + StorageController |
| ConfigStore | StorageController + DisplayController |
| CameraWebServer | CameraController + WiFiManager + WebServer |
| BleApiServer | BLE stack + Adafruit NeoPixel |
| StatusDashboard | DisplayController + WiFi |

## Including in Your Project

### Within this repo

```ini
lib_extra_dirs = ../lib
```

(from any project in `firmware/services/`)

### External project

```bash
git clone https://github.com/yourorg/Robotics.git /path/to/Robotics
```

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

External dependencies (U8g2, Adafruit NeoPixel, etc.) are resolved automatically by PlatformIO from each library's `library.json` manifest.
