# embedded-system-services

Host-side Python client library for ESP32-S3 embedded system firmware services. Provides a reusable async class for communicating with ESP32-S3 firmware over **BLE** (Bluetooth Low Energy).

## Install

```bash
# From PyPI (when published)
pip install embedded-system-services

# From local checkout (development)
pip install -e /path/to/Robotics/python-libs

# With uv (recommended)
cd /path/to/Robotics/python-libs
uv pip install -e .
```

## Modules Overview

| Module | Protocol | Firmware Service | What it controls |
|---|---|---|---|
| `api_ble` | BLE GATT | ApiBLE | Unified: Camera + GPIO + Config |

## Usage

### Unified BLE (`api_ble`) — Recommended

```python
import asyncio
from embedded_system_services import ApiBleClient

async def main():
    api = ApiBleClient(device_name="ApiBLE")

    await api.connect()

    # Camera
    await api.capture("photo.jpg")
    await api.set_camera_settings(quality=10)
    await api.flash_capture("night.jpg")

    # GPIO
    await api.configure_pin(1, "out")
    await api.set_pin(1, 1)
    value = await api.get_pin(14)

    # Config
    await api.set_config(tenant_id="abc", user_id="xyz")
    cfg = await api.get_config()
    print(cfg)

    await api.disconnect()

asyncio.run(main())
```

## API Reference

### `ApiBleClient`

| Method | Args | Returns | Description |
|---|---|---|---|
| `connect(name)` | `name: str = "ApiBLE"` | `None` | Scan and connect |
| `disconnect()` | — | `None` | Disconnect |
| `capture(path)` | `output_path: str` | `dict` | Take photo, save JPEG || `stream(count, delay, pattern)` | `count: int, delay_sec: float` | `list[dict]` | Capture N photos |
| `flash_capture(path)` | `output_path: str` | `dict` | Capture with flash LED |
| `get_camera_settings()` | — | `dict` | Read camera settings |
| `set_camera_settings(**kwargs)` | `quality, size, brightness, ...` | `None` | Update camera settings |
| `get_camera_params()` | — | `dict` | Read valid settings schema |
| `configure_pin(pin, mode)` | `pin: int, mode: str` | `None` | GPIO mode: `"out"`, `"in"`, `"ain"` |
| `set_pin(pin, value)` | `pin: int, value: int` | `None` | Digital write |
| `get_pin(pin)` | `pin: int` | `str` | Read pin value |
| `read_gpio_state()` | — | `dict` | Read full GPIO/LED state |
| `set_led(color)` | `color: str` | `None` | LED color or `"off"` |
| `get_config()` | — | `dict` | Read all config keys |
| `set_config(**kwargs)` | `key=value` | `None` | Set typed config keys |
| `get_config_schema()` | — | `str` | Read config schema |
| `reset_config()` | — | `None` | Clear all config |

## BLE Protocol Details

### Camera Service (`0000CA00-0000-1000-8000-00805F9B34FB`)

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control (CA01) | `...CA01` | Write | Commands: `snapshot`, `flash_on`, `flash_off`, `save`, `reset` |
| Settings (CA02) | `...CA02` | Write+Read | Key=value pairs: `quality=10,size=QVGA` |
| Frame (CA03) | `...CA03` | Notify | Chunked JPEG delivery (chunk_size-byte chunks) |
| Info (CA04) | `...CA04` | Read | Frame metadata: `{"size":N,"w":W,"h":H,"q":Q}` |
| Params (CA05) | `...CA05` | Read | Valid settings schema |

Frame chunking protocol:
1. First notification: `[4 bytes total_size LE]` + up to `chunk_size` bytes JPEG
2. Subsequent: up to `chunk_size` bytes continuation
3. Client accumulates until `total_size` reached

The firmware auto-sizes `chunk_size` to the negotiated ATT MTU (one notification per chunk) and exposes `ble_mtu`, `chunk_size`, and `chunk_delay_ms` as persisted camera settings (see [BLE Throughput](../test/host/README.md#ble-throughput)). Set them over the air via `set_camera_settings`:

```python
await api.set_camera_settings(ble_mtu=517, chunk_delay_ms=2)
```

### GPIO Service (`0000BA00-0000-1000-8000-00805F9B34FB`)

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control (BA01) | `...BA01` | Write | Commands: `led=red`, `pin=4:out`, `set=4:1`, `get=4` |
| Status (BA02) | `...BA02` | Read | State: `led=r;g;b,pin4=1,pin7=512` |

Multiple commands in one write (comma-separated):
```
led=green,pin=4:out,set=4:1
```

### Config Service (`0000CF00-0000-1000-8000-00805F9B34FB`)

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control (CF01) | `...CF01` | Write | `key=val,key=val,...` partial updates |
| State (CF02) | `...CF02` | Read | Full state string |
| Schema (CF03) | `...CF03` | Read | Schema: `key:type:default` |

Type auto-detection on first write:
- `"true"` / `"false"` → bool
- all digits → int
- digits + `.` → float
- else → String

## Testing

See `../test/host/` for integration tests:

```bash
cd ../test/host
uv run python test_api_ble.py --snapshot
uv run python test_api_ble.py --stream 3
uv run python test_api_ble.py --config tenant_id=abc
```

## Dependencies

- Python >= 3.11
- `bleak` >= 0.22 (BLE client, async)

## Related

- [Robotics root README](../README.md) — Full project overview
- [camera_agent README](../camera_agent/README.md) — AI agent that uses this library
- [test/host README](../test/host/README.md) — Detailed test documentation
- [Firmware device drivers](../firmware/device_drivers/) — C++ drivers this library talks to
