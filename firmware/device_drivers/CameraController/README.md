# CameraController

Single-responsibility wrapper around the `esp_camera` library (bundled with the Arduino-ESP32 framework). Owns `camera_config_t` and `sensor_t`. Provides board presets for 6 common ESP32 camera modules, frame capture with raw `camera_fb_t*` access, and get/set methods for all sensor settings.

## What it contains

| File | Role |
|---|---|
| `src/CameraController.h` | Class declaration — BoardModel enum, FrameSize enum, WhiteBalance enum, all getter/setter method signatures |
| `src/CameraController.cpp` | Full implementation — board preset pin tables, esp_camera_init, sensor register read/write, flash control |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

No external dependencies — `esp_camera` is bundled with the `arduino-esp32` framework.

### Hardware Requirements

- **PSRAM** is required (8 MB recommended). The DMA frame buffer pool lives in PSRAM.
- **Sufficient power** — camera modules draw 200–400 mA. USB-powered boards may brown out under peak load. Add a 100–470 µF capacitor across power pins for stability.
- Disable brownout detection if using USB power: `-DCONFIG_ESP32_BROWNOUT_DET=n` (or `-DBROWNOUT_DET=n`).
- Do **not** call `pinMode()` on camera data pins before `esp_camera_init()`.

## Configuration

### Board Model (constructor)

| Value | Board | Flash LED Pin | Notes |
|---|---|---|---|
| `AI_THINKER` | ESP32-CAM with OV2640 | GPIO 4 | ESP32 chip (not S3), OV2640 sensor |
| `ESP32S3_CAM_LCD` | Espressif ESP32-S3 CAM LCD | — | ESP32-S3 with camera + LCD |
| `ESP32S3_EYE` | ESP-EYE for S3 | — | ESP32-S3 |
| `NULLLAB_ESP32S3_CAM` | nulllaborg ESP32S3-CAM | GPIO 3 | ESP32-S3, OV2640/OV3660 |
| `XIAO_ESP32S3` | Seeed XIAO ESP32S3 + OV2640 | — | Compact form factor |
| `M5STACK_CAMS3` | M5Stack Camera Unit for S3 | — | ESP32-S3 |
| `CUSTOM` | User-defined pins | `-1` by default | All 17 pins must be set via `setPin()` before `begin()` |

### Custom Pin Configuration

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setPin(d0–d7, vsync, href, pclk, xclk, sda, scl, pwdn, reset, led)` | `int` × 14 | GPIO numbers or `-1` for unused | Board preset | Sets all 17 camera pins. Required for `CUSTOM` model. `d0–d7` are the 8-bit parallel data bus (Y2–Y9 in schematics). `pwdn`, `reset`, and `led` can be `-1` if not connected. |

### XCLK Master Clock Frequency

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setXCLKFreq(freq)` | `XCLKFreq` enum | `XCLK_10MHZ` (10 MHz), `XCLK_20MHZ` (20 MHz) | 20 MHz | Master clock fed to the camera sensor from the MCU. 10 MHz may reduce EMI or brownout risk on USB-powered boards. **Must be called BEFORE `begin()`.** Changes after `begin()` require an `end()` + `begin()` cycle. |

### DMA Frame Buffer Count

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setFbCount(count)` | `uint8_t` | 1 – 4 | 2 | Number of DMA frame buffers allocated from PSRAM. More buffers reduce the risk of `capture()` returning NULL under heavy load, but consume more PSRAM. Each buffer is `width × height × bytes_per_pixel`. At QVGA/JPEG this is roughly 60–100 KB per buffer. **Must be called BEFORE `begin()`.** |

### Frame Size

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setFrameSize(size)` | `FrameSize` enum | 18 values: `SIZE_96X96` (0), `QQVGA` (1), `QCIF` (3), `HQVGA` (4), `SIZE_240X240` (5), `QVGA` (6), `CIF` (8), `HVGA` (9), `VGA` (10), `SVGA` (11), `XGA` (12), `HD` (13), `SXGA` (14), `UXGA` (15), `FHD` (16), `QXGA` (19), `QHD` (20), `WQXGA` (21), `QSXGA` (23) | `QVGA` (320×240) | Image dimensions. Larger frames use more PSRAM and take longer to capture. **Changing size triggers DMA pool reallocation** — a blocking operation taking ~50–200ms during which no capture is possible. Enum values are not contiguous (match esp_camera sensor.h IDs). |

### JPEG Quality

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setJpegQuality(quality)` | `uint8_t` | 0 – 63 | 12 | JPEG compression level. **0 = best quality (largest file), 63 = worst quality (smallest file).** Typical range: 10–30. Values above 40 produce visible blocking artifacts. Constants: `JPEG_BEST = 0`, `JPEG_WORST = 63`. |

### Image Adjustments (sensor register writes, non-blocking)

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setBrightness(value)` | `int8_t` | -2 – 2 | 0 | Image brightness. -2 = darkest, 2 = brightest. Instant sensor IC register write. |
| `setContrast(value)` | `int8_t` | -2 – 2 | 0 | Image contrast. Same instant behavior. |
| `setSaturation(value)` | `int8_t` | -2 – 2 | 0 | Color saturation. -2 = grayscale, 2 = maximum saturation. |
| `setSpecialEffect(effect)` | `int` | 0 – 6 | 0 (no effect) | 0=off, 1=negative, 2=black&white, 3=reddish, 4=greenish, 5=blueish, 6=sepia. Sensor-side — no CPU cost. |

### Flip/Mirror

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setVFlip(on)` | `bool` | true / false | false | Vertical mirror. Use when camera is physically mounted upside-down. |
| `setHFlip(on)` | `bool` | true / false | false | Horizontal mirror. Use for selfie/front-facing orientation. |

### White Balance

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setWhiteBalance(mode)` | `WhiteBalance` enum | `WB_AUTO` (0), `WB_SUNNY` (1), `WB_CLOUDY` (2), `WB_OFFICE` (3), `WB_HOME` (4) | `WB_AUTO` | Color temperature preset. AUTO lets the sensor dynamically adjust. Manual presets lock to a fixed color temperature for consistent results in known lighting. |

### Exposure Control

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setAecMode(on)` | `bool` | true / false | — | Auto-Exposure Control. When ON (default behavior of most presets), the sensor automatically adjusts exposure time and gain. When OFF, use `setAecValue()` and `setAgcGain()` for manual control. |
| `setAecValue(value)` | `uint16_t` | 0 – 1200 | — | Manual exposure time in arbitrary sensor units (roughly shutter speed). Only effective when AEC is OFF. Higher = longer exposure, brighter image, more motion blur. |
| `setAgcGain(gain)` | `uint8_t` | 0 – 30 | — | Manual analog gain. Only effective when AEC is OFF. Higher = brighter image but more noise. |

## Usage

```cpp
#include <CameraController.h>

CameraController cam(CameraController::AI_THINKER);

void setup() {
    cam.setXCLKFreq(CameraController::XCLK_20MHZ);  // before begin()
    cam.setFbCount(2);                               // before begin()
    cam.begin();
    cam.setFrameSize(CameraController::QVGA);
    cam.setJpegQuality(12);
}

void loop() {
    camera_fb_t* fb = cam.capture();
    if (fb) {
        Serial.printf("Frame: %u bytes, %ux%u\n", fb->len, fb->width, fb->height);
        // fb->buf  — uint8_t* raw image data (JPEG by default)
        // fb->len  — size_t buffer length
        // fb->format — pixformat_t (PIXFORMAT_JPEG)
        cam.release(fb);  // REQUIRED — frees the DMA buffer
    }
    delay(1000);
}
```

## Frame Buffer Lifecycle

**Critical rule**: Call `release(fb)` after every successful `capture()`. The DMA pool has a limited number of buffers (default 2). After 2 unreleased captures, `capture()` returns NULL forever (or until a buffer is released). Never call `release()` on a NULL pointer.

```
camera_fb_t* fb = cam.capture();   // acquires a buffer from the pool
if (fb) {
    process(fb->buf, fb->len);
    cam.release(fb);                // returns buffer to the pool
}
```

## FreeRTOS Integration

The camera is synchronous (no internal queues). For multi-threaded use, create a camera task:

```cpp
static void cameraTask(void*) {
    for (;;) {
        camera_fb_t* fb = cam.capture();
        if (fb) xQueueSend(frameQueue, &fb, portMAX_DELAY);
    }
}

static void consumerTask(void*) {
    for (;;) {
        camera_fb_t* fb;
        if (xQueueReceive(frameQueue, &fb, portMAX_DELAY)) {
            process(fb->buf, fb->len);
            cam.release(fb);
        }
    }
}
```

## Flash LED

| Method | Description |
|---|---|
| `flashOn()` | Turn on the board's flash LED. Pin depends on board model. |
| `flashOff()` | Turn off the flash LED. |

Flash LED GPIO per board: `AI_THINKER` = GPIO 4, `NULLLAB_ESP32S3_CAM` = GPIO 3. Other boards may not have a flash LED pin.

## Hardware

| Board | Chip | Camera Connector | PSRAM |
|---|---|---|---|
| AI_THINKER | ESP32 | OV2640 (24-pin DVP) | 4 MB |
| ESP32S3_CAM_LCD | ESP32-S3 | OV2640/OV5640 | 8 MB |
| ESP32S3_EYE | ESP32-S3 | OV2640 | 8 MB |
| NULLLAB_ESP32S3_CAM | ESP32-S3 | OV2640/OV3660 | 8 MB |
| XIAO_ESP32S3 | ESP32-S3 | OV2640 (via module) | 8 MB |
| M5STACK_CAMS3 | ESP32-S3 | Various (via module) | 8 MB |

Supported sensors: OV2640, OV5640, OV3660, OV7725, GC032A, GC2145, SC031GS. Sensor is auto-detected during `begin()`.

## Expected Behavior

- `begin()` initializes the camera, allocates DMA buffers in PSRAM, and runs sensor auto-detection. Returns `true` on success, `false` on failure.
- `capture()` blocks until a frame is ready (typically 30–100ms depending on frame size and exposure). Returns NULL if the DMA pool is exhausted or the sensor is not ready.
- **Frame size change**: `setFrameSize()` triggers DMA pool reallocation (~50–200ms blocking). During this time, any pending capture will return NULL. No other sensor setting triggers reallocation.
- **Settings changes (brightness, contrast, etc.)**: Instant sensor IC register writes — non-blocking, take effect on the next frame.
- **First capture after boot**: May take longer (auto-exposure, auto-white-balance settling). Subsequent captures are consistent.
- **PSRAM usage**: At QVGA/JPEG, each frame buffer is ~60–100 KB. At UXGA (1600×1200), each buffer is ~2 MB. With `setFbCount(2)`, UXGA requires ~4 MB of PSRAM.
- `end()` deinitializes the camera and frees DMA buffers. A subsequent `begin()` reinitializes as fresh.
- **GPIO 0 conflict** on AI-Thinker boards: GPIO 0 is shared between camera XCLK output and boot strap pin. Do not connect external pull-up/down on GPIO 0.
