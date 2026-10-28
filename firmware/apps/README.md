# firmware/apps

Firmware applications that compose device drivers and, optionally, service libraries. Each app is a standalone PlatformIO project — build and flash directly to an ESP32-S3.

## Apps

| App | Hardware | Services used | Description |
|---|---|---|---|
| [ApiBLE](ApiBLE/) | nulllaborg ESP32-S3 CAM | none (app-local BLE modules) | Unified BLE app: Camera + GPIO + Config. Advertises as ApiBLE. |
| [CameraWebServerDemo](CameraWebServerDemo/) | nulllaborg ESP32-S3 CAM | `CameraWebServer` | WiFi-connected MJPEG web server with captive portal. |
| [MoistureMonitorDemo](MoistureMonitorDemo/) | DevKitC-1, OLED, LM393 sensor | `MoistureMonitor` | Soil moisture monitor with DRY/WET/OK status on OLED. |
| [TimedCycleControllerDemo](TimedCycleControllerDemo/) | DevKitC-1, OLED, relay | `TimedCycleController` | Duty-cycle pump controller with live countdown timers. |
| [StatusDashboardDemo](StatusDashboardDemo/) | DevKitC-1, OLED | `StatusDashboard` | Multi-task system telemetry dashboard with scrolling OLED. |
| [MoisturePump](MoisturePump/) | DevKitC-1, OLED, LM393, relay | `MoistureMonitor`, `TimedCycleController` | Irrigation: MoistureMonitor + TimedCycleController. |
| [StatusPanel](StatusPanel/) | DevKitC-1, OLED, NeoPixel | `StatusDashboard` | System health: StatusDashboard + LedRGBController. |
| [MultiSensor](MultiSensor/) | DevKitC-1, OLED, 3 sensors | none (device drivers only) | 3-channel analog sensor monitor. |

`ApiBLE`'s Camera (CA00), GPIO (BA00), and Config (CF00) modules are implemented in `apps/ApiBLE/src` — they are **not** libraries from `firmware/services/`.

## Build

Install PlatformIO once, then run from the repo root:

```bash
uv tool install platformio==6.1.19
cd firmware/apps/<App>
pio run --target upload
pio run --target upload --target monitor
```

## Import Pattern

All apps import from the same two directories:

```ini
lib_extra_dirs =
    ../services          # firmware/services/ — service libraries
    ../device_drivers    # firmware/device_drivers/ — hardware drivers
```

## Relationship to Services

Apps are **flashable** — they have `main.cpp` + `platformio.ini` and get uploaded to the ESP32.

Services are **libraries only** — they have `library.json` and are imported via `lib_extra_dirs`. They cannot be flashed directly.

Each Demo app exercises a single service, except MultiSensor (drivers only). Integration apps compose multiple pieces: MoisturePump uses two services, StatusPanel uses a service plus a driver, and MultiSensor uses multiple drivers.
