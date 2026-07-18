# CameraBluetoothServer

BLE camera server that composes `CameraController` with the Arduino-ESP32 BLE stack to serve JPEG snapshots over Bluetooth Low Energy. Uses a FreeRTOS producer-consumer architecture with two pinned tasks: camera capture on Core 0 and BLE event handling on Core 1.

## What it contains

| File | Role |
|---|---|
| `src/CameraBluetoothServer.h` | Class declaration — BLE service UUIDs, characteristic definitions, settings state |
| `src/CameraBluetoothServer.cpp` | Full implementation — BLE setup, chunked frame delivery, settings CRUD, FreeRTOS tasks |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

### Dependencies

| Dependency | Source |
|---|---|
| `CameraController` | Internal (same esp32-libs folder) |
| `StorageController` | Internal (same esp32-libs folder) |
| Arduino BLE stack (`BLEDevice`, `BLEServer`, `BLECharacteristic`) | Bundled with Arduino-ESP32 framework |
| FreeRTOS (`queue`, `mutex`, `tasks`) | Bundled with Arduino-ESP32 framework |

### Hardware

Designed for **nulllaborg ESP32-S3 CAM** with OV2640 or OV3660 camera. USB-C for power, programming, and serial monitor. Board preset: `NULLLAB_ESP32S3_CAM`, 8 MB Flash, 8 MB PSRAM.

## Configuration

### Internal Constants

| Constant | Type | Value | Description |
|---|---|---|---|
| `CAM_TASK_STACK` | `uint16_t` | 4096 | Stack size (words) for the camera capture FreeRTOS task |
| `BLE_TASK_STACK` | `uint16_t` | 8192 | Stack size (words) for the BLE event loop FreeRTOS task |
| `CAM_TASK_PRIO` | `uint8_t` | 1 | Priority of the camera task (lower number = lower priority) |
| `BLE_TASK_PRIO` | `uint8_t` | 2 | Priority of the BLE task |
| `CHUNK_SIZE` | `uint16_t` | 240 | Max bytes per BLE notification chunk (limited by BLE MTU) |

### Camera Settings (persisted to NVS, modifiable via BLE)

| Setting | Type | Range | Default | Description |
|---|---|---|---|---|
| `_frameSize` | `FrameSize` enum | 18 sizes, 96×96 to QSXGA | `QVGA` (320×240) | Captured image resolution. Changing size triggers DMA pool reallocation (~50–200ms). |
| `_jpegQuality` | `uint8_t` | 0 (best) – 63 (worst) | 20 | JPEG compression level. Lower = better quality, larger file. Values above 40 produce visible artifacts. |
| `_brightness` | `int8_t` | -2 – 2 | 0 | Image brightness. -2 = darkest, 2 = brightest. Writes to sensor IC register (instant). |
| `_contrast` | `int8_t` | -2 – 2 | 0 | Image contrast. Same sensor-side effect as brightness. |
| `_saturation` | `int8_t` | -2 – 2 | 0 | Color saturation. -2 = grayscale, 2 = maximum saturation. |
| `_specialEffect` | `int` | 0 – 6 | 0 (off) | 0=off, 1=negative, 2=black&white, 3=red tint, 4=green tint, 5=blue tint, 6=sepia. |
| `_vflip` | `bool` | true / false | true | Vertical mirror. True by default because the camera is typically mounted upside-down. |
| `_hflip` | `bool` | true / false | false | Horizontal mirror. |
| `_wb` | `WhiteBalance` enum | WB_AUTO / WB_SUNNY / WB_CLOUDY / WB_OFFICE / WB_HOME | `WB_AUTO` | White balance preset. AUTO lets sensor adjust dynamically. |
| `_flashOn` | `bool` | true / false | false | Flash LED state. Persistent across settings changes. |
| `_aecOn` | `bool` | true / false | true | Auto-exposure control. When OFF, manual `_shutter` and `_gain` are used. |
| `_shutter` | `uint16_t` | 0 – 1200 | 0 | Manual exposure time (arbitrary sensor units). Only effective when `_aecOn` is false. |
| `_gain` | `uint8_t` | 0 – 30 | 0 | Manual analog gain. Higher = brighter but noisier. Only effective when `_aecOn` is false. |

### Settings via BLE

All camera settings can be read and written over the **CA02 (Settings)** characteristic. The format is comma-separated key=value pairs, supporting partial updates (only send the settings you want to change). Keys correspond to camera sensor register names: `framesize`, `quality`, `brightness`, `contrast`, `saturation`, `special_effect`, `vflip`, `hflip`, `awb`, `aec`, `shutter`, `gain`.

Settings are persisted to NVS in the `"camera"` namespace and restored on boot.

## Usage

```cpp
#include <CameraBluetoothServer.h>

CameraBluetoothServer server;

void setup() {
    Serial.begin(921600);
    server.begin();   // Initializes camera, BLE, loads settings from NVS
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));  // FreeRTOS tasks handle everything
}
```

## BLE Protocol

### Service UUID: `0000CA00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Control | CA01 | Write | Commands: `"snapshot"`, `"flash_on"`, `"flash_off"`, `"save"`, `"reset"` |
| Settings | CA02 | Write + Read | Key=value pairs: `"quality=12,framesize=6"` |
| Frame Data | CA03 | Notify | Chunked JPEG delivery (240-byte chunks) |
| Info | CA04 | Read | JSON: `{"size":12345,"w":320,"h":240,"q":20}` |
| Params | CA05 | Read | JSON schema of valid settings keys, values, and ranges |

### Frame Chunking Protocol

1. First notification: `[4 bytes total_size LE]` + first ≤240 bytes of JPEG
2. Subsequent notifications: ≤240 bytes each, continuation data
3. Client accumulates bytes until `total_size` is reached

## FreeRTOS Architecture

```
Core 0: _cameraTask  (priority 1, stack 4096)
          └─ captures frames → xQueueOverwrite(_frameQueue, depth=1)

Core 1: _bleTask      (priority 2, stack 8192)
          └─ runs BLE event loop, callbacks fire here
          └─ on "snapshot" command: reads from queue, sends chunked over CA03
```

## Expected Behavior

- On boot: camera initializes, BLE starts advertising as **"ESP32-CAM"**, settings loaded from NVS.
- Serial output at 921600 baud: initialization progress, BLE connection/disconnection events, memory diagnostics (Flash, DRAM, PSRAM usage).
- **First-time setup**: All camera settings at their defaults (QVGA, JPEG quality 20). Change settings over BLE and send `"save"` to persist.
- **Snapshot flow**: Send `"snapshot"` over CA01 → camera captures → JPEG delivered via CA03 notifications in 240-byte chunks. A QVGA JPEG at quality 20 is typically 2–15 KB.
- **Settings**: Write key=value pairs to CA02 (e.g., `"quality=10,framesize=10"`). Read CA02 to see all current settings as a key=value string. Read CA05 to see valid ranges.
- **Info**: Read CA04 after each snapshot for frame metadata (size in bytes, width, height, quality).
- **flash_on / flash_off**: Controls the board's flash LED (GPIO 3 on NULLLAB_ESP32S3_CAM). State is not persisted unless `"save"` is sent.
- **reset**: Factory resets all NVS (clears camera settings and WiFi credentials), then reboots.
- The camera task runs continuously in the background. The BLE callbacks only initiate snapshot delivery when requested — frames are not streamed automatically.
- **Python test client**: Use `test_camera_ble.py` (in `ESP32-S3/examples/CameraBluetoothServer/`) for testing from a laptop. Requires `bleak` library.
