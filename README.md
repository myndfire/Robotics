# Robotics

Robotics and Embedded Systems — firmware drivers, services, apps, and host-side tools for the Espressif ESP32-S3 platform.

## What is this?

A complete embedded-to-cloud stack for ESP32-S3 projects:

- **Firmware** (C++) — hardware drivers, reusable services, flashable apps
- **Python Libraries** — host-side BLE/Serial clients that talk to the firmware
- **Tests** — host-side integration tests against real hardware
- **AI Agent** — natural language control via LLM + BLE

## Repository Structure

```
Robotics/
├── README.md
├── AGENTS.md                  ← Agent-focused conventions and build notes
├── firmware/                  ← Firmware libraries and flashable apps
│   ├── device_drivers/        ← Hardware abstraction libraries
│   │   ├── CameraController/    → OV2640 camera, DMA, JPEG encoding
│   │   ├── StorageController/  → NVS flash persistence
│   │   ├── LedRGBController/   → NeoPixel LED control
│   │   ├── DisplayController/  → OLED display (U8g2)
│   │   ├── GpioController/     → GPIO pin modes, digital/analog I/O
│   │   └── ...
│   ├── services/              ← Reusable service libraries (linked into apps)
│   │   ├── CameraWebServer/   → WiFi HTTP camera server
│   │   └── ...
│   └── apps/                  ← Flashable firmware images
│       ├── ApiBLE/            → Unified BLE app: Camera + GPIO + Config
│       └── ...
│
├── python-libs/               ← Host-side Python library
│   └── embedded_system_services/
│       └── api_ble/           → ApiBleClient (BLE, unified Camera + GPIO + Config)
│
├── test/
│   └── host/                  ← Integration tests
│       └── test_api_ble.py    → Unified ApiBLE tests
│
└── camera_agent/              ← AI agent (natural language control)
    ├── src/camera_agent/
    │   ├── main.py            → CLI entry point
    │   ├── agent_factory.py   → ManagedAgent with camera tools
    │   ├── tools/             → Agent tools (capture, describe)
    │   │   ├── _debug.py      → Shared logging decorator
    │   │   ├── capture.py     → BLE camera capture tool
    │   │   └── describe.py    → Vision model description tool
    │   └── vision.py          → Vision agent (Qwen3-VL via ManagedAgent)
    ├── README.md
    └── .env.example
```

## Quick Start

Each block starts from the repository root. There is no root Python environment.

If another component's virtualenv is active, run `deactivate` first. `uv run` uses the `pyproject.toml` in the folder you enter.

### 1. Clone the repository

```bash
git clone https://github.com/myndfire/Robotics.git
cd Robotics
```

### 2. Install PlatformIO once

```bash
uv tool install platformio==6.1.19
```

### 3. Flash firmware

```bash
# Unified BLE app: Camera + GPIO + Config (nulllaborg ESP32-S3 CAM)
cd firmware/apps/ApiBLE
pio run --target upload
```

### 4. Run host tests

```bash
cd test/host
uv sync
uv run python test_api_ble.py --snapshot
uv run python test_api_ble.py --stream 3
uv run python test_api_ble.py --config tenant_id=abc
```

### 5. Run the AI camera agent

```bash
cd camera_agent
cp .env.example .env
# Edit .env: set CAMERA_AGENT_DEVICE and the vLLM endpoint/model
uv sync
uv run python -m camera_agent.main
```

## Tiers

| Tier | Language | Location | Runs on | What it does |
|---|---|---|---|---|
| **Device drivers** | C++ | `firmware/device_drivers/` | Linked into an app image | Hardware abstraction (camera, GPIO, LED, storage, display) |
| **Services** | C++ | `firmware/services/` | Linked into an app image | Reusable protocols (BLE, HTTP, Serial, NVS settings) |
| **Apps** | C++ | `firmware/apps/` | ESP32-S3 | Complete flashable applications |
| **Host library** | Python | `python-libs/` | Laptop | BLE/Serial clients for firmware services |
| **Host tests** | Python | `test/host/` | Laptop | Integration tests against real hardware |
| **AI Agent** | Python | `camera_agent/` | Laptop | LLM-powered natural language control |

Only an **app image** runs on the device. Drivers and services are libraries compiled into an app at build time — they are never flashed or run standalone. The default `ApiBLE` app is self-contained: its Camera (CA00), GPIO (BA00), and Config (CF00) modules live in `firmware/apps/ApiBLE/src`, and it does not link any library from `firmware/services/`.

| App | Services used |
|---|---|
| `ApiBLE` | none (app-local BLE modules) |
| `CameraWebServerDemo` | `CameraWebServer` |
| `MoistureMonitorDemo` | `MoistureMonitor` |
| `TimedCycleControllerDemo` | `TimedCycleController` |
| `StatusDashboardDemo` | `StatusDashboard` |
| `MoisturePump` | `MoistureMonitor`, `TimedCycleController` |
| `StatusPanel` | `StatusDashboard` |
| `MultiSensor` | none (device drivers only) |

## Component Relationships

```
camera_agent (Python)
    └─ camera_agent.tools.take_picture / take_pictures
         └─ ApiBleClient
              └─ bleak (BLE GATT)
                   └─ ApiBLE (ESP32 firmware)
                        ├─ CameraBleModule → CameraController → OV2640 sensor
                        ├─ GpioBleModule → StorageController → NVS
                        └─ ConfigBleModule → StorageController → NVS
                        
camera_agent (Python)
    └─ camera_agent.tools.describe_picture
         └─ camera_agent.vision.describe_image
              └─ ManagedAgent + Qwen3-VL (vLLM)
              
test/host/test_api_ble.py (Python)
    └─ ApiBleClient
         └─ bleak (BLE GATT)
              └─ ApiBLE (ESP32 firmware)
                   ├─ CameraBleModule → CameraController → OV2640
                   ├─ GpioBleModule → GPIO pins
                   └─ ConfigBleModule → StorageController → NVS
```

## Hardware

| Board | Flash | PSRAM | Special Features |
|---|---|---|---|
| **ESP32-S3 DevKitC-1** | 8 MB | 8 MB | Built-in NeoPixel (GPIO 48) |
| **nulllaborg ESP32-S3 CAM** | 8 MB | 8 MB | OV2640/OV3660 camera, flash LED (GPIO 3) |
| **DORHEA Mini** | 4 MB | 2 MB | External NeoPixel (GPIO 21), compact |

## Key Design Decisions

### Why two agents in camera_agent?

The conversational agent decides when to capture/describe and calls tools. A separate vision agent receives the JPEG pixels, so image descriptions never depend on the text model guessing. Both roles default to the multimodal `Qwen3-VL-4B-Instruct-4bit` served by vLLM, but the vision agent can be pointed at a different endpoint or model via `CAMERA_AGENT_VISION_*`.

### Why separate device drivers and services?

Drivers own hardware (single responsibility). Services own protocols (BLE, HTTP, Serial). Apps compose both. This makes drivers reusable across different apps (e.g., `CameraController` used by both `ApiBLE` and `CameraWebServer`).

### Why Python host libraries?

The firmware exposes standard protocols (BLE GATT, HTTP, Serial). Python clients let you test, script, and build agents without writing C++. The `camera_agent` is an example of what's possible.

## External Project Usage

### As firmware dependencies

```ini
; platformio.ini
lib_extra_dirs =
    /path/to/Robotics/firmware/device_drivers
    /path/to/Robotics/firmware/services
```

### As Python library

```bash
pip install -e /path/to/Robotics/python-libs
```

```python
from embedded_system_services import ApiBleClient
```

## More Documentation

| Document | What's inside |
|---|---|
| `camera_agent/README.md` | Natural language camera agent: architecture, setup, usage, troubleshooting |
| `test/host/README.md` | Integration tests for all firmware services: BLE camera, GPIO, serial config |
| [`firmware/ARCHITECTURE.md`](firmware/ARCHITECTURE.md) | Firmware platform architecture and ApiBLE design (BLE, camera, GPIO, config) |
| `firmware/apps/ApiBLE/README.md` | Unified BLE app: Camera + GPIO + Config |
| `firmware/apps/README.md` | All flashable firmware apps |
| `firmware/services/README.md` | Reusable firmware services |
| `AGENTS.md` | Agent conventions: build steps, test procedures, code style |

## License

Apache 2.0 — See [LICENSE](LICENSE)
