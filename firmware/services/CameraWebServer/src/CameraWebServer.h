/*
 * CameraWebServer.h — WiFi-connected camera web server
 *
 * Composes CameraController with an Arduino WebServer to provide
 * snapshot and MJPEG live-stream endpoints over WiFi.  Uses a
 * FreeRTOS producer-consumer architecture:
 *
 *   camera_task (Core 0):  captures → xQueueOverwrite (depth=1)
 *   web_task    (Core 1):  handles HTTP, peeks queue (never removes)
 *
 * This design has exactly ONE OWNER of the DMA frame buffer pool
 * (camera_task is the only task that calls capture() and release()).
 * HTTP route handlers are read-only peeks — no mutexes, no leaks.
 *
 * ==================== Usage ====================
 *
 *   #include <CameraWebServer.h>
 *
 *   CameraWebServer server;
 *
 *   void setup() {
 *       server.begin();  // WiFi connect, cam init, routes, tasks
 *       // Serial prints:  CameraWebServer ready: http://192.168.1.42
 *   }
 *
 *   void loop() {
 *       vTaskDelay(pdMS_TO_TICKS(1000));
 *   }
 *
 * ==================== Endpoints ====================
 *
 *   GET  /                  HTML dashboard with auto-refreshing snapshot
 *   GET  /style.css         Dark-theme stylesheet
 *   GET  /camera/capture    Single JPEG snapshot
 *   GET  /camera/stream     MJPEG live stream (~20 fps, saves as .mjpeg)
 *   POST /wifi/reset        Clear WiFi credentials → reboot
 *
 * ==================== Frame Lifecycle ====================
 *
 *   1. camera_task captures a frame → buffer taken from DMA pool
 *   2. Peek current frame pointer from queue (old frame)
 *   3. xQueueOverwrite the queue slot with the new pointer
 *   4. Release the OLD frame back to DMA pool
 *   5. HTTP handlers peek the queue to read current frame
 *   6. Handlers NEVER release — release owned exclusively by camera_task
 *
 *   Queue depth = 1.  xQueueOverwrite always succeeds (replaces
 *   oldest).  If a client is slow, the frame is simply replaced
 *   at the camera's capture rate (~20 fps).  No backpressure,
 *   no stalls, no buffer exhaustion.
 *
 * ==================== WiFi Setup ====================
 *
 *   Uses tzapu/WiFiManager with 180-second config portal timeout.
 *   On first boot (no saved credentials):
 *     • Creates AP "ESP32-S3-CAM-Setup"
 *     • Connect phone/laptop, captive portal opens
 *     • Enter WiFi credentials → ESP32 reboots and connects
 *
 *   Credentials persist in NVS across power cycles.
 *   POST /wifi/reset clears them and reboots into AP mode.
 *
 * ==================== Hardware ====================
 *
 *   nulllaborg ESP32-S3 CAM (OV2640 / OV3660).
 *   USB-C for power + programming + serial monitor (native USB GPIO19/20).
 *   Board preset: NULLLAB_ESP32S3_CAM.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <CameraController.h>

class CameraWebServer {
public:
    CameraWebServer();

    /*
     * One-shot setup: connects WiFi, initialises camera, configures
     * HTTP routes, spawns FreeRTOS tasks.  Prints the access URL
     * to serial on success.
     */
    void begin();

private:
    // ── Task parameters ──────────────────────────────────────────

    static const uint16_t CAM_TASK_STACK  = 4096;
    static const uint16_t WEB_TASK_STACK  = 8192;
    static const uint8_t  CAM_TASK_PRIO   = 1;
    static const uint8_t  WEB_TASK_PRIO   = 2;

    // ── Private helpers ──────────────────────────────────────────

    void _setupWiFi();
    void _setupCamera();
    void _setupRoutes();

    // ── Route handlers ───────────────────────────────────────────

    void _handleRoot();
    void _handleStyle();
    void _handleCapture();
    void _handleStream();
    void _handleWifiReset();

    // ── FreeRTOS task entry points + implementations ─────────────

    static void _cameraTaskEntry(void* pv);
    static void _webTaskEntry(void* pv);
    void _cameraTask();
    void _webTask();


    // ── Members ──────────────────────────────────────────────────

    WebServer        _server{80};
    CameraController _cam{CameraController::NULLLAB_ESP32S3_CAM};
    QueueHandle_t    _frameQueue{nullptr};
    TaskHandle_t     _camTaskHandle{nullptr};
    TaskHandle_t     _webTaskHandle{nullptr};

    // Static reference so route lambdas can access the queue
    // without capturing this-> (lambdas are C function pointers).
    static QueueHandle_t s_frameQueue;

    // ── HTML / CSS (compile-time string constants) ───────────────

    static const char INDEX_HTML[];
    static const char STYLE_CSS[];
};
