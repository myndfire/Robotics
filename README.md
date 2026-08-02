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
├── firmware/                  ← ALL code flashed to ESP32-S3
│   ├── device_drivers/        ← Hardware abstraction libraries
│   │   ├── CameraController/    → OV2640 camera, DMA, JPEG encoding
│   │   ├── StorageController/  → NVS flash persistence
│   │   ├── LedRGBController/   → NeoPixel LED control
│   │   ├── DisplayController/  → OLED display (U8g2)
│   │   ├── GpioController/     → GPIO pin modes, digital/analog I/O
│   │   └── ...
│   ├── services/              ← Reusable firmware services
│   │   ├── CameraWebServer/   → WiFi HTTP camera server
│   │   └── ...
│   └── apps/                  ← Flashable firmware applications
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
    │   ├── tools/             → Agent tools (take_picture, describe_picture)
    │   │   ├── _debug.py      → Shared logging decorator
    │   │   ├── capture.py     → BLE camera capture tool
    │   │   └── describe.py    → Vision model description tool
    │   └── vision.py          → Vision agent (llava via ManagedAgent)
    ├── README.md
    └── .env.example
```

## Quick Start

### 1. Clone and enter workspace

```bash
git clone https://github.com/myndfire/Robotics.git
cd Robotics
```

### 2. Install PlatformIO (for firmware builds)

```bash
# From repo root — installs PlatformIO + dependencies
uv sync
```

### 3. Flash firmware

```bash
# Unified BLE app: Camera + GPIO + Config (nulllaborg ESP32-S3 CAM)
cd firmware/apps/ApiBLE
uv run pio run --target upload
```

### 4. Run tests from host

```bash
# From repo root — uv finds workspace automatically
uv run python test/host/test_api_ble.py --snapshot
uv run python test/host/test_api_ble.py --stream 3
uv run python test/host/test_api_ble.py --config tenant_id=abc
```

### 5. Run the AI camera agent

```bash
cd camera_agent
cp .env.example .env
# Edit .env: set CAMERA_AGENT_DEVICE, pull llava with ollama
uv sync
uv run python -m camera_agent.main
```

## Tiers

| Tier | Language | Location | Runs on | What it does |
|---|---|---|---|---|
| **Device drivers** | C++ | `firmware/device_drivers/` | ESP32-S3 | Hardware abstraction (camera, GPIO, LED, storage, display) |
| **Services** | C++ | `firmware/services/` | ESP32-S3 | Reusable protocols (BLE, HTTP, Serial, NVS settings) |
| **Apps** | C++ | `firmware/apps/` | ESP32-S3 | Complete flashable applications |
| **Host library** | Python | `python-libs/` | Laptop | BLE/Serial clients for firmware services |
| **Host tests** | Python | `test/host/` | Laptop | Integration tests against real hardware |
| **AI Agent** | Python | `camera_agent/` | Laptop | LLM-powered natural language control |

## Component Relationships

```
camera_agent (Python)
    └─ camera_agent.tools.take_picture
         └─ ApiBleClient
              └─ bleak (BLE GATT)
                   └─ ApiBLE (ESP32 firmware)
                        ├─ CameraBleModule → CameraController → OV2640 sensor
                        ├─ GpioBleModule → StorageController → NVS
                        └─ ConfigBleModule → StorageController → NVS
                        
camera_agent (Python)
    └─ camera_agent.tools.describe_picture
         └─ camera_agent.vision.describe_image
              └─ ManagedAgent + llava (Ollama)
              
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

### Why two models in camera_agent?

The conversational model (`gpt-oss:20b`) is text-only — it would hallucinate image descriptions. The vision model (`llava`) sees pixels but is weak at conversation. The agent composes them: text model decides when to capture/describe, vision model provides factual descriptions.

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
| `firmware/apps/ApiBLE/README.md` | Unified BLE app: Camera + GPIO + Config |
| `firmware/apps/README.md` | All flashable firmware apps |
| `firmware/services/README.md` | Reusable firmware services |
| `AGENTS.md` | Agent conventions: build steps, test procedures, code style |

## License

Apache 2.0 — See [LICENSE](LICENSE)
