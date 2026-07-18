# CameraBLE

Flashable firmware app running `CameraBluetoothServer` from `firmware/device_drivers/` on a nulllaborg ESP32-S3 CAM board. Advertises as **"ESP32-CAM"** over BLE and serves JPEG snapshots via chunked BLE notifications.

## Hardware

| Component | Details |
|---|---|
| Board | nulllaborg ESP32-S3 CAM |
| Camera | OV2640 or OV3660 (auto-detected) |
| Flash LED | GPIO 3 |
| Serial | USB-C, 921600 baud |

## Setup

```bash
cd firmware/apps/CameraBLE
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

The serial monitor shows:

```
Camera: OK | QVGA 320x240 JPEG quality=20
BLE: Camera service started
CameraBluetoothServer ready:
  Advertising as "ESP32-CAM"
```

## BLE Protocol

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control (CA01) | `0000CA01-...-00805F9B34FB` | Write | Commands: `snapshot`, `flash_on`, `flash_off`, `save`, `reset` |
| Settings (CA02) | `0000CA02-...` | Write+Read | Camera settings: `quality=20,size=QVGA,brightness=1,...` |
| Frame (CA03) | `0000CA03-...` | Notify | Chunked JPEG delivery (240-byte chunks) |
| Info (CA04) | `0000CA04-...` | Read | Frame metadata: `{"size":N,"w":W,"h":H,"q":Q}` |
| Params (CA05) | `0000CA05-...` | Read | JSON schema of valid settings keys and ranges |

## Testing

Use the host-side test script:

```bash
pip install -e ./python-libs
python test/host/test_camera_ble.py
python test/host/test_camera_ble.py --stream 3 --delay 2
python test/host/test_camera_ble.py --settings
```

See `test/host/README.md` for full documentation.
