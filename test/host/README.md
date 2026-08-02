# test/host

Host-side test scripts that communicate with ESP32-S3 firmware over BLE.

## Prerequisites

- **Python 3.10+** with [uv](https://docs.astral.sh/uv/) package manager
- **ESP32-S3 hardware** with USB-C cable for flashing
- **Bluetooth enabled** on your laptop (System Settings > Bluetooth > On)
- **PlatformIO** (installed via `uv sync` at the repo root)

## Quick Start

```bash
# From repo root — install everything once
uv sync

# Flash the unified ApiBLE firmware
cd firmware/apps/ApiBLE
uv run pio run --target upload

# Run tests from anywhere in the workspace
uv run python test/host/test_api_ble.py --snapshot
```

> **Tip:** `uv run` can be invoked from any subdirectory. uv automatically walks up to find `pyproject.toml` and uses the workspace-root venv.

---

## ApiBLE Test Script (`test_api_ble.py`)

Unified test client for the **ApiBLE** firmware app. Communicates with a **nulllaborg ESP32-S3 CAM** board running the unified BLE firmware that combines Camera (CA00), GPIO (BA00), and Config (CF00) services.

The ESP32 advertises as **"ApiBLE"**.

### Flash the Firmware

```bash
cd firmware/apps/ApiBLE
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

### Python Test Client

```bash
# Camera
uv run python test/host/test_api_ble.py --snapshot              # single snapshot
uv run python test/host/test_api_ble.py --snapshot photo.jpg    # custom filename
uv run python test/host/test_api_ble.py --stream 5 --delay 0.5  # 5 captures, 0.5s apart
uv run python test/host/test_api_ble.py --flash                 # flash photo
uv run python test/host/test_api_ble.py --camera-settings       # read camera settings
uv run python test/host/test_api_ble.py --set-cam quality=10    # change setting
uv run python test/host/test_api_ble.py --cam-params            # valid settings schema

# GPIO
uv run python test/host/test_api_ble.py --pin 1:out             # configure pin 1 as output
uv run python test/host/test_api_ble.py --set-pin 1:1           # pin 1 HIGH
uv run python test/host/test_api_ble.py --get-pin 14            # read pin 14
uv run python test/host/test_api_ble.py --led red               # set LED color
uv run python test/host/test_api_ble.py --gpio-state            # read full GPIO state

# Config
uv run python test/host/test_api_ble.py --config tenant_id=abc,user_id=xyz
uv run python test/host/test_api_ble.py --show-config           # read all config
uv run python test/host/test_api_ble.py --schema                # read config schema
uv run python test/host/test_api_ble.py --clear-config          # clear all config
```

### BLE Protocol Reference

#### Camera Service (CA00)

| Char | UUID | Properties | Purpose |
|---|---|---|---|
| CA01 | `0000CA01-...` | Write | `snapshot`, `flash_on`, `flash_off` |
| CA02 | `0000CA02-...` | Write+Read | Settings string (`quality=10,vflip=0,...`) |
| CA03 | `0000CA03-...` | Notify | Chunked JPEG (240-byte chunks, 4-byte LE size header) |
| CA04 | `0000CA04-...` | Read | Frame info JSON: `{"size":N,"w":W,"h":H,"q":Q}` |
| CA05 | `0000CA05-...` | Read | Valid settings schema JSON |

#### GPIO Service (BA00)

| Char | UUID | Properties | Purpose |
|---|---|---|---|
| BA01 | `0000BA01-...` | Write | Commands: `pin=1:out`, `set=1:1`, `get=14`, `led=red` |
| BA02 | `0000BA02-...` | Read | Status: `led=off,pin1=1,pin14=512,...` |

Supported pins on ESP32-S3-CAM: **1, 14, 40, 41, 42** (40/41/42 are strapping pins — avoid external pull-down at boot).

#### Config Service (CF00)

| Char | UUID | Properties | Purpose |
|---|---|---|---|
| CF01 | `0000CF01-...` | Write | `key=val,key=val,...` partial updates |
| CF02 | `0000CF02-...` | Read | Full state string of all stored keys |
| CF03 | `0000CF03-...` | Read | Schema: `key:type:default,key2:type2:default2,...` |

Type auto-detection: `true`/`false` → bool, all-digits → int, digits+`.` → float, else → String.

### Architecture

```
Host Python (test_api_ble.py)
    └─ ApiBleClient ── bleak ── BLE radio
         └─ ApiBLE firmware (ESP32-S3)
              ├─ CameraBleModule  → CameraController → OV2640 sensor
              ├─ GpioBleModule    → StorageController → NVS
              └─ ConfigBleModule  → StorageController → NVS
```
