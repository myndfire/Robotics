# test/host

Host-side test scripts that communicate with ESP32-S3 firmware services over BLE.

## Prerequisites

- **Python 3.10+** with [uv](https://docs.astral.sh/uv/) package manager
- **ESP32-S3 hardware** with USB-C cable for flashing
- **Bluetooth enabled** on your laptop (System Settings > Bluetooth > On)
- **PlatformIO** (installed via `uv sync` at the repo root — see below)

> **Note on `pip`:** The root `pyproject.toml` includes `pip` in its `[dependency-groups] dev` list. This is required because PlatformIO's `tool-esptoolpy` package runs a post-install script that calls `pip` to install Python dependencies (`cryptography`, `ecdsa`, `bitstring`, `reedsolo`, `intelhex`). Without `pip` in the uv venv, this script fails and corrupts the PlatformIO package cache, leading to `MissingPackageManifestError`.

## Quick Start

```bash
# From repo root — install everything once (root + test/host deps)
uv sync

# Flash firmware from its app directory (or anywhere — uv finds the workspace root)
cd firmware/apps/CameraBLE
uv run pio run --target upload

# Run tests from test/host (or anywhere — uv finds the workspace root)
cd ../../test/host
uv run python test_camera_ble.py --stream 3
```

> **Tip:** `uv run` can be invoked from any subdirectory of the workspace. uv automatically walks up the directory tree to find `pyproject.toml` and uses the workspace-root venv. You don't need to `cd` to specific directories to run commands.

---

## Camera BLE (`CameraBLE`)

Communicates with the **CameraBluetoothServer** firmware driver running on a **nulllaborg ESP32-S3 CAM** board (OV2640/OV3660 camera). The ESP32 advertises as **"ESP32-CAM"** and delivers JPEG snapshots over BLE in chunked 240-byte notifications.

**Service UUID:** `0000CA00-0000-1000-8000-00805F9B34FB`

### Flash the Firmware

```bash
cd firmware/apps/CameraBLE
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

Single build environment (`esp32-s3-cam`) with QIO flash, OPI PSRAM, 8 MB flash, and `default_8MB.csv` partitions.

### Python Test Client

*Run from `test/host/` directory.*

```bash
# Single snapshot → saved as snapshot.jpg
uv run python test_camera_ble.py

# Custom output filename
uv run python test_camera_ble.py --output my_photo.jpg

# Stream mode — capture 5 snapshots, 500ms apart (2 fps)
uv run python test_camera_ble.py --stream 5 --delay 0.5

# Read all current camera settings as JSON
uv run python test_camera_ble.py --settings

# Update one or more settings (comma-separated partial updates)
uv run python test_camera_ble.py --set "quality=10"
uv run python test_camera_ble.py --set "brightness=1,contrast=1,saturation=1"
uv run python test_camera_ble.py --set "size=VGA,wb=sunny"
uv run python test_camera_ble.py --set "vflip=0,hflip=1"          # disable vflip, enable hflip
uv run python test_camera_ble.py --set "effect=2"                  # black & white
uv run python test_camera_ble.py --set "aec=off,shutter=500,gain=10"  # manual exposure

# Change settings, then capture and persist to NVS
uv run python test_camera_ble.py --set "quality=10" --save

# Photo with flash LED
uv run python test_camera_ble.py --flash
uv run python test_camera_ble.py --flash --output night_shot.jpg

# Discover all valid settings keys, types, and ranges
uv run python test_camera_ble.py --params

# Factory reset — wipes camera settings + WiFi credentials, reboots
uv run python test_camera_ble.py --reset
```

### What `uv run python test_camera_ble.py --stream 3` does

This command runs the host-side BLE test client that communicates with the **CameraBluetoothServer** firmware on your ESP32-S3 CAM. Here's the full flow:

1. **BLE Scan** — Scans for nearby BLE peripherals advertising the name `ESP32-CAM` (default timeout: 10 seconds).
2. **Connect & Discover** — Connects to the ESP32 and discovers the Camera Service (`0000CA00-...`) and its characteristics:
   - **CA01** Control (write)
   - **CA02** Settings (read/write)
   - **CA03** Frame (notify)
   - **CA04** Info (read)
   - **CA05** Params (read)
3. **Send Snapshot Command** — Writes `"snapshot"` to the **CA01** Control characteristic. The firmware's `camera_task` (Core 0) captures a fresh JPEG frame and queues it.
4. **Receive Chunked JPEG over BLE** — The firmware sends the JPEG via **CA03** notifications in 240-byte chunks:
   - **Notification 1**: `[4 bytes: total size LE]` + `[up to 240 bytes: first data]`
   - **Notifications 2+**: `[up to 240 bytes: continuation data]`
5. **Reassemble & Save** — The client accumulates chunks until `total_size` bytes are received, then saves the JPEG.
6. **Repeat 3 times** — Because of `--stream 3`, steps 3–5 repeat 3 times with a 1-second delay between shots (default `--delay`).
7. **Output files** — Saves each snapshot as `snapshot_000.jpg`, `snapshot_001.jpg`, `snapshot_002.jpg`.

> **Tip:** You can run this from any subdirectory of the workspace. `uv run` walks up to the repo root and uses the workspace venv.

### Web Bluetooth (Browser) Compatibility

The CameraBLE firmware is also accessible from **Web Bluetooth**-capable browsers (Chrome, Edge, Opera on macOS/Windows/Android). The firmware explicitly includes the **BLE2902 CCCD descriptor** on the CA03 Frame characteristic, which is required by the Web Bluetooth API to enable notifications via `characteristic.startNotifications()`.

```javascript
// Browser example (Web Bluetooth API)
const service = await server.getPrimaryService('0000ca00-0000-1000-8000-00805f9b34fb');
const frameChar = await service.getCharacteristic('0000ca03-0000-1000-8000-00805f9b34fb');
await frameChar.startNotifications();
frameChar.addEventListener('characteristicvaluechanged', (event) => {
    const data = event.target.value;
    // Reassemble JPEG chunks...
});
```

> **Note:** Safari and Firefox do not support Web Bluetooth. Use Chrome or Edge. iOS (15.4+) supports Web Bluetooth in third-party browsers using the `WKWebView` entitlement.

The test client scans for BLE devices advertising as `ESP32-CAM` and reassembles JPEG frames from the chunked BLE notification protocol (first notification: 4-byte LE total_size header + up to 240 bytes data; subsequent notifications: up to 240 bytes continuation data).

### What `test_camera_ble.py` Tests (Indirectly)

`test_camera_ble.py` communicates over BLE — it never imports firmware C++ code.
However, many test scenarios exercise internal device drivers inside the ESP32:

| Test Flag | BLE Characteristic | Firmware Driver Involved | What It Tests |
|---|---|---|---|
| `--save` | CA01 (Control) | `StorageController` | NVS write persistence |
| `--reset` | CA01 (Control) | `StorageController` | NVS erase + reboot |
| `--settings` | CA02 (Settings) | `CameraController` | Current sensor config |
| `--set` | CA02 (Settings) | `CameraController` | Sensor register writes |
| `--flash` | CA01 (Control) | `LedRGBController` | Flash LED on/off timing |
| *(default)* | CA03 (Frame) | `CameraController` | DMA capture → JPEG encode → PSRAM |

The dependency chain is:

```
test_camera_ble.py (Python host)
  └─ CameraBleClient (Python)
       └─ bleak / BLE GATT
            └─ CameraBluetoothServer (ESP32 firmware)
                 ├─ CameraController  → OV2640 sensor + DMA + JPEG
                 ├─ StorageController → NVS persistence
                 └─ LedRGBController  → Flash LED
```

### Settings Reference

| Setting Key | Type | Example Values | Description |
|---|---|---|---|
| `quality` | int (0-63) | `10` | JPEG quality. 0 = best (largest file), 63 = worst. |
| `size` | string | `QVGA`, `VGA`, `SVGA`, `XGA`, `HD` | Frame resolution. Changing triggers DMA pool reallocation (~50-200ms). |
| `brightness` | int (-2..2) | `1` | Image brightness. -2=darkest, 2=brightest. Instant. |
| `contrast` | int (-2..2) | `1` | Image contrast. Instant sensor register write. |
| `saturation` | int (-2..2) | `1` | Color saturation. -2=grayscale, 2=max. Instant. |
| `effect` | int (0-6) | `2` | 0=off, 1=neg, 2=b&w, 3=red, 4=green, 5=blue, 6=sepia |
| `vflip` | int (0/1) | `1` | Vertical flip. 1 = enabled (default). |
| `hflip` | int (0/1) | `0` | Horizontal flip. 0 = disabled (default). |
| `wb` | string | `auto`, `sunny`, `cloudy`, `office`, `home` | White balance preset. |
| `aec` | string | `on`, `off` | Auto-exposure control. Turn `off` for manual `shutter`/`gain`. |
| `shutter` | int (0-1200) | `0` | Manual exposure time. Only when `aec=off`. |
| `gain` | int (0-30) | `0` | Manual analog gain. Only when `aec=off`. Higher = brighter but noisier. |
| `flash_gain` | int (5-30) | `20` | Analog gain applied during flash photography. 5 = darkest/cleanest, 30 = brightest/noisiest. |
| `flash_shutter` | int (200-1200) | `800` | Exposure time during flash photography. Higher = brighter, more motion blur. |

Use `--params` to see the complete schema from the device, including valid enum values and ranges.

### Camera Settings Cookbook

```bash
# Indoor fluorescent lighting — neutral, moderate quality
uv run python test_camera_ble.py --set "brightness=0,contrast=0,wb=office,quality=8" --save

# Outdoor bright sunlight — warm white balance, high quality
uv run python test_camera_ble.py --set "brightness=-1,contrast=1,wb=sunny,quality=5" --save

# Black & white security camera — low quality, small file
uv run python test_camera_ble.py --set "effect=2,quality=30,size=QVGA" --save

# Maximum quality shot
uv run python test_camera_ble.py --set "quality=0,size=VGA,brightness=0,contrast=0,saturation=0,wb=auto"

# Low-light manual exposure (flash + manual gain)
uv run python test_camera_ble.py --set "aec=off,shutter=1000,gain=20" --flash
```

### All CLI Arguments

| Flag | Type | Default | Description |
|---|---|---|---|
| `--timeout SEC` | float | 10.0 | BLE scan timeout in seconds. Increase if device is far away. |
| `--settings` | flag | — | Read and print all current camera settings as JSON |
| `--set KEY=VAL,...` | string | — | Update camera settings. Comma-separated key=value pairs. |
| `--save` | flag | — | Persist current settings to NVS flash (survives power cycles) |
| `--flash` | flag | — | Take a photo with flash LED enabled |
| `--reset` | flag | — | Factory reset — wipe NVS, reboot ESP32 |
| `--params` | flag | — | Show valid settings keys, types, and ranges |
| `--stream N` | int | 0 | Number of snapshots to capture in sequence |
| `--delay SEC` | float | 1.0 | Delay between stream captures |
| `--output PATH` | string | snapshot.jpg | Output file path for single captures |

Default (no action flags): captures a single snapshot and saves to `snapshot.jpg`.

---

## BLE API (`BleApiServerDemo`)

Communicates with the **BleApiServer** firmware service running on an **ESP32-S3 DevKitC-1** (or DORHEA Mini). The ESP32 advertises as **"ESP32-API"** and exposes GPIO pins and a NeoPixel LED over BLE. GPIO pin modes and values can be optionally persisted to NVS.

**Service UUID:** `0000BA00-0000-1000-8000-00805F9B34FB`

### Flash the Firmware

```bash
cd firmware/apps/BleApiServerDemo

# ESP32-S3 DevKitC-1 (default — NeoPixel GPIO 48, NEO_GRB)
uv run pio run -e esp32-s3-devkitc-1 --target upload
uv run pio run -e esp32-s3-devkitc-1 --target upload --target monitor

# DORHEA Mini (NeoPixel GPIO 21, NEO_RGB)
uv run pio run -e esp32-s3-dorhea --target upload
uv run pio run -e esp32-s3-dorhea --target upload --target monitor
```

Two build environments are defined in `platformio.ini`. The `-DDORHEA_BOARD` flag selects the correct NeoPixel pin and colour order.

### Python Test Client

*Run from `test/host/` directory.*

```bash
# Read current state (LED colour + all GPIO pin values)
uv run python test_ble_api.py

# Configure GPIO 4 as output, then set it HIGH
uv run python test_ble_api.py --config "pin=4:out"
uv run python test_ble_api.py --set 4:1
uv run python test_ble_api.py --set 4:0        # set LOW

# Configure multiple pins at once
uv run python test_ble_api.py --config "pin=4:out,pin=7:in"

# Configure GPIO 7 as analog input, then read it
uv run python test_ble_api.py --config "pin=7:ain"
uv run python test_ble_api.py --get 7

# Persist GPIO config and value across reboots
uv run python test_ble_api.py --config "pin=4:out:persist"
uv run python test_ble_api.py --set 4:1:persist

# LED control
uv run python test_ble_api.py --led red
uv run python test_ble_api.py --led green
uv run python test_ble_api.py --led blue
uv run python test_ble_api.py --led cyan
uv run python test_ble_api.py --led magenta
uv run python test_ble_api.py --led yellow
uv run python test_ble_api.py --led white
uv run python test_ble_api.py --led off
uv run python test_ble_api.py --led on        # alias for green
```

The test client scans for BLE devices advertising as `ESP32-API`, connects, and interacts via GATT service UUID `0000BA00-...`. All commands write to the **BA01** Control characteristic and read state from the **BA02** Status characteristic.

### Pin Mode `:persist` Suffix

Adding `:persist` to `pin=` or `set=` commands saves the configuration to NVS (`"bluetooth_api"` namespace). On next boot:

```bash
# Configure GPIO 4 as output, then set HIGH — all persisted
uv run python test_ble_api.py --config "pin=4:out:persist"
uv run python test_ble_api.py --set 4:1:persist

# Power cycle the ESP32, then read state — GPIO 4 is still HIGH
uv run python test_ble_api.py
# Output: GPIO 4: 1
```

Without `:persist`, pin configuration is lost on reboot.

### GPIO Modes

| Mode | Description | Uses |
|---|---|---|
| `out` | Digital output (LOW by default) | Driving LEDs, relays |
| `in` | Digital input with internal pull-up | Reading buttons, switches |
| `ain` | Analog input (12-bit, 0-4095) | Reading sensors, potentiometers |

### Runtime LED Reconfiguration

```bash
# Remap NeoPixel to a different GPIO at runtime
uv run python test_ble_api.py --config "onboard_RGBLed_pin=21"

# Change colour byte order at runtime
uv run python test_ble_api.py --config "onboard_RGBLed_type=rgb"
uv run python test_ble_api.py --config "onboard_RGBLed_type=grb"
uv run python test_ble_api.py --config "onboard_RGBLed_type=gbr"
```

These settings are automatically persisted to NVS.

### All CLI Arguments

| Flag | Type | Default | Description |
|---|---|---|---|
| `--timeout SEC` | float | 10.0 | BLE scan timeout in seconds |
| `--config STR` | string | — | Configure GPIO pin modes. Format: `"pin=4:out,pin=7:in"`. Add `:persist` to save to NVS. |
| `--set PIN:VAL` | string | — | Digital write. Format: `"4:1"` (HIGH) or `"4:0"` (LOW). Add `:persist` to save to NVS. |
| `--get PIN` | string | — | Read a GPIO pin. Format: `"7"`. Returns digital or analog based on configured mode. |
| `--led COLOR` | string | — | Set NeoPixel color. Valid: `red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `white`, `on`, `off`. |

Default (no action flags): reads and prints the full device state (LED color and all configured pin values).

### What `test_ble_api.py` Tests (Indirectly)

`test_ble_api.py` communicates over BLE — it never imports firmware C++ code.
However, many test scenarios exercise internal device drivers inside the ESP32:

| Test Flag | BLE Characteristic | Firmware Driver Involved | What It Tests |
|---|---|---|---|
| `--set 4:1` | BA01 (Control) | `GpioController` | Digital write |
| `--set 4:1:persist` | BA01 (Control) | `GpioController` + `StorageController` | Digital write + NVS persistence |
| `--config "pin=4:out"` | BA01 (Control) | `GpioController` | Pin mode config |
| `--config "pin=4:out:persist"` | BA01 (Control) | `GpioController` + `StorageController` | Pin mode + NVS persistence |
| `--led red` | BA01 (Control) | `LedRGBController` | NeoPixel colour |
| `--get 7` | BA01 (Control) | `GpioController` | Digital/analog read |

The dependency chain is:

```
test_ble_api.py (Python host)
  └─ BleApiClient (Python)
       └─ bleak / BLE GATT
            └─ BleApiServer (ESP32 firmware)
                 ├─ GpioController    → GPIO read/write
                 ├─ LedRGBController  → NeoPixel LED
                 └─ StorageController → NVS persistence
```

---

## ConfigStore (`ConfigStoreDemo`)

Communicates with the **ConfigStore** firmware service over **USB Serial UART** (not BLE). The ESP32-S3 exposes a command-line interface at 921600 baud for reading/writing typed NVS settings: device name (`String`), interval (`int`), enabled flag (`bool`), and gain (`float`). All values persist across power cycles.

### Flash the Firmware

```bash
cd firmware/apps/ConfigStoreDemo
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

Requires an OLED display wired to GPIO 41 (SDA) / GPIO 42 (SCL). See `firmware/services/ConfigStore/README.md` for wiring details.

### Python Test Client

*Requires USB cable connection (not Bluetooth).*

```bash
# Auto-detect port, run all tests
uv run python test_config_store.py

# Specify port explicitly
uv run python test_config_store.py --port /dev/cu.usbmodem1101

# Test NVS persistence across reboot (DTR toggle)
uv run python test_config_store.py --test-persist

# Read current config only
uv run python test_config_store.py --show-only
```

### Serial Commands (Firmware-side)

| Command | Action |
|---|---|
| `name <str>` | Set device name |
| `intv <N>` | Set interval seconds |
| `enab <0\|1>` | Enable/disable |
| `gain <N.N>` | Set gain multiplier |
| `show` | Display all values |
| `list` | List stored keys |
| `clear` | Reset to defaults |

### Test Scenarios

The test script validates:
- Reading initial config
- Setting all four typed values (String, int, bool, float)
- Verifying responses match expected output
- Listing keys and confirming all are stored
- Optional: persistence across power cycle (`--test-persist`)
- Reset to defaults and verification

### What `test_config_store.py` Tests (Indirectly)

`test_config_store.py` communicates over USB Serial UART — it never imports firmware C++ code.
However, every test scenario exercises internal device drivers inside the ESP32:

| Test Scenario | Serial Command | Firmware Driver Involved | What It Tests |
|---|---|---|---|
| Read config | `show` | `StorageController` | NVS read |
| Set name | `name <str>` | `StorageController` | NVS write (String) |
| Set interval | `intv <N>` | `StorageController` | NVS write (int) |
| Set enabled | `enab <0\|1>` | `StorageController` | NVS write (bool) |
| Set gain | `gain <N.N>` | `StorageController` | NVS write (float) |
| List keys | `list` | `StorageController` | NVS key enumeration |
| Reset | `clear` | `StorageController` | NVS namespace erase |
| `--test-persist` | DTR reboot + `show` | `StorageController` | NVS survival across reboot |
| OLED display | (all commands) | `DisplayController` | 128×32 live summary update |

The dependency chain is:

```
test_config_store.py (Python host)
  └─ ConfigStoreSerialClient (Python)
       └─ pyserial / USB UART
            └─ ConfigStore (ESP32 firmware service)
                 ├─ StorageController  → NVS/Preferences persistence
                 └─ DisplayController    → U8g2 OLED (128×32)
```

---

## Camera Web Server (`CameraWebServerDemo`)

Communicates with the **CameraWebServer** firmware service running on a **nulllaborg ESP32-S3 CAM** board. This is accessed via a **web browser** — there is no Python test client.

### Flash the Firmware

```bash
cd firmware/apps/CameraWebServerDemo
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

Single build environment (`esp32-s3-cam`) with QIO flash, OPI PSRAM, 8 MB flash, `default_8MB.csv` partitions, and the `tzapu/WiFiManager` dependency.

### Usage

1. **First boot (no saved WiFi):** Board creates AP **"ESP32-S3-CAM-Setup"**. Connect your phone/laptop to this AP → captive portal opens → enter your home WiFi credentials → board reboots.
2. **Subsequent boots:** Board auto-connects to saved WiFi. Watch serial monitor (921600 baud) for the IP address:
   ```
   WiFi: connected | IP=192.168.1.42
   CameraWebServer ready: http://192.168.1.42
   ```
3. Open `http://<ip>` in a browser to see the dark-theme dashboard with auto-refreshing snapshot (every 1 second).
4. Open `http://<ip>/camera/stream` for the MJPEG live stream (can be saved as `.mjpeg`).
5. Click the "Reset WiFi Credentials" button or POST to `http://<ip>/wifi/reset` to clear saved credentials and reboot into AP mode.

### Endpoints

| URL | Content |
|---|---|
| `http://<ip>/` | HTML dashboard — auto-refreshing snapshot, stream link, wifi reset button |
| `http://<ip>/camera/capture` | Single JPEG snapshot (raw from PSRAM, no copy) |
| `http://<ip>/camera/stream` | MJPEG live stream (~5 fps, multipart/x-mixed-replace) |
| `http://<ip>/wifi/reset` | POST — clear WiFi credentials, reboot into AP mode |
| `http://<ip>/style.css` | Dark-theme stylesheet |

### Architecture

FreeRTOS producer-consumer: `camera_task` (Core 0) captures frames at ~20 fps and overwrites a depth-1 queue. `web_task` (Core 1) handles HTTP requests and peeks the queue without removing. The camera_task is the sole owner of DMA frame buffers — no leaks, no mutexes, no contention.

---

## Python Dependencies

| Venv | Packages | Source |
|---|---|---|
| **Root workspace** (`/` or `firmware/`) | `platformio>=6.1`, `pyyaml`, `pip` | Root `pyproject.toml` `[dependency-groups] dev` |
| **Host tests** (`test/host/`) | `bleak>=0.22`, `embedded-system-services` | `test/host/pyproject.toml` (pulls `embedded-system-services` from `python-libs/` workspace member) |
| **CameraWebServer** | None (browser-based) | — |

- `uv sync` at the **repo root** installs PlatformIO + pyyaml + pip for firmware builds.
- `uv sync` from **`test/host/`** installs `embedded-system-services` (via `python-libs/`) + `bleak` for host-side BLE test scripts.
- `uv run` works from **any subdirectory** — uv walks up the tree to find the workspace root and uses the correct venv.

---

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `Multiple top-level packages discovered in a flat-layout` | Root `pyproject.toml` lacks explicit package config | Already fixed — `packages = []` set under `[tool.setuptools]` in root `pyproject.toml`. |
| `MissingPackageManifestError: Could not find 'package.json'` | `tool-esptoolpy` post-install failed because `pip` was missing from uv venv | Already fixed — `pip` added to root dev dependencies. If you hit this, delete `~/.platformio/packages/tool-esptoolpy` and retry `uv run pio run --target upload`. |
| `Device 'ESP32-CAM' not found` | Board not powered or BLE not advertising | Check USB connection. Press RST button. Ensure Bluetooth is ON on your laptop. |
| `Device 'ESP32-API' not found` | Same as above | Same checks. Verify firmware environment matches your board (DevKitC-1 vs DORHEA). |
| `No module named 'embedded_system_services'` | Dependencies not installed | Run `uv sync` from repo root, or `uv sync` from `test/host/`. |
| Snapshot is all black | Lens cap still on or lighting too dark | Remove lens cap. Use `--flash` for low light. |
| `No frame available yet` | Camera still initializing | Wait a few seconds after boot. Camera takes 1-2 seconds to init. |
| macOS Bluetooth permission popup | Terminal needs Bluetooth access | Grant permission: System Settings > Privacy & Security > Bluetooth. |
| `pio: command not found` / `Failed to spawn: pio` | PlatformIO not installed | Run `uv sync` from the repo root. |
| MJPEG stream freezes in browser | Network congestion or client too slow | Reduce to 5 fps. Reload page. Try `/camera/capture` instead for still images. |
| WiFi captive portal doesn't appear | Board already has saved credentials | Browse to `http://<ip>/wifi/reset` or use `--reset` on the camera BLE test. |
