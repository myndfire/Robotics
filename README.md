# Robotics

Robotics and Embedded Systems — firmware drivers, services, apps, and host-side tools for the Espressif ESP32-S3 platform.

## Repository Structure

```
Robotics/
├── README.md
├── firmware/                              ← ALL code flashed to ESP32-S3
│   ├── device_drivers/                    ← Hardware driver libraries
│   │   ├── README.md
│   │   ├── AdcController/
│   │   ├── CameraBluetoothServer/
│   │   ├── CameraController/
│   │   ├── DisplayController/
│   │   ├── LedRGBController/
│   │   ├── MoistureSensorController/
│   │   ├── RelayController/
│   │   └── StorageController/
│   ├── services/                          ← Firmware service libraries
│   │   ├── README.md
│   │   ├── BleApiServer/
│   │   ├── CameraWebServer/
│   │   ├── ConfigStore/
│   │   ├── MoistureMonitor/
│   │   ├── StatusDashboard/
│   │   └── TimedCycleController/
│   └── apps/                              ← Flashable firmware applications
│       ├── README.md
│       ├── CameraBLE/
│       ├── BleApiServerDemo/
│       ├── CameraWebServerDemo/
│       ├── ConfigStoreDemo/
│       ├── MoistureMonitorDemo/
│       ├── MoisturePump/
│       ├── MultiSensor/
│       ├── StatusDashboardDemo/
│       ├── StatusPanel/
│       └── TimedCycleControllerDemo/
├── python-libs/                           ← Host-side Python client library
│   └── embedded_system_services/
│       ├── camera_ble/                    ← CameraBleClient
│       └── ble_api/                       ← BleApiClient
└── test/
    └── host/                              ← Host-side test scripts
        ├── README.md
        ├── test_camera_ble.py
        └── test_ble_api.py
```

## Tiers

| Tier | Language | Location | Runs on | Flashable? |
|---|---|---|---|---|
| Device drivers | C++ | `firmware/device_drivers/` | ESP32-S3 | No — libraries only |
| Services | C++ | `firmware/services/` | ESP32-S3 | No — libraries only |
| Apps | C++ | `firmware/apps/` | ESP32-S3 | Yes — flashable |
| Host library | Python | `python-libs/` | Laptop | Installable via pip |
| Host tests | Python | `test/host/` | Laptop | Scripts |

## Quick Start

### Flash and run firmware

```bash
# BLE camera server (nulllaborg ESP32-S3 CAM)
cd firmware/apps/CameraBLE
uv run pio run --target upload

# BLE GPIO + LED server (ESP32-S3 DevKitC-1)
cd firmware/apps/BleApiServerDemo
uv run pio run -e esp32-s3-devkitc-1 --target upload
```

### Run host tests

```bash
cd test/host
uv sync                           # installs deps (once)
uv run python test_camera_ble.py --stream 3
uv run python test_ble_api.py --led green
```

## External Project Usage

```bash
git clone https://github.com/yourorg/Robotics.git /path/to/Robotics
```

```ini
# platformio.ini — firmware project
lib_extra_dirs =
    /path/to/Robotics/firmware/device_drivers
    /path/to/Robotics/firmware/services
```

```python
# Python project
# pip install -e /path/to/Robotics/python-libs
# -- or from test/host/ --
# cd test/host && uv sync
from embedded_system_services import CameraBleClient, BleApiClient
```

## Hardware

Target boards:
- **ESP32-S3 DevKitC-1** — 8 MB Flash, 8 MB PSRAM, built-in NeoPixel (GPIO 48)
- **nulllaborg ESP32-S3 CAM** — camera board with OV2640/OV3660, 8 MB Flash, 8 MB PSRAM
- **DORHEA Mini** — compact ESP32-S3 variant with external NeoPixel (GPIO 21)
