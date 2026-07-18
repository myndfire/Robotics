# firmware/apps

Firmware applications that compose services and drivers. Each app is a standalone PlatformIO project — build and flash directly to an ESP32-S3.

## Apps

| App | Hardware | Description |
|---|---|---|
| [CameraBLE](CameraBLE/) | nulllaborg ESP32-S3 CAM | BLE camera snapshot server. Advertises as ESP32-CAM. |
| [BleApiServerDemo](BleApiServerDemo/) | DevKitC-1 / DORHEA Mini | BLE GPIO + NeoPixel API server. Advertises as ESP32-API. |
| [CameraWebServerDemo](CameraWebServerDemo/) | nulllaborg ESP32-S3 CAM | WiFi-connected MJPEG web server with captive portal. |
| [MoistureMonitorDemo](MoistureMonitorDemo/) | DevKitC-1, OLED, LM393 sensor | Soil moisture monitor with DRY/WET/OK status on OLED. |
| [TimedCycleControllerDemo](TimedCycleControllerDemo/) | DevKitC-1, OLED, relay | Duty-cycle pump controller with live countdown timers. |
| [ConfigStoreDemo](ConfigStoreDemo/) | DevKitC-1, OLED | Typed NVS configuration manager with serial CLI. |
| [StatusDashboardDemo](StatusDashboardDemo/) | DevKitC-1, OLED | Multi-task system telemetry dashboard with scrolling OLED. |
| [MoisturePump](MoisturePump/) | DevKitC-1, OLED, LM393, relay | Irrigation: MoistureMonitor + TimedCycleController. |
| [StatusPanel](StatusPanel/) | DevKitC-1, OLED, NeoPixel | System health: StatusDashboard + LedRGBController. |
| [MultiSensor](MultiSensor/) | DevKitC-1, OLED, 3 sensors | 3-channel analog sensor monitor. |

## Build

```bash
cd firmware/apps/<App>
uv run pio run --target upload
uv run pio run --target upload --target monitor
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

Each Demo app exercises a single service. Integration apps (MoisturePump, StatusPanel, MultiSensor) compose multiple services together.
