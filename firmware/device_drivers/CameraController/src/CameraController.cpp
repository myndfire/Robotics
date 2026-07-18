/*
 * CameraController.cpp — ESP32 camera driver wrapper
 *
 * Full implementation and reference documentation.
 *
 * ==================================================================
 *                       HARDWARE SETUP
 * ==================================================================
 *
 * ── AI-Thinker ESP32-CAM (most common board) ────────────────
 *
 *   ESP32-CAM Pinout (OV2640 sensor, 2MP, flash LED):
 *     Power: 5V pin   → 5 V DC (via FTDI or 5V header)
 *            GND pin  → GND
 *     Serial: U0R (GPIO 3)  → FTDI / USB-UART TX
 *             U0T (GPIO 1)  → FTDI / USB-UART RX
 *     Camera pins (pre-wired on-board, no user wiring needed):
 *       D0 (Y2) = GPIO 5    D1 (Y3) = GPIO 18   D2 (Y4) = GPIO 19
 *       D3 (Y5) = GPIO 21   D4 (Y6) = GPIO 36   D5 (Y7) = GPIO 39
 *       D6 (Y8) = GPIO 34   D7 (Y9) = GPIO 35
 *       VSYNC   = GPIO 25   HREF    = GPIO 23   PCLK  = GPIO 22
 *       XCLK    = GPIO 0     (master clock output to camera)
 *       SIOD    = GPIO 26   SIOC    = GPIO 27   (SCCB / I2C control)
 *       PWDN    = GPIO 32   RESET   = not connected
 *       Flash LED = GPIO 4   (active HIGH)
 *
 *   Programming procedure:
 *     1. Wire FTDI: TX→U0R, RX→U0T, GND→GND, 5V→5V
 *        (some FTDI adapters require setting jumper to 5V)
 *     2. Connect jumper wire from GPIO 0 to GND
 *        (puts ESP32 into download / bootloader mode)
 *     3. Press the RST button (or cycle power)
 *     4. Flash via PlatformIO:
 *          uv run pio run -d <example> --target upload
 *     5. Remove GPIO 0 → GND jumper
 *     6. Press RST button or cycle power to run the new firmware
 *     7. Open serial monitor at 921600 baud
 *
 *   IMPORTANT: GPIO 0 is shared between XCLK (camera master clock)
 *   and the boot-select strapping pin.  This is a hardware design
 *   trade-off on the AI-Thinker board — the camera XCLK is driven
 *   HIGH by the MCU, which conflicts with the boot-mode pull-down
 *   requirement.  The esp_camera library handles this by holding
 *   GPIO 0 in the correct state during boot.
 *
 * ── Espressif ESP32-S3 CAM LCD ──────────────────────────────
 *
 *   Official Espressif dev board with ESP32-S3, OV2640 or OV5640,
 *   LCD connector, and USB-C for power + programming.  No external
 *   FTDI needed — flash directly over USB-C.
 *
 *   Board name in PlatformIO: "ESP32-S3-USB-OTG" or custom
 *   Camera pins (pre-wired on-board):
 *     D0=13, D1=47, D2=14, D3=3, D4=41, D5=42, D6=12, D7=39
 *     VSYNC=21, HREF=38, PCLK=11, XCLK=40
 *     SIOD=17, SIOC=18
 *     PWDN and RESET not connected (-1)
 *
 * ── ESP-EYE (ESP32-S3) ──────────────────────────────────────
 *
 *   Face-recognition dev board.  USB-C for power + flash.
 *   Camera pins: D0=11, D1=9, D2=8, D3=10, D4=12, D5=18, D6=17, D7=16
 *                VSYNC=6, HREF=7, PCLK=13, XCLK=15
 *                SIOD=4, SIOC=5
 *
 * ── Seeed XIAO ESP32S3 + OV2640 Camera Module ──────────────
 *
 *   Compact 21×17.5 mm board with USB-C, OV2640 ribbon connector.
 *   Camera pins: D0=15, D1=17, D2=18, D3=16, D4=14, D5=12, D6=11, D7=48
 *                VSYNC=38, HREF=47, PCLK=13, XCLK=10
 *                SIOD=40, SIOC=39
 *
 * ── M5Stack Camera Unit for S3 ──────────────────────────────
 *
 *   Camera pins: D0=6, D1=15, D2=16, D3=7, D4=5, D5=10, D6=4, D7=13
 *                VSYNC=42, HREF=18, PCLK=12, XCLK=11
 *                SIOD=17, SIOC=41, RESET=21
 *                Flash LED = GPIO 14
 *
 * ── CUSTOM Board ────────────────────────────────────────────
 *
 *   For boards not in the preset list, use BoardModel::CUSTOM and
 *   call setPin() with all 16 parameters before begin():
 *
 *     CameraController cam(CameraController::CUSTOM);
 *     cam.setPin(d0, d1, d2, d3, d4, d5, d6, d7,
 *                vsync, href, pclk, xclk,
 *                sda, scl, pwdn=-1, reset=-1, led=-1);
 *     cam.begin();
 *
 *
 * ==================================================================
 *                       POWER CONSIDERATIONS
 * ==================================================================
 *
 * ── Voltage ─────────────────────────────────────────────────
 *
 *   The ESP32-CAM module expects 5 V on the 5V pin (regulated down
 *   to 3.3 V by the on-board AMS1117 LDO).  Do NOT feed 5 V to the
 *   3.3 V pin — you will destroy the board.
 *
 *   USB-powered boards (ESP32-S3 CAM LCD, XIAO, ESP-EYE) get 5 V
 *   from USB-C and regulate on-board.
 *
 * ── Current ─────────────────────────────────────────────────
 *
 *   The OV2640 sensor draws ~40 mA during active capture.  A WiFi
 *   transmission (802.11b/g/n) can draw 300–400 mA peak.  Combined,
 *   you need a power supply capable of at least 500 mA continuous.
 *
 *   Many FTDI adapters only supply 100–200 mA on their 5V pin —
 *   this is a common cause of camera init failures or brownout
 *   resets.  Symptoms:
 *     • esp_camera_init() returns error 0x105 (ESP_ERR_TIMEOUT)
 *     • Board resets when WiFi.begin() is called
 *     • Frame captures return NULL intermittently
 *
 *   Solutions:
 *     1. Use a FTDI adapter with a genuine 5 V / 1 A regulator
 *     2. Power the ESP32-CAM via an external 5 V supply, shared
 *        GND with the FTDI adapter
 *     3. Add a 100–470 µF electrolytic capacitor across 5V and
 *        GND near the board header
 *
 * ── Brownout detection ──────────────────────────────────────
 *
 *   ESP32 has a brownout detector (BOD) that resets the chip if
 *   the 3.3 V rail dips below ~2.6–2.7 V.  If you see repeated
 *   resets, add a capacitor or use a better power supply.
 *
 *
 * ==================================================================
 *                       PSRAM REQUIREMENTS
 * ==================================================================
 *
 *   Frame buffers are allocated from PSRAM (SPIRAM) by default
 *   (CAMERA_FB_IN_PSRAM).  This requires:
 *
 *   • AI-Thinker ESP32-CAM:  4 MB external PSRAM (standard on
 *     most modules — check that your board has the PSRAM chip)
 *   • ESP32-S3 boards:       2–8 MB OPI or QPI PSRAM
 *
 *   If your board lacks PSRAM, change fb_location to
 *   CAMERA_FB_IN_DRAM (edit the source or add a setter — the DRAM
 *   pool limits you to ~QQVGA/QVGA resolutions due to heap size).
 *
 *   To check PSRAM size at runtime:
 *     Serial.printf("PSRAM: %d bytes\n", ESP.getPsramSize());
 *
 *
 * ==================================================================
 *                       SUPPORTED SENSORS
 * ==================================================================
 *
 *   The esp_camera library auto-detects the sensor model via its
 *   I2C PID register during esp_camera_init().  No sensor model
 *   parameter is needed — the driver reads the chip ID and loads
 *   the correct register table.
 *
 *   Commonly supported sensors (driver database includes all):
 *     • OV2640    (PID 0x26)  — 2 MP, most common on ESP32-CAM
 *     • OV5640    (PID 0x5640) — 5 MP, auto-focus, found on some
 *       ESP32-S3 CAM LCD boards and M5Stack Camera units
 *     • OV3660    (PID 0x3660) — 3 MP
 *     • OV7725    (PID 0x77)   — VGA
 *     • GC032A    (PID 0x232A)  — 0.3 MP
 *     • GC2145    (PID 0x2145)  — 2 MP
 *     • SC031GS   (PID 0x0031)  — 0.3 MP global shutter
 *
 *
 * ==================================================================
 *                       FRAME BUFFER LIFECYCLE
 * ==================================================================
 *
 *   This is the single most important concept to understand.
 *
 *   esp_camera_init() creates a pool of 2 DMA frame buffers in
 *   PSRAM.  Each buffer holds one complete camera frame (JPEG or
 *   raw, depending on pixel_format).
 *
 *   capture() → esp_camera_fb_get()
 *     • Takes one buffer from the pool.
 *     • Blocks until a full frame is received from the DVP bus
 *       (typically 20–40 ms at QVGA, longer at higher res).
 *     • Returns NULL if both buffers are in use (pool exhausted).
 *
 *   release(fb) → esp_camera_fb_return(fb)
 *     • Returns the buffer to the pool so capture() can reuse it.
 *     • MUST be called exactly once for every successful capture().
 *     • NEVER call on a NULL pointer.
 *     • The buffer contents become invalid after release() — copy
 *       data before releasing if you need it later.
 *
 *   Pool exhaustion scenario:
 *     1. capture() takes buffer #1  → pool has 1 remaining
 *     2. capture() takes buffer #2  → pool has 0 remaining
 *     3. capture() returns NULL      → no buffers available!
 *     4. release(buffer #1)         → pool has 1 again
 *     5. capture() succeeds again
 *
 *   This is why the FreeRTOS pattern (camera task → queue →
 *   consumer task) works well: the camera task never releases,
 *   the consumer task always releases.  No buffer leaks.
 *
 *
 * ==================================================================
 *                       IMAGE DATA
 * ==================================================================
 *
 *   The default pixel format is PIXFORMAT_JPEG.  Each frame buffer
 *   contains a complete, self-contained JPEG image starting with
 *   the SOI marker 0xFF 0xD8.
 *
 *   You can send JPEG data directly to:
 *     • Serial:        Serial.write(fb->buf, fb->len)
 *     • HTTP client:   client.write(fb->buf, fb->len)
 *     • HTTP server:   server.send_P(200, "image/jpeg",
 *                        (const char*)fb->buf, fb->len)
 *     • SD card:       file.write(fb->buf, fb->len)
 *     • MJPEG stream:  multipart/x-mixed-replace HTTP response
 *                      with boundary delimiters between frames
 *
 *   Converting to other formats (if needed):
 *     • fmt2jpg()   — any format → JPEG (included in esp_camera)
 *     • fmt2bmp()   — any format → BMP
 *     • fmt2rgb888() — any format → RGB888 (for ML / display)
 *     • jpg2rgb565() — JPEG → RGB565 (decode for TFT / LCD)
 *
 *   These converter functions live in img_converters.h
 *   (#include "img_converters.h").
 *
 *
 * ==================================================================
 *                       FRAME SIZE GUIDE
 * ==================================================================
 *
 *   enum ID  | Resolution | Pixels  | JPEG size (typ.) | Notes
 *   ─────────┼────────────┼─────────┼──────────────────┼──────
 *   QQVGA(4) | 160 × 120  |  19.2K  |  2–6 KB          | Tiny, fast
 *   QVGA (7) | 320 × 240  |  76.8K  |  8–20 KB         | Default, good balance
 *   VGA  (10)| 640 × 480  | 307.2K  | 25–60 KB         | Requires PSRAM
 *   SVGA (8) | 800 × 600  | 480K    | 40–90 KB         | Heavy, slower FPS
 *   XGA  (9) | 1024× 768  | 786K    | 60–150 KB        | Max for most modules
 *   SXGA (12)| 1280×1024  | 1.31M   | 100–200 KB       | Very slow capture
 *   UXGA (13)| 1600×1200  | 1.92M   | 150–400 KB       | Full 2 MP sensor res
 *
 *   Higher resolutions significantly increase capture latency.
 *   At QVGA, expect ~15–25 FPS.  At UXGA, ~1–3 FPS.
 *
 *   The camera_config_t must be sized for the largest frame you
 *   intend to capture.  The library defaults to QVGA buffers
 *   (the most compatible).  If you switch to a higher resolution
 *   via setFrameSize() after begin(), the library calls
 *   esp_camera_reconfigure() which re-allocates the DMA pool.
 *
 *
 * ==================================================================
 *                       JPEG QUALITY
 * ==================================================================
 *
 *   Quality 0–63 (lower = better quality, larger file):
 *      0–10   Very high quality, ~30–50 KB at QVGA
 *     10–20   Good balance for web streaming,  10–20 KB
 *     20–40   Acceptable for snapshots,          5–12 KB
 *     40–63   Blocky / artifact-heavy,           2– 6 KB
 *
 *   Default: 12 (good quality, reasonable file size).
 *
 *
 * ==================================================================
 *                       FREERTOS PATTERN
 * ==================================================================
 *
 *   CameraController is synchronous — capture() blocks until the
 *   DMA receives a complete frame.  For multi-threaded use, follow
 *   this pattern at the APPLICATION level:
 *
 *     // Shared resources
 *     CameraController cam(CameraController::AI_THINKER);
 *     QueueHandle_t frameQueue = xQueueCreate(2, sizeof(camera_fb_t*));
 *
 *     // Producer: captures continuously, never releases
 *     static void cameraTask(void*) {
 *       for (;;) {
 *         camera_fb_t* fb = cam.capture();
 *         if (fb) {
 *           // Non-blocking send — if queue full, frame is dropped
 *           xQueueSend(frameQueue, &fb, 0);
 *         }
 *         vTaskDelay(pdMS_TO_TICKS(50));
 *       }
 *     }
 *
 *     // Consumer: pops frames, processes, ALWAYS releases
 *     static void consumerTask(void*) {
 *       for (;;) {
 *         camera_fb_t* fb;
 *         if (xQueueReceive(frameQueue, &fb, portMAX_DELAY)) {
 *           // send over WiFi, save to SD, run ML model, etc.
 *           Serial.printf("Frame: %u bytes\n", fb->len);
 *           cam.release(fb);  // CRITICAL — returns buffer to DMA pool
 *         }
 *       }
 *     }
 *
 *
 * ==================================================================
 *                       COMMON PITFALLS
 * ==================================================================
 *
 *   1. FORGETTING release(): After 2 unreleased captures, all
 *      subsequent captures return NULL.  The camera appears broken
 *      but it's just a buffer leak.
 *
 *   2. POWER: ESP32-CAM brownout resets during WiFi + camera are
 *      almost always a power supply problem.  Add a capacitor.
 *
 *   3. GPIO 0 CONFLICT: On AI-Thinker ESP32-CAM, GPIO 0 is both
 *      the boot mode strap AND the camera XCLK output.  The
 *      esp_camera library manages this, but if you also attach
 *      other peripherals to GPIO 0, expect boot problems.
 *
 *   4. PSRAM ABSENCE: If your ESP32-CAM board does NOT have the
 *      external PSRAM chip (some budget clones omit it), camera
 *      init will fail because CAMERA_FB_IN_PSRAM has nowhere to
 *      allocate.  Verify with ESP.getPsramSize().
 *
 *   5. I2C ADDRESS CONFLICTS: The camera uses SCCB (an I2C variant)
 *      for control.  If you use the same I2C bus for other devices
 *      (OLED, sensor), ensure no address clash.  OV2640 is at 0x30
 *      (7-bit), OV5640 at 0x3C.
 *
 *   6. pinMode RESET: Do NOT call pinMode() on camera data pins
 *      before esp_camera_init().  The driver sets up DMA and pin
 *      muxing; user pinMode calls will conflict.
 *
 *   7. RECONFIGURE TIMING: Calling setFrameSize() during active
 *      capture tears down and re-creates the DMA pool.  Call it
 *      between captures, not during.
 *
 * See CameraController.h for the public API declaration and
 * PlatformIO example projects under LibraryExamples/CameraController/.
 */

#include "CameraController.h"

CameraController::CameraController(BoardModel model)
    : _model(model)
    , _sensor(nullptr)
    , _initialized(false)
    , _ledPin(-1)
{
    // Zero-init the config struct, then set safe defaults
    memset(&_config, 0, sizeof(_config));
    _config.pixel_format  = PIXFORMAT_JPEG;
    _config.frame_size    = FRAMESIZE_QVGA;  // enum value 6
    _config.jpeg_quality  = 12;
    _config.fb_count      = 2;
    _config.fb_location   = CAMERA_FB_IN_PSRAM;
    _config.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
    _config.ledc_channel  = LEDC_CHANNEL_0;
    _config.ledc_timer    = LEDC_TIMER_0;
    _config.xclk_freq_hz  = 20000000;

    _applyBoardPreset();
}

// ── Board presets ──────────────────────────────────────────────────────
//
// Pin assignments are sourced from Espressif's camera_pins.h.
// d0–d7 map to the camera module's Y2–Y9 outputs.
// All presets use 20 MHz XCLK via LEDC.

void CameraController::_applyBoardPreset() {
    switch (_model) {
        case AI_THINKER:
            // Y2=5, Y3=18, Y4=19, Y5=21, Y6=36, Y7=39, Y8=34, Y9=35
            // VS=25, HR=23, PC=22, XC=0, SD=26, SC=27, PW=32, LED=4
            setPin(5, 18, 19, 21, 36, 39, 34, 35,
                   25, 23, 22, 0,
                   26, 27, 32, -1, 4);
            break;

        case ESP32S3_CAM_LCD:
            // Y2=13, Y3=47, Y4=14, Y5=3, Y6=41, Y7=42, Y8=12, Y9=39
            // VS=21, HR=38, PC=11, XC=40, SD=17, SC=18
            setPin(13, 47, 14, 3, 41, 42, 12, 39,
                   21, 38, 11, 40,
                   17, 18, -1, -1, -1);
            break;

        case ESP32S3_EYE:
            // Y2=11, Y3=9, Y4=8, Y5=10, Y6=12, Y7=18, Y8=17, Y9=16
            // VS=6, HR=7, PC=13, XC=15, SD=4, SC=5, LED=3
            setPin(11, 9, 8, 10, 12, 18, 17, 16,
                   6, 7, 13, 15,
                   4, 5, -1, -1, 3);
            break;

        case NULLLAB_ESP32S3_CAM:
            // Same camera pins as ESP32S3_EYE.
            // nulllaborg ESP32S3-CAM board: flash LED on GPIO 3,
            // TF card (GPIO 38/39/40), battery JST, USB-C.
            setPin(11, 9, 8, 10, 12, 18, 17, 16,
                   6, 7, 13, 15,
                   4, 5, -1, -1, 3);
            break;

        case XIAO_ESP32S3:
            // Y2=15, Y3=17, Y4=18, Y5=16, Y6=14, Y7=12, Y8=11, Y9=48
            // VS=38, HR=47, PC=13, XC=10, SD=40, SC=39
            setPin(15, 17, 18, 16, 14, 12, 11, 48,
                   38, 47, 13, 10,
                   40, 39, -1, -1, -1);
            break;

        case M5STACK_CAMS3:
            // Y2=6, Y3=15, Y4=16, Y5=7, Y6=5, Y7=10, Y8=4, Y9=13
            // VS=42, HR=18, PC=12, XC=11, SD=17, SC=41, RS=21, LED=14
            setPin(6, 15, 16, 7, 5, 10, 4, 13,
                   42, 18, 12, 11,
                   17, 41, -1, 21, 14);
            break;

        default: // CUSTOM — user must call setPin() before begin()
            break;
    }
}

void CameraController::setPin(int d0, int d1, int d2, int d3,
                               int d4, int d5, int d6, int d7,
                               int vsync, int href, int pclk, int xclk,
                               int sda, int scl, int pwdn, int reset, int led) {
    _config.pin_d0     = d0;
    _config.pin_d1     = d1;
    _config.pin_d2     = d2;
    _config.pin_d3     = d3;
    _config.pin_d4     = d4;
    _config.pin_d5     = d5;
    _config.pin_d6     = d6;
    _config.pin_d7     = d7;
    _config.pin_vsync  = vsync;
    _config.pin_href   = href;
    _config.pin_pclk   = pclk;
    _config.pin_xclk   = xclk;
    _config.pin_sccb_sda = sda;
    _config.pin_sccb_scl = scl;
    _config.pin_pwdn   = pwdn;
    _config.pin_reset  = reset;

    _ledPin = led;
    if (_ledPin >= 0) {
        pinMode(_ledPin, OUTPUT);
        digitalWrite(_ledPin, LOW);
    }
}

// ── Lifecycle ───────────────────────────────────────────────────────────
//
// begin() calls esp_camera_init() with the configured camera_config_t.
// The driver: (1) configures LEDC for the XCLK output, (2) powers up the
// sensor via PWDN / RESET pins if connected, (3) probes the sensor PID
// over SCCB/I2C, (4) loads the correct register table, (5) allocates the
// frame buffer DMA pool from PSRAM.  Resolution and JPEG quality set
// before begin() take effect during init; changes after begin() use
// esp_camera_reconfigure() or direct sensor register writes.
//
// Common error codes from esp_camera_init():
//   0x105 (ESP_ERR_TIMEOUT)       — sensor not responding, check power/wiring
//   0x101 (ESP_ERR_NO_MEM)        — PSRAM not found, or fb_count too large
//   0x102 (ESP_ERR_INVALID_ARG)   — invalid pin assignments (e.g. duplicate)

bool CameraController::begin() {
    if (_initialized) return true;

    esp_err_t err = esp_camera_init(&_config);
    if (err != ESP_OK) {
        Serial.printf("esp_camera_init failed: 0x%x\n", err);
        return false;
    }

    _sensor = esp_camera_sensor_get();
    if (!_sensor) {
        Serial.println("Failed to get camera sensor");
        return false;
    }

    _initialized = true;
    return true;
}

void CameraController::end() {
    if (!_initialized) return;
    esp_camera_deinit();
    _initialized = false;
    _sensor = nullptr;
}

// ── Frame capture ───────────────────────────────────────────────────────
//
// capture() blocks until the DVP peripheral receives a complete frame.
// At QVGA/JPEG this is typically 20–40 ms.  Higher resolutions are
// proportionally slower.  Returns NULL if both DMA buffers are exhausted
// (see "Frame Buffer Lifecycle" in the file header).
//
// release() returns a buffer to the DMA pool.  Always call after
// processing — a leaked buffer permanently reduces available capacity.

camera_fb_t* CameraController::capture() {
    return esp_camera_fb_get();
}

void CameraController::release(camera_fb_t* fb) {
    if (fb) esp_camera_fb_return(fb);
}

// ── Sensor settings setters ─────────────────────────────────────────────
//
// All setters delegate to the sensor_t callback table loaded by the
// esp_camera driver during init.  Each writes to registers on the sensor
// IC via the SCCB bus.  Settings persist until power loss or explicit
// change — they are stored in sensor RAM, not ESP32 NVS.
//
// Frame size changes require re-initialization of the DMA buffer pool
// (done internally by esp_camera_reconfigure).  This is blocking and
// should NOT be called between capture() and release() — it can corrupt
// in-flight DMA transfers.
//
// setJpegQuality() writes immediately; setFrameSize() calls the heavier
// esp_camera_reconfigure() path.

bool CameraController::setFrameSize(FrameSize size) {
    if (!_sensor) return false;
    _config.frame_size = (framesize_t)size;
    return esp_camera_reconfigure(&_config) == ESP_OK;
}

bool CameraController::setJpegQuality(uint8_t quality) {
    if (!_sensor) return false;
    if (quality > 63) quality = 63;
    _config.jpeg_quality = quality;
    return _sensor->set_quality(_sensor, quality) == 0;
}

bool CameraController::setBrightness(int8_t value) {
    if (!_sensor) return false;
    if (value < -2) value = -2;
    if (value >  2) value =  2;
    return _sensor->set_brightness(_sensor, value) == 0;
}

bool CameraController::setContrast(int8_t value) {
    if (!_sensor) return false;
    if (value < -2) value = -2;
    if (value >  2) value =  2;
    return _sensor->set_contrast(_sensor, value) == 0;
}

bool CameraController::setSaturation(int8_t value) {
    if (!_sensor) return false;
    if (value < -2) value = -2;
    if (value >  2) value =  2;
    return _sensor->set_saturation(_sensor, value) == 0;
}

bool CameraController::setSpecialEffect(int effect) {
    if (!_sensor) return false;
    if (effect < 0) effect = 0;
    if (effect > 6) effect = 6;
    return _sensor->set_special_effect(_sensor, effect) == 0;
}

bool CameraController::setVFlip(bool on) {
    if (!_sensor) return false;
    return _sensor->set_vflip(_sensor, on ? 1 : 0) == 0;
}

bool CameraController::setHFlip(bool on) {
    if (!_sensor) return false;
    return _sensor->set_hmirror(_sensor, on ? 1 : 0) == 0;
}

bool CameraController::setWhiteBalance(WhiteBalance mode) {
    if (!_sensor) return false;
    return _sensor->set_wb_mode(_sensor, (int)mode) == 0;
}

bool CameraController::setAecMode(bool on) {
    if (!_sensor) return false;
    return _sensor->set_exposure_ctrl(_sensor, on ? 1 : 0) == 0;
}

bool CameraController::setAecValue(uint16_t value) {
    if (!_sensor) return false;
    if (value > 1200) value = 1200;
    return _sensor->set_aec_value(_sensor, value) == 0;
}

bool CameraController::setAgcGain(uint8_t gain) {
    if (!_sensor) return false;
    if (gain > 30) gain = 30;
    return _sensor->set_agc_gain(_sensor, gain) == 0;
}

void CameraController::setXCLKFreq(XCLKFreq freq) {
    _config.xclk_freq_hz = (int)freq;
    // NOTE: changing XCLK after begin() requires end() + begin()
    // for the new frequency to take effect.
}

void CameraController::setFbCount(uint8_t count) {
    if (count < 1) count = 1;
    if (count > 4) count = 4;
    _config.fb_count = count;
    // NOTE: changing fb_count after begin() requires end() + begin()
    // for the new buffer pool size to take effect.
}

// ── Sensor settings getters ─────────────────────────────────────────────

CameraController::FrameSize CameraController::getFrameSize() const {
    return (FrameSize)_config.frame_size;
}

uint8_t CameraController::getJpegQuality() const {
    return _config.jpeg_quality;
}

int8_t CameraController::getBrightness() const {
    if (!_sensor) return 0;
    return (int8_t)_sensor->status.brightness;
}

int8_t CameraController::getContrast() const {
    if (!_sensor) return 0;
    return (int8_t)_sensor->status.contrast;
}

int8_t CameraController::getSaturation() const {
    if (!_sensor) return 0;
    return (int8_t)_sensor->status.saturation;
}

int CameraController::getSpecialEffect() const {
    if (!_sensor) return 0;
    return (int)_sensor->status.special_effect;
}

bool CameraController::getVFlip() const {
    if (!_sensor) return false;
    return _sensor->status.vflip != 0;
}

bool CameraController::getHFlip() const {
    if (!_sensor) return false;
    return _sensor->status.hmirror != 0;
}

CameraController::WhiteBalance CameraController::getWhiteBalance() const {
    if (!_sensor) return WB_AUTO;
    return (WhiteBalance)_sensor->status.wb_mode;
}

CameraController::XCLKFreq CameraController::getXCLKFreq() const {
    return (XCLKFreq)_config.xclk_freq_hz;
}

// ── Flash LED ───────────────────────────────────────────────────────────

void CameraController::flashOn() {
    if (_ledPin >= 0) digitalWrite(_ledPin, HIGH);
}

void CameraController::flashOff() {
    if (_ledPin >= 0) digitalWrite(_ledPin, LOW);
}
