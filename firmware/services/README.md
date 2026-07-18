# firmware/services

Reusable firmware service **libraries** that compose hardware drivers from `firmware/device_drivers/`. Each service provides a clean C++ API and a `library.json` manifest for PlatformIO dependency resolution.

Services are **NOT flashable** — they have no `main.cpp` or `platformio.ini`. Use the apps in `firmware/apps/` to flash and run them.

## Service Index

| Service | Composes | Description |
|---|---|---|
| [BleApiServer](BleApiServer/) | BLE stack, LedRGBController, StorageController | BLE peripheral for GPIO control and NeoPixel LED. Full pin registry with NVS persistence. Advertises as ESP32-API. |
| [CameraWebServer](CameraWebServer/) | CameraController, WiFiManager, WebServer | WiFi-connected MJPEG camera web server with captive portal. Dark-theme dashboard, snapshot, and live streaming. |
| [MoistureMonitor](MoistureMonitor/) | MoistureSensorController, DisplayController, StorageController | Soil moisture monitoring with DRY/WET/OK status. FreeRTOS sensor + display tasks, NVS-persisted calibration. |
| [TimedCycleController](TimedCycleController/) | RelayController, DisplayController, StorageController | Duty-cycle pump/solenoid controller. Configurable ON/OFF durations persisted to NVS, live OLED countdowns. |
| [ConfigStore](ConfigStore/) | StorageController, DisplayController | Typed NVS configuration manager (String, int, bool, float). Serial command interface with OLED live summary. |
| [StatusDashboard](StatusDashboard/) | DisplayController, WiFi | Multi-task system telemetry dashboard with scrolling OLED. Uptime, heap, PSRAM, WiFi RSSI, SDK version. |

## Architecture

```
firmware/apps/      ──imports──▶  firmware/services/  ──imports──▶  firmware/device_drivers/
     (flashable apps)                (reusable libraries)               (hardware drivers)
```

## Including in Your Project

### Within this repo

```ini
lib_extra_dirs =
    ../services
    ../device_drivers
```

### External project

```bash
git clone https://github.com/yourorg/Robotics.git /path/to/Robotics
```

```ini
lib_extra_dirs =
    /path/to/Robotics/firmware/services
    /path/to/Robotics/firmware/device_drivers
```

## Host Services

BleApiServer communicates with the host-side Python client library:

| Firmware Service | Python Client | Module |
|---|---|---|
| BleApiServer | BleApiClient | `from embedded_system_services.ble_api import BleApiClient` |
| CameraBluetoothServer (in `device_drivers/`) | CameraBleClient | `from embedded_system_services.camera_ble import CameraBleClient` |

Install: `pip install -e ./python-libs`. See `test/host/` for usage examples.

## Each Service Contains

| File | Purpose |
|---|---|
| `src/ServiceName.h` | Public class API |
| `src/ServiceName.cpp` | Full implementation |
| `library.json` | PlatformIO manifest — name, version, dependencies |
| `README.md` | API documentation, wiring, expected behavior |
