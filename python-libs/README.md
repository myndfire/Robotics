# embedded-system-services

Host-side Python client library for ESP32-S3 embedded system firmware services. Provides reusable classes for communicating with ESP32-S3 firmware over BLE.

## Install

```bash
pip install -e /path/to/Robotics/python-libs
```

Or with uv:

```bash
cd python-libs
uv pip install -e .
```

## Usage

```python
from embedded_system_services import CameraBleClient, BleApiClient
import asyncio

async def main():
    # Camera snapshot
    cam = CameraBleClient()
    await cam.connect()
    await cam.capture("photo.jpg")
    await cam.disconnect()

    # GPIO control
    api = BleApiClient()
    await api.connect()
    await api.configure_pin(4, "out")
    await api.set_pin(4, 1)
    await api.set_led("green")
    await api.disconnect()

asyncio.run(main())
```

## Modules

### `embedded_system_services.camera_ble`

BLE client for the CameraBluetoothServer firmware service (`firmware/device_drivers/CameraBluetoothServer/`).

| Method | Description |
|---|---|
| `connect(name=None)` | Scan for and connect to the BLE camera device. Default: searches for "ESP32-CAM". |
| `disconnect()` | Disconnect from the device. |
| `capture(output_path)` | Take a single photo. Sends "snapshot" over BLE, receives chunked JPEG, saves to disk. |
| `stream(count, delay_sec, output_pattern)` | Capture multiple photos with configurable delay. |
| `get_settings()` | Read all camera settings as a dict (quality, brightness, contrast, etc.). |
| `set_settings(**settings)` | Update camera settings. Changes take effect on next capture. |
| `get_params()` | Get valid setting keys, types, and ranges from the device. |
| `flash_capture(output_path)` | Take a photo with the flash LED enabled. |
| `save_settings()` | Persist settings to NVS flash on the ESP32. |
| `reset()` | Factory reset — wipe all NVS and reboot the ESP32. |

### `embedded_system_services.ble_api`

BLE client for the BleApiServer firmware service (`firmware/services/BleApiServer/`).

| Method | Description |
|---|---|
| `connect(name=None)` | Scan for and connect to the BLE API device. Default: "ESP32-API". |
| `disconnect()` | Disconnect from the device. |
| `read_state()` | Read full device state (LED color, pin values) from the Status characteristic. |
| `configure_pin(pin, mode)` | Set GPIO pin mode: "out", "in", or "ain". |
| `set_pin(pin, value)` | Digital write to a GPIO pin (0 or 1). |
| `get_pin(pin)` | Read a GPIO pin's value. |
| `set_led(color)` | Set NeoPixel color: red, green, blue, cyan, magenta, yellow, white, on, off. |
| `configure_pins(**pins)` | Configure multiple GPIO pins at once. |

## Firmware Services

These client classes communicate with firmware services running on ESP32-S3 hardware:

| Python Client | Firmware Service | Location |
|---|---|---|
| `CameraBleClient` | CameraBluetoothServer | `../firmware/device_drivers/CameraBluetoothServer/` |
| `BleApiClient` | BleApiServer | `../firmware/services/BleApiServer/` |

For integration tests that use these clients, see `../test/host/`.

## Dependencies

- Python >= 3.10
- `bleak` >= 0.22 (BLE client library)
