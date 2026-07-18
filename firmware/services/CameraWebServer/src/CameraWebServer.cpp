/*
 * CameraWebServer.cpp
 *
 * Full implementation.  See CameraWebServer.h for architecture,
 * frame lifecycle, and usage documentation.
 */

#include "CameraWebServer.h"

// ── Static member initialisation ──────────────────────────────────────

QueueHandle_t CameraWebServer::s_frameQueue = nullptr;

// ── HTML page ─────────────────────────────────────────────────────────

const char CameraWebServer::INDEX_HTML[] = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Camera</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div id="status">CameraWebServer v1.0 &mdash; ESP32-S3 CAM</div>

<div class="card">
    <h2>Snapshot</h2>
    <img id="snapshot" src="/camera/capture" alt="Camera snapshot" />
    <p class="hint">Auto-refreshes every second. Click image to open full size.</p>
</div>

<div class="card">
    <h2>Live Stream</h2>
    <p>
        <a href="/camera/stream" target="_blank" class="btn">Open MJPEG Stream</a>
        <span class="hint">(opens in new tab — save as .mjpeg to record)</span>
    </p>
</div>

<div class="card">
    <h2>WiFi</h2>
    <button class="btn btn-reset" onclick="resetWiFi()">Reset WiFi Credentials</button>
    <p class="hint">Clears saved credentials and reboots into setup AP mode.</p>
</div>

<div class="footer">
    FreeRTOS: camera_task (Core 0) + web_task (Core 1)
    &mdash; xQueueOverwrite/xQueuePeek — zero-copy frame sharing
</div>

<script>
function refresh() {
    var img = document.getElementById('snapshot');
    img.src = '/camera/capture?t=' + Date.now();
}
setInterval(refresh, 1000);

function resetWiFi() {
    if (!confirm('Clear WiFi credentials and reboot?')) return;
    fetch('/wifi/reset', { method: 'POST' })
        .then(function() { document.getElementById('status').textContent = 'Rebooting...'; })
        .catch(function() { alert('Reset failed'); });
}
</script>
</body>
</html>
)raw";

const char CameraWebServer::STYLE_CSS[] = R"raw(
* { margin:0; padding:0; box-sizing:border-box; }
body {
    background:#111; color:#eee; font-family:monospace;
    max-width:720px; margin:0 auto; padding:16px;
}
#status {
    background:#222; padding:10px 14px; border-radius:6px;
    margin-bottom:16px; font-size:13px; color:#5f5;
}
.card {
    background:#1a1a1a; border-radius:8px; padding:16px;
    margin-bottom:16px;
}
h2 {
    font-size:15px; color:#55f; margin-bottom:10px;
    border-bottom:1px solid #333; padding-bottom:6px;
}
img {
    width:100%; max-width:640px; display:block;
    border-radius:4px; border:1px solid #333; margin:8px 0;
}
.btn {
    display:inline-block; padding:8px 16px; border-radius:4px;
    color:#eee; text-decoration:none; font-size:13px;
    border:none; cursor:pointer;
    background:#25a; margin:4px 2px;
}
.btn:hover { opacity:0.85; }
.btn-reset { background:#a33; }
.hint { font-size:11px; color:#888; margin-top:4px; }
.footer {
    text-align:center; font-size:10px; color:#555;
    margin-top:20px; padding-top:12px; border-top:1px solid #222;
}
)raw";

// ── Constructor ───────────────────────────────────────────────────────

CameraWebServer::CameraWebServer() {}

// ── Public: begin() — one-shot setup ──────────────────────────────────

void CameraWebServer::begin() {
    Serial.begin(921600);
    delay(1000);

    _setupWiFi();
    _setupCamera();
    _setupRoutes();

    // Depth-1 queue — only the latest frame matters for web display
    _frameQueue = xQueueCreate(1, sizeof(camera_fb_t*));
    s_frameQueue = _frameQueue;

    _server.begin();

    // Spawn FreeRTOS tasks
    xTaskCreatePinnedToCore(
        _cameraTaskEntry, "cam_task", CAM_TASK_STACK,
        this, CAM_TASK_PRIO, &_camTaskHandle, 0);

    xTaskCreatePinnedToCore(
        _webTaskEntry, "web_task", WEB_TASK_STACK,
        this, WEB_TASK_PRIO, &_webTaskHandle, 1);

    Serial.println();
    Serial.println("CameraWebServer ready:");
    Serial.printf("  http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("  Endpoints: /  /camera/capture  /camera/stream  /wifi/reset");
}

// ── WiFi setup (same pattern as MoistureMonitorAgent) ──────────────────

void CameraWebServer::_setupWiFi() {
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setTitle("ESP32-S3-CAM");
    const char* menu[] = {"wifi", "wifinoscan"};
    wm.setMenu(menu, 2);

    Serial.println("WiFi: connecting...");
    if (!wm.autoConnect("ESP32-S3-CAM-Setup")) {
        Serial.println("WiFi: timeout — restarting");
        delay(3000);
        ESP.restart();
    }
    WiFi.mode(WIFI_STA);
    Serial.printf("WiFi: connected | SSID=%s | IP=%s\n",
        WiFi.SSID().c_str(),
        WiFi.localIP().toString().c_str());
}

// ── Camera setup ──────────────────────────────────────────────────────

void CameraWebServer::_setupCamera() {
    if (!_cam.begin()) {
        Serial.println("Camera: init FAILED — check camera ribbon cable");
        return;
    }

    _cam.setFrameSize(CameraController::QVGA);    // 320×240
    _cam.setJpegQuality(12);                       // 0=best, 63=worst
    _cam.setVFlip(true);                           // right-side up on S3 CAM Dev Kit
    _cam.setHFlip(false);

    Serial.printf("Camera: OK | %dx%d JPEG quality=%d\n",
        320, 240, _cam.getJpegQuality());
}

// ── HTTP routes ───────────────────────────────────────────────────────

void CameraWebServer::_setupRoutes() {
    _server.on("/", HTTP_GET,
        [this]() { _handleRoot(); });

    _server.on("/style.css", HTTP_GET,
        [this]() { _handleStyle(); });

    _server.on("/camera/capture", HTTP_GET,
        [this]() { _handleCapture(); });

    _server.on("/camera/stream", HTTP_GET,
        [this]() { _handleStream(); });

    _server.on("/wifi/reset", HTTP_POST,
        [this]() { _handleWifiReset(); });
}

// ── Route handlers ────────────────────────────────────────────────────

void CameraWebServer::_handleRoot() {
    _server.send(200, "text/html", INDEX_HTML);
}

void CameraWebServer::_handleStyle() {
    _server.send(200, "text/css", STYLE_CSS);
}

/*
 * Snapshot: peeks the frame queue (read without removing),
 * sends the JPEG buffer as a complete HTTP response, and
 * closes the connection.  Raw headers + client.write() are
 * used to avoid copying the JPEG data (10–20 KB in PSRAM)
 * into a temporary String.
 *
 * If no frame is available yet (camera still booting), returns 503.
 */
void CameraWebServer::_handleCapture() {
    camera_fb_t* fb = nullptr;
    if (xQueuePeek(s_frameQueue, &fb, 0) != pdTRUE || !fb) {
        _server.send(503, "text/plain", "No frame available yet");
        return;
    }

    // Build response manually to avoid copying the JPEG buffer
    // (fb->buf lives in PSRAM; client.write() sends it directly).
    WiFiClient client = _server.client();
    client.printf(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.stop();
}

/*
 * MJPEG live stream: sends HTTP headers directly to the client socket
 * (bypassing the WebServer which would use chunked transfer encoding),
 * then loops while the client stays connected, peeking the frame queue
 * and sending each frame as a MIME multipart part.
 *
 * Frame rate is bounded by the camera_task capture rate (~20 fps).
 * The JPEG data at fb->buf is in PSRAM — client.write() sends it
 * without copying.
 */
void CameraWebServer::_handleStream() {
    WiFiClient client = _server.client();

    // Write HTTP headers manually to avoid chunked transfer encoding
    // that WebServer.send() with CONTENT_LENGTH_UNKNOWN would inject.
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
        camera_fb_t* fb = nullptr;
        if (xQueuePeek(s_frameQueue, &fb, pdMS_TO_TICKS(500)) != pdTRUE || !fb) {
            continue;  // no frame yet — wait and retry
        }

        client.print("--frame\r\n");
        client.print("Content-Type: image/jpeg\r\n");
        client.printf("Content-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");

        // Throttle to ~5 fps for browser compatibility.
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void CameraWebServer::_handleWifiReset() {
    _server.send(200, "text/plain", "Clearing WiFi credentials and rebooting...");
    delay(500);
    WiFiManager wm;
    wm.resetSettings();
    delay(500);
    ESP.restart();
}

// ── FreeRTOS task entries ─────────────────────────────────────────────

void CameraWebServer::_cameraTaskEntry(void* pv) {
    static_cast<CameraWebServer*>(pv)->_cameraTask();
}

void CameraWebServer::_webTaskEntry(void* pv) {
    static_cast<CameraWebServer*>(pv)->_webTask();
}

// ── camera_task — producer ────────────────────────────────────────────

/*
 * Captures frames continuously at ~20 fps.
 *
 * Frame lifecycle (single owner — this task):
 *   1. cam.capture()      → takes a buffer from the DMA pool
 *   2. xQueuePeek(...)    → get OLD frame pointer from queue
 *   3. xQueueOverwrite()  → push NEW pointer into queue
 *   4. cam.release(old)   → return OLD buffer to DMA pool
 *
 * On the very first iteration, the queue is empty so old == nullptr
 * and step 4 is a no-op.  Thereafter, each frame is released exactly
 * when its successor arrives.
 *
 * No mutexes: this is the only writer to the queue.
 * Web handlers are read-only peekers — no removal, no contention.
 */
void CameraWebServer::_cameraTask() {
    for (;;) {
        camera_fb_t* fb = _cam.capture();
        if (!fb) continue;  // capture() failed — retry next tick

        // Read the OLD frame pointer BEFORE overwriting the slot.
        // xQueuePeek does NOT remove — it just gives us the pointer.
        camera_fb_t* old = nullptr;
        xQueuePeek(_frameQueue, &old, 0);

        // Overwrite the single queue slot with the new frame.
        // This always succeeds (depth-1 queue).
        xQueueOverwrite(_frameQueue, &fb);

        // Release the OLD frame back to the DMA pool.
        // On first iteration, old is nullptr → no-op.
        if (old) {
            _cam.release(old);
        }

        // ~20 fps ceiling.  Without a delay, the camera can
        // produce frames faster than a browser can consume.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── web_task — consumer ──────────────────────────────────────────────

/*
 * Drains the Arduino WebServer event loop.  All HTTP handlers run
 * in this task's context.  server.handleClient() processes one
 * pending request per call; the tight loop with delay(1) ensures
 * minimal latency.
 */
void CameraWebServer::_webTask() {
    for (;;) {
        _server.handleClient();
        delay(1);
    }
}
