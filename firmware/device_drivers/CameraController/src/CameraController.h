/*
 * CameraController.h — ESP32 camera driver wrapper
 *
 * Single-responsibility wrapper around the esp_camera library (bundled
 * with the Arduino-ESP32 framework).  Owns camera_config_t and sensor_t.
 * Provides board presets for common ESP32 camera modules, frame capture
 * with raw camera_fb_t* access, and get/set methods for all sensor
 * settings (brightness, contrast, saturation, flip, effects, etc.).
 *
 * ==================== Supported Boards ====================
 *
 *   AI_THINKER          — ESP32-CAM with OV2640 (ESP32 chip)
 *   ESP32S3_CAM_LCD     — Espressif ESP32-S3 CAM LCD (ESP32-S3 chip)
 *   ESP32S3_EYE         — ESP-EYE for S3 / nulllaborg ESP32S3-CAM (ESP32-S3)
 *   NULLLAB_ESP32S3_CAM — nulllaborg ESP32S3-CAM (same pins as EYE, adds flash LED GPIO 3)
 *   XIAO_ESP32S3        — Seeed XIAO ESP32S3 + OV2640 module
 *   M5STACK_CAMS3       — M5Stack Camera Unit for S3
 *   CUSTOM              — User provides all pins via setPin()
 *
 * ==================== Frame Buffer Lifecycle ====================
 * capture() returns a pointer to the DMA frame buffer pool.  You MUST
 * call release(fb) after processing each frame — the pool has only 2
 * buffers.  After 2 unreleased captures, capture() returns NULL until
 * a buffer is returned.
 *
 *   camera_fb_t* fb = cam.capture();
 *   if (fb) {
 *       Serial.write(fb->buf, fb->len);    // fb->buf = JPEG bytes
 *       // fb->width, fb->height, fb->len, fb->format
 *       cam.release(fb);                    // REQUIRED
 *   }
 *
 * ==================== Usage Example ====================
 *
 *   #include <CameraController.h>
 *
 *   CameraController cam(CameraController::AI_THINKER);
 *
 *   void setup() {
 *       cam.begin();
 *       cam.setFrameSize(CameraController::QVGA);
 *       cam.setJpegQuality(12);
 *   }
 *
 *   void loop() {
 *       camera_fb_t* fb = cam.capture();
 *       if (fb) {
 *           Serial.printf("Frame: %u bytes\n", fb->len);
 *           cam.release(fb);
 *       }
 *       delay(1000);
 *   }
 *
 * ==================== FreeRTOS Pattern (application level) ========
 * The camera is synchronous (no internal FreeRTOS queues).  For
 * multi-threaded use, create a camera task that captures frames and
 * sends them to a consumer queue:
 *
 *   static void cameraTask(void*) {
 *       for (;;) {
 *           camera_fb_t* fb = cam.capture();
 *           if (fb) xQueueSend(queue, &fb, portMAX_DELAY);
 *       }
 *   }
 *
 *   static void consumerTask(void*) {
 *       for (;;) {
 *           camera_fb_t* fb;
 *           if (xQueueReceive(queue, &fb, portMAX_DELAY)) {
 *               doSomething(fb->buf, fb->len);
 *               cam.release(fb);
 *           }
 *       }
 *   }
 */

#pragma once

#include <Arduino.h>
#include <esp_camera.h>

class CameraController {
public:
    // ── Board model presets ─────────────────────────────────────

    enum BoardModel {
        AI_THINKER,              // ESP32-CAM, OV2640, flash LED on GPIO 4
        ESP32S3_CAM_LCD,         // Espressif ESP32-S3 CAM LCD
        ESP32S3_EYE,             // ESP-EYE for ESP32-S3 / nulllaborg ESP32S3-CAM
        NULLLAB_ESP32S3_CAM,     // nulllaborg ESP32S3-CAM (flash LED on GPIO 3)
        XIAO_ESP32S3,            // Seeed XIAO ESP32S3 + OV2640 module
        M5STACK_CAMS3,           // M5Stack Camera Unit for S3
        CUSTOM                   // All pins must be set via setPin()
    };

    // ── Frame size enum ─────────────────────────────────────────
    // Values match the esp_camera framesize_t enum IDs from sensor.h.
    // These are sequential C enum values — do NOT reorder.
    enum FrameSize : uint8_t {
        SIZE_96X96   = 0,       // 96×96
        QQVGA        = 1,       // 160×120
        QCIF         = 3,       // 176×144
        HQVGA        = 4,       // 240×176
        SIZE_240X240 = 5,       // 240×240
        QVGA         = 6,       // 320×240  (default)
        CIF          = 8,       // 400×296
        HVGA         = 9,       // 480×320
        VGA          = 10,      // 640×480
        SVGA         = 11,      // 800×600
        XGA          = 12,      // 1024×768
        HD           = 13,      // 1280×720
        SXGA         = 14,      // 1280×1024
        UXGA         = 15,      // 1600×1200
        FHD          = 16,      // 1920×1080
        QXGA         = 19,      // 2048×1536  (3 MP)
        QHD          = 20,      // 2560×1440  (5 MP)
        WQXGA        = 21,      // 2560×1600  (5 MP)
        QSXGA        = 23,      // 2560×2048  (5 MP)
    };

    // ── White balance mode (sensor IC register) ─────────────────

    enum WhiteBalance : uint8_t {
        WB_AUTO   = 0,     // Automatic (default)
        WB_SUNNY  = 1,     // Sunny / outdoor
        WB_CLOUDY = 2,     // Cloudy
        WB_OFFICE = 3,     // Office / fluorescent
        WB_HOME   = 4,     // Home / incandescent
    };

    // ── XCLK master clock frequency (set BEFORE begin()) ─────────
    // Default: 20 MHz.  10 MHz may reduce EMI on some boards.
    // Must be set before begin() — changes after init require
    // a full end() + begin() cycle.

    enum XCLKFreq : uint32_t {
        XCLK_10MHZ = 10000000,
        XCLK_20MHZ = 20000000,
    };

    // ── JPEG quality constants ──────────────────────────────────

    static const uint8_t JPEG_BEST  = 0;
    static const uint8_t JPEG_WORST = 63;

    /*
     * Constructor.  model selects the camera board pinout.
     * For CUSTOM, call setPin() before begin().
     */
    CameraController(BoardModel model = AI_THINKER);

    /*
     * Set all camera pins.  Only required for CUSTOM model or to
     * override specific pins on a known board.
     *
     *   d0–d7      — 8-bit parallel data bus (Y2–Y9 in schematics)
     *   vsync      — vertical sync input
     *   href       — horizontal reference input
     *   pclk       — pixel clock input
     *   xclk       — master clock output (MCU → camera)
     *   sda / scl  — SCCB (I2C) control bus
     *   pwdn       — power down (pass -1 if not connected)
     *   reset      — reset (pass -1 if not connected)
     *   led        — flash LED (pass -1 if not used)
     */
    void setPin(int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7,
                int vsync, int href, int pclk, int xclk,
                int sda, int scl, int pwdn = -1, int reset = -1, int led = -1);

    // ── Lifecycle ────────────────────────────────────────────────

    /*
     * Initialize the camera with the configured pinout.
     * Defaults: QVGA (320x240), JPEG quality 12, PIXFORMAT_JPEG,
     *           2 frame buffers in PSRAM.
     * Returns true on success.
     */
    bool begin();

    /*
     * De-initialize the camera.  Frees the frame buffer pool and
     * puts the sensor in power-down state.
     */
    void end();

    // ── Frame capture ────────────────────────────────────────────

    /*
     * Capture a single frame.  Returns a pointer to a camera_fb_t
     * struct, or NULL on failure.
     *
     *   fb->buf    — uint8_t*  raw image data
     *   fb->len    — size_t    buffer length in bytes
     *   fb->width  — size_t    image width
     *   fb->height — size_t    image height
     *   fb->format — pixformat_t  (PIXFORMAT_JPEG by default)
     *
     * The caller MUST call release() after processing each frame.
     * The DMA pool has 2 buffers — after 2 unreleased captures,
     * capture() returns NULL until a buffer is returned.
     */
    camera_fb_t* capture();

    /*
     * Return a frame buffer to the DMA pool.  Call exactly once for
     * every successful capture() — never call on NULL pointers.
     */
    void release(camera_fb_t* fb);

    // ── Sensor settings setters ──────────────────────────────────

    bool setFrameSize(FrameSize size);
    bool setJpegQuality(uint8_t quality);   // 0 (best) – 63 (worst)
    bool setBrightness(int8_t value);       // -2 to 2
    bool setContrast(int8_t value);         // -2 to 2
    bool setSaturation(int8_t value);       // -2 to 2
    bool setSpecialEffect(int effect);      // 0=off, 1=neg, 2=b&w, 3=red, 4=green, 5=blue, 6=sepia
    bool setVFlip(bool on);
    bool setHFlip(bool on);
    bool setWhiteBalance(WhiteBalance mode);
    bool setAecMode(bool on);                // auto-exposure on/off
    bool setAecValue(uint16_t value);        // manual exposure 0-1200 (shutter speed)
    bool setAgcGain(uint8_t gain);           // manual gain 0-30
    void setXCLKFreq(XCLKFreq freq);        // call BEFORE begin()
    void setFbCount(uint8_t count);          // call BEFORE begin(), default 2

    // ── Sensor settings getters ──────────────────────────────────

    FrameSize getFrameSize() const;
    uint8_t   getJpegQuality() const;
    int8_t    getBrightness() const;
    int8_t    getContrast() const;
    int8_t    getSaturation() const;
    int       getSpecialEffect() const;
    bool      getVFlip() const;
    bool      getHFlip() const;
    WhiteBalance getWhiteBalance() const;
    XCLKFreq  getXCLKFreq() const;

    // ── Flash LED (board-dependent) ──────────────────────────────

    void flashOn();
    void flashOff();

    // ── State ────────────────────────────────────────────────────

    bool      isInitialized() const { return _initialized; }
    BoardModel getModel()      const { return _model; }

private:
    BoardModel      _model;
    camera_config_t _config;
    sensor_t*       _sensor;
    bool            _initialized;
    int             _ledPin;   // flash LED GPIO, -1 if none

    void _applyBoardPreset();
};
