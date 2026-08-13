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
>
> **Photo speed:** The default BLE scan timeout is **3s** (override with `--timeout N`). Snapshot throughput is dominated by the JPEG-over-BLE transfer — see [BLE Throughput](#ble-throughput) below to tune it.

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

# BLE throughput tuning (persisted in firmware NVS)
uv run python test/host/test_api_ble.py --chunk-delay-ms 2      # ms between JPEG notify chunks
uv run python test/host/test_api_ble.py --chunk-size 240        # fixed chunk bytes; 0 = auto from MTU
uv run python test/host/test_api_ble.py --ble-mtu 517           # local ATT MTU cap (23-517)

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
| CA03 | `0000CA03-...` | Notify | Chunked JPEG (chunk_size-byte chunks, 4-byte LE size header) |
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

### BLE Throughput

Photo speed over BLE is dominated by the JPEG transfer, not the capture (the firmware pre-captures continuously at ~20 FPS into a buffered queue). Three firmware-side knobs — persistent, set over the air, stored in NVS (`"cam"` namespace):

| Setting | Default | Range | Meaning |
|---|---|---|---|
| `ble_mtu` | `517` | `23–517` | Local ATT MTU cap applied at startup (`BLEDevice::setMTU`). Each host negotiates its own MTU per connection (macOS/CoreBluetooth auto-negotiates). |
| `chunk_size` | `0` (auto) | `0–510` | JPEG chunk size in bytes. `0` auto-sizes to fit one notification: `mtu − 3 − 4` (reserving the 4-byte size header). A fixed value overrides auto-sizing. |
| `chunk_delay_ms` | `8` | `0–1000` | Delay between notified chunks. Lower = higher throughput; increase if the link drops chunks. |

Example — enable a large MTU and drop the inter-chunk delay:

```bash
uv run python test/host/test_api_ble.py --ble-mtu 517 --chunk-delay-ms 2
```

The client logs the negotiated ATT MTU on connect (`Connected: ... (ATT MTU: 247)`), and the firmware prints the computed chunk size to serial (`CAM BLE: negotiated MTU=... chunk_size=...`). Compare snapshot timing before/after:

```bash
time uv run python test/host/test_api_ble.py --snapshot
```

---

## Capturing a Fast-Moving Object

To freeze a fast-moving object you want a **short exposure** (low `shutter`) with enough light/sensitivity, then transfer the resulting frame over BLE quickly. The relevant camera settings (all persisted via NVS `"cam"`):

| Setting | For fast motion: | Notes |
|---|---|---|
| `aec` | `off` | Disable auto-exposure so manual `shutter`/`gain` take effect. |
| `shutter` | low (e.g. `200`–`600`) | Exposure in sensor units. Lower = shorter exposure = less motion blur, but darker. |
| `gain` | higher (e.g. `10`–`30`) | Sensor gain/ISO to compensate for the short exposure. Watch for noise. |
| `quality` | mid-range (e.g. `10`–`20`) | JPEG quality. Lower = smaller/ faster transfer; higher = more detail. |
| `size` | as needed | Larger = more captured detail but longer transfer; smaller = faster. |

You can also reduce transfer time with the throughput knobs above:
```bash
uv run python test/host/test_api_ble.py --ble-mtu 517 --chunk-delay-ms 2
```

### Recommended burst-capture command

Set a short exposure and burst files, saving each to its own JPEG:

```bash
# configure exposure once (persisted), then burst 10 frames as fast as allowed
uv run python test/host/test_api_ble.py \
  --set-cam aec=off,shutter=300,gain=20,quality=15 \
  --stream 10 --delay 0 --ble-mtu 517 --chunk-delay-ms 2
```

Notes:
- `--delay 0` captures as quickly as the transfer allows. A smaller `size`/`quality` and lower `chunk-delay-ms` make each frame finish faster, so the burst clears quicker between shots.
- A single "fast object, well-lit" shot needs only the `--set-cam` exposure plus a `--snapshot`:
  ```bash
  uv run python test/host/test_api_ble.py \
    --set-cam aec=off,shutter=200,gain=20,quality=12 --snapshot fast.jpg
  ```
- If motion blur persists, lower `shutter` further (e.g. `150`) and raise `gain`; if the shot is too dark/noisy, raise `gain` or `shutter` slightly.

Valid `size` values: `96x96`, `QQVGA`, `QCIF`, `HQVGA`, `240x240`, `QVGA` (default), `CIF`, `HVGA`, `VGA`, `SVGA`, `XGA`, `HD`, `SXGA`, `UXGA`, `FHD`, `QXGA`. Smaller sizes (e.g. `QVGA`/`VGA`) transfer fastest — best for high-speed bursts.

### Architecture

```
Host Python (test_api_ble.py)
    └─ ApiBleClient ── bleak ── BLE radio
         └─ ApiBLE firmware (ESP32-S3)
              ├─ CameraBleModule  → CameraController → OV2640 sensor
              ├─ GpioBleModule    → StorageController → NVS
              └─ ConfigBleModule  → StorageController → NVS
```
