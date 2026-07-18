/*
 * CameraBluetoothServer.cpp
 *
 * Full implementation.  See CameraBluetoothServer.h for architecture,
 * BLE service layout, and frame chunking protocol.
 */

#include "CameraBluetoothServer.h"

// ── BLE Service & Characteristic UUIDs ─────────────────────────────────

#define CAMERA_SERVICE_UUID       "0000ca00-0000-1000-8000-00805f9b34fb"
#define CONTROL_CHAR_UUID         "0000ca01-0000-1000-8000-00805f9b34fb"
#define SETTINGS_CHAR_UUID        "0000ca02-0000-1000-8000-00805f9b34fb"
#define FRAME_CHAR_UUID           "0000ca03-0000-1000-8000-00805f9b34fb"
#define INFO_CHAR_UUID            "0000ca04-0000-1000-8000-00805f9b34fb"
#define PARAMS_CHAR_UUID          "0000ca05-0000-1000-8000-00805f9b34fb"

// ── Static member initialisation ───────────────────────────────────────

QueueHandle_t CameraBluetoothServer::s_frameQueue = nullptr;

// ── Constructor ────────────────────────────────────────────────────────

CameraBluetoothServer::CameraBluetoothServer() {}

// ── Public: begin() — one-shot setup ───────────────────────────────────

void CameraBluetoothServer::begin() {
    Serial.begin(921600);
    delay(1000);

    _setupCamera();
    _loadSettings();  // override compile-time defaults with NVS values
    _setupBLE();

    // Depth-1 queue — only the latest frame matters
    _frameQueue = xQueueCreate(1, sizeof(camera_fb_t*));
    s_frameQueue = _frameQueue;
    _frameMutex = xSemaphoreCreateMutex();

    // Start advertising
    _bleServer->getAdvertising()->start();

    // Spawn FreeRTOS tasks
    xTaskCreatePinnedToCore(
        _cameraTaskEntry, "cam_task", CAM_TASK_STACK,
        this, CAM_TASK_PRIO, &_camTaskHandle, 0);

    xTaskCreatePinnedToCore(
        _bleTaskEntry, "ble_task", BLE_TASK_STACK,
        this, BLE_TASK_PRIO, &_bleTaskHandle, 1);

    Serial.println();
    Serial.println("CameraBluetoothServer ready:");
    Serial.println("  Advertising as \"ESP32-CAM\"");
    Serial.println("  Connect with a BLE client (nRF Connect, LightBlue, bleak)");
    Serial.println("  Service UUID: 0000CA00-0000-1000-8000-00805F9B34FB");
    Serial.println("  Write \"snapshot\" to CA01 control char to capture");
}

// ── Camera setup ───────────────────────────────────────────────────────

void CameraBluetoothServer::_setupCamera() {
    _cam.setFbCount(4);  // 4 DMA buffers for high-res stability

    if (!_cam.begin()) {
        Serial.println("Camera: init FAILED — check camera ribbon cable");
        return;
    }

    _cam.setFrameSize(_frameSize);
    _cam.setJpegQuality(_jpegQuality);
    _cam.setVFlip(_vflip);
    _cam.setHFlip(_hflip);
    _cam.setBrightness(_brightness);
    _cam.setContrast(_contrast);
    _cam.setSaturation(_saturation);
    _cam.setSpecialEffect(_specialEffect);
    _cam.setWhiteBalance(_wb);
    _cam.setAecMode(true);  // auto exposure by default

    Serial.printf("Camera: OK | %dx%d JPEG quality=%d\n",
        (int)CameraController::QVGA, (int)CameraController::QVGA, _cam.getJpegQuality());
    Serial.printf("         vflip=%d hflip=%d wb=%s\n",
        _vflip, _hflip, _wbToString().c_str());
}

// ── NVS load/save ─────────────────────────────────────────────────────

void CameraBluetoothServer::_loadSettings() {
    _store.begin("cam_server");

    // Cast enums to int for NVS storage
    _frameSize      = (CameraController::FrameSize)_store.get<int>("size",  (int)CameraController::QVGA);
    _jpegQuality    = (uint8_t) _store.get<int>("qual",  20);
    _brightness     = (int8_t)  _store.get<int>("brt",   0);
    _contrast       = (int8_t)  _store.get<int>("cnt",   0);
    _saturation     = (int8_t)  _store.get<int>("sat",   0);
    _specialEffect  =          _store.get<int>("eff",    0);
    _vflip          =          _store.get<bool>("vflip", true);
    _hflip          =          _store.get<bool>("hflip", false);
    _wb             = (CameraController::WhiteBalance)_store.get<int>("wb", (int)CameraController::WB_AUTO);
    _aecOn          =          _store.get<bool>("aec",   true);
    _shutter        = (uint16_t)_store.get<int>("sht",   0);
    _gain           = (uint8_t) _store.get<int>("gain",  0);
    _flashGain      = (uint8_t) _store.get<int>("fgain", 20);
    _flashShutter   = (uint16_t)_store.get<int>("fsht",  800);

    _store.end();

    // Apply loaded settings to the camera
    _cam.setFrameSize(_frameSize);
    _cam.setJpegQuality(_jpegQuality);
    _cam.setBrightness(_brightness);
    _cam.setContrast(_contrast);
    _cam.setSaturation(_saturation);
    _cam.setSpecialEffect(_specialEffect);
    _cam.setVFlip(_vflip);
    _cam.setHFlip(_hflip);
    _cam.setWhiteBalance(_wb);
    _cam.setAecMode(_aecOn);
    if (!_aecOn) {
        _cam.setAecValue(_shutter);
        _cam.setAgcGain(_gain);
    }

    Serial.println("NVS: settings loaded");
}

void CameraBluetoothServer::_saveSettings() {
    _store.begin("cam_server");

    _store.put<int>("size",  (int)_frameSize);
    _store.put<int>("qual",  _jpegQuality);
    _store.put<int>("brt",   _brightness);
    _store.put<int>("cnt",   _contrast);
    _store.put<int>("sat",   _saturation);
    _store.put<int>("eff",   _specialEffect);
    _store.put<bool>("vflip", _vflip);
    _store.put<bool>("hflip", _hflip);
    _store.put<int>("wb",    (int)_wb);
    _store.put<bool>("aec",  _aecOn);
    _store.put<int>("sht",   _shutter);
    _store.put<int>("gain",  _gain);
    _store.put<int>("fgain", _flashGain);
    _store.put<int>("fsht",  _flashShutter);

    _store.end();
    Serial.println("NVS: settings saved");
}

// ── BLE setup ──────────────────────────────────────────────────────────

void CameraBluetoothServer::_setupBLE() {
    BLEDevice::init("ESP32-CAM");
    _bleServer = BLEDevice::createServer();

    // Restart advertising on disconnect; cancel in-flight snapshot
    class SrvCB : public BLEServerCallbacks {
        CameraBluetoothServer* _srv;
    public:
        SrvCB(CameraBluetoothServer* srv) : _srv(srv) {}
        void onConnect(BLEServer* s) override {
            (void)s;
            Serial.println("BLE: client connected");
        }
        void onDisconnect(BLEServer* s) override {
            (void)s;
            _srv->_pendingSnapshot = false;
            Serial.println("BLE: client disconnected, restarting advertising");
            s->getAdvertising()->start();
        }
    };
    _bleServer->setCallbacks(new SrvCB(this));

    _cameraService = _bleServer->createService(CAMERA_SERVICE_UUID);

    // CA01 — Control (write)
    _controlChr = _cameraService->createCharacteristic(
        CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    {
        class CtrlCB : public BLECharacteristicCallbacks {
            CameraBluetoothServer* _srv;
        public:
            CtrlCB(CameraBluetoothServer* srv) : _srv(srv) {}
            void onWrite(BLECharacteristic* c) override { (void)c; _srv->_onControlWrite(); }
        };
        _controlChr->setCallbacks(new CtrlCB(this));
    }

    // CA02 — Settings (write + read)
    _settingsChr = _cameraService->createCharacteristic(
        SETTINGS_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
    {
        class SetCB : public BLECharacteristicCallbacks {
            CameraBluetoothServer* _srv;
        public:
            SetCB(CameraBluetoothServer* srv) : _srv(srv) {}
            void onWrite(BLECharacteristic* c) override {
                _srv->_onSettingsWrite(c->getValue());
            }
            void onRead(BLECharacteristic* c) override  { (void)c; _srv->_onSettingsRead(); }
        };
        _settingsChr->setCallbacks(new SetCB(this));
    }
    _settingsChr->setValue(_buildSettingsString());

    // CA03 — Frame Data (notify). NimBLE auto-adds BLE2902 CCCD.
    _frameChr = _cameraService->createCharacteristic(
        FRAME_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);

    // CA04 — Frame Info (read)
    _infoChr = _cameraService->createCharacteristic(
        INFO_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _infoChr->setValue("{\"size\":0,\"w\":0,\"h\":0,\"q\":0}");

    // CA05 — Params schema (read-only, driver-owned)
    _paramsChr = _cameraService->createCharacteristic(
        PARAMS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _paramsChr->setValue(_buildParamsJson());

    _cameraService->start();

    Serial.println("BLE: Camera service started");
}

// ── BLE callbacks ──────────────────────────────────────────────────────

void CameraBluetoothServer::_onControlWrite() {
    String cmd = _controlChr->getValue();
    cmd.trim();
    cmd.toLowerCase();

    Serial.printf("BLE: control command = \"%s\"\n", cmd.c_str());

    if (cmd == "snapshot") {
        _pendingSnapshot = true;
    } else if (cmd == "flash_on") {
        // Save current AEC state before disabling it.
        // If AEC is auto-adjusting, lock exposure so the flash
        // doesn't get compensated away by the sensor.
        _savedAecOn   = _aecOn;
        _savedShutter = _shutter;
        _savedGain    = _gain;

        if (_aecOn) {
            _cam.setAecMode(false);
            uint16_t expShutter = (_shutter != 0) ? _shutter : _flashShutter;
            uint8_t  expGain    = (_gain != 0)    ? _gain    : _flashGain;
            _cam.setAecValue(expShutter);
            _cam.setAgcGain(expGain);
        }

        _cam.flashOn();
        _flashOn = true;
    } else if (cmd == "flash_off") {
        _cam.flashOff();
        _flashOn = false;

        if (_savedAecOn) {
            _cam.setAecMode(true);
        }
    } else if (cmd == "reset") {
        _frameSize      = CameraController::QVGA;
        _jpegQuality    = 20;
        _brightness     = 0;
        _contrast       = 0;
        _saturation     = 0;
        _specialEffect  = 0;
        _vflip          = true;
        _hflip          = false;
        _wb             = CameraController::WB_AUTO;
        _flashOn        = false;
        _aecOn          = true;
        _shutter        = 0;
        _gain           = 0;
        _flashGain      = 20;
        _flashShutter   = 800;

        CameraController::FrameSize oldSize = _cam.getFrameSize();
        _cam.setJpegQuality(_jpegQuality);
        _cam.setBrightness(_brightness);
        _cam.setContrast(_contrast);
        _cam.setSaturation(_saturation);
        _cam.setSpecialEffect(_specialEffect);
        _cam.setVFlip(_vflip);
        _cam.setHFlip(_hflip);
        _cam.setWhiteBalance(_wb);
        _cam.setAecMode(true);
        _cam.flashOff();

        // Only reconfigure DMA if frame size actually changed
        if (_frameSize != oldSize) {
            _cam.setXCLKFreq(CameraController::XCLK_20MHZ);
            _cam.setFrameSize(_frameSize);
        }

        _settingsChr->setValue(_buildSettingsString());
        Serial.println("BLE: settings reset to factory defaults");

        // Clear NVS so reboot doesn't reload old saved settings
        _store.begin("cam_server");
        _store.clear();
        _store.end();
        Serial.println("NVS: namespace cleared");

        // Discard stale frame from queue (camera_task will refill)
        if (xSemaphoreTake(_frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            camera_fb_t* staleFb = nullptr;
            xQueuePeek(_frameQueue, &staleFb, 0);
            if (staleFb) {
                camera_fb_t* nullFb = nullptr;
                xQueueOverwrite(_frameQueue, &nullFb);
                _cam.release(staleFb);
            }
            xSemaphoreGive(_frameMutex);
        }
    } else if (cmd == "save") {
        _saveSettings();
        Serial.println("BLE: settings saved to NVS");
    } else {
        Serial.printf("BLE: unknown command \"%s\"\n", cmd.c_str());
    }
}

void CameraBluetoothServer::_onSettingsWrite(const String& input) {
    Serial.printf("BLE: settings write = \"%s\"\n", input.c_str());
    _parseSettingsString(input);

    // Only reconfigure DMA if frame size actually changed
    CameraController::FrameSize oldSize = _cam.getFrameSize();
    if (_frameSize != oldSize) {
        if (_frameSize >= CameraController::XGA) {
            _cam.setXCLKFreq(CameraController::XCLK_10MHZ);
        } else {
            _cam.setXCLKFreq(CameraController::XCLK_20MHZ);
        }
        _cam.setFrameSize(_frameSize);
    }

    // Apply other settings (no DMA teardown)
    _cam.setJpegQuality(_jpegQuality);
    _cam.setBrightness(_brightness);
    _cam.setContrast(_contrast);
    _cam.setSaturation(_saturation);
    _cam.setSpecialEffect(_specialEffect);
    _cam.setVFlip(_vflip);
    _cam.setHFlip(_hflip);
    _cam.setWhiteBalance(_wb);
    _cam.setAecMode(_aecOn);
    if (!_aecOn) {
        _cam.setAecValue(_shutter);
        _cam.setAgcGain(_gain);
    }

    _settingsChr->setValue(_buildSettingsString());
    Serial.printf("BLE: settings updated -> %s\n", _buildSettingsString().c_str());

    // Discard stale frame from queue (camera_task will refill,
    // or _sendFrameChunked will capture fresh if needed)
    if (xSemaphoreTake(_frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        camera_fb_t* staleFb = nullptr;
        xQueuePeek(_frameQueue, &staleFb, 0);
        if (staleFb) {
            camera_fb_t* nullFb = nullptr;
            xQueueOverwrite(_frameQueue, &nullFb);
            _cam.release(staleFb);
        }
        xSemaphoreGive(_frameMutex);
    }
}

void CameraBluetoothServer::_onSettingsRead() {
    _settingsChr->setValue(_buildSettingsString());
}

// ── Frame delivery ─────────────────────────────────────────────────────

void CameraBluetoothServer::_sendFrameChunked() {
    camera_fb_t* fb = nullptr;
    bool ownBuffer = false;  // true if we captured fresh (must release)

    // Try to get a frame from the queue (mutex protects access)
    if (xSemaphoreTake(_frameMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        xQueuePeek(_frameQueue, &fb, 0);
        if (!fb) {
            xSemaphoreGive(_frameMutex);
        }
    }

    // If queue was empty, capture a fresh frame with current settings
    if (!fb) {
        fb = _cam.capture();
        ownBuffer = true;
        if (!fb) {
            Serial.println("BLE: no frame available");
            _infoChr->setValue("{\"size\":0,\"w\":0,\"h\":0,\"q\":0}");
            return;
        }
    }

    // Copy frame data (safe: we either hold mutex or own the buffer)
    uint32_t totalSize = fb->len;
    uint8_t* copyBuf = (uint8_t*)malloc(totalSize);
    if (!copyBuf) {
        if (!ownBuffer) xSemaphoreGive(_frameMutex);
        if (ownBuffer)  _cam.release(fb);
        Serial.println("BLE: malloc failed for frame copy");
        return;
    }
    memcpy(copyBuf, fb->buf, totalSize);

    if (!ownBuffer) xSemaphoreGive(_frameMutex);  // release queue lock
    if (ownBuffer)  _cam.release(fb);             // release fresh capture

    // Update info characteristic
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf),
        "{\"size\":%lu,\"w\":320,\"h\":240,\"q\":%d}",
        totalSize, _jpegQuality);
    _infoChr->setValue(infoBuf);

    // ── Chunked delivery from safe copy ────────────────────────────
    uint8_t chunk[CHUNK_SIZE + 4];
    uint32_t header = totalSize;
    memcpy(chunk, &header, 4);

    uint32_t offset = 0;
    uint32_t dataLen = (totalSize < CHUNK_SIZE) ? totalSize : CHUNK_SIZE;
    memcpy(chunk + 4, copyBuf + offset, dataLen);

    _frameChr->setValue(chunk, dataLen + 4);
    _frameChr->notify();
    offset += dataLen;

    vTaskDelay(pdMS_TO_TICKS(8));

    while (offset < totalSize) {
        dataLen = totalSize - offset;
        if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;

        _frameChr->setValue(copyBuf + offset, dataLen);
        _frameChr->notify();
        offset += dataLen;

        vTaskDelay(pdMS_TO_TICKS(8));
    }

    free(copyBuf);
    Serial.printf("BLE: sent %lu bytes in chunks\n", totalSize);
}

// ── Settings serialisation ─────────────────────────────────────────────

String CameraBluetoothServer::_buildSettingsString() const {
    String s;
    s += "size=" + _frameSizeToString() + ",";
    s += "quality=" + String(_jpegQuality) + ",";
    s += "brightness=" + String(_brightness) + ",";
    s += "contrast=" + String(_contrast) + ",";
    s += "saturation=" + String(_saturation) + ",";
    s += "effect=" + String(_specialEffect) + ",";
    s += "vflip=" + String(_vflip ? 1 : 0) + ",";
    s += "hflip=" + String(_hflip ? 1 : 0) + ",";
    s += "wb=" + _wbToString() + ",";
    s += "aec=" + String(_aecOn ? "on" : "off") + ",";
    s += "shutter=" + String(_shutter) + ",";
    s += "gain=" + String(_gain) + ",";
    s += "flash_gain=" + String(_flashGain) + ",";
    s += "flash_shutter=" + String(_flashShutter);
    return s;
}

void CameraBluetoothServer::_parseSettingsString(const String& input) {
    int start = 0;
    while (start < (int)input.length()) {
        int eq = input.indexOf('=', start);
        if (eq < 0 || eq <= start) break;

        int comma = input.indexOf(',', eq);
        if (comma < 0) comma = input.length();

        String key = input.substring(start, eq);
        String val = input.substring(eq + 1, comma);
        key.trim();
        val.trim();
        key.toLowerCase();
        val.toLowerCase();

        if (key == "size") {
            if (val == "qqvga")      _frameSize = CameraController::QQVGA;
            else if (val == "qvga")  _frameSize = CameraController::QVGA;
            else if (val == "vga")   _frameSize = CameraController::VGA;
            else if (val == "svga")  _frameSize = CameraController::SVGA;
            else if (val == "xga")   _frameSize = CameraController::XGA;
            else if (val == "hd")    _frameSize = CameraController::HD;
            else if (val == "sxga")  _frameSize = CameraController::SXGA;
            else if (val == "uxga")  _frameSize = CameraController::UXGA;
        } else if (key == "quality") {
            _jpegQuality = (uint8_t)constrain(val.toInt(), 0, 63);
        } else if (key == "brightness") {
            _brightness = (int8_t)constrain(val.toInt(), -2, 2);
        } else if (key == "contrast") {
            _contrast = (int8_t)constrain(val.toInt(), -2, 2);
        } else if (key == "saturation") {
            _saturation = (int8_t)constrain(val.toInt(), -2, 2);
        } else if (key == "effect") {
            _specialEffect = constrain(val.toInt(), 0, 6);
        } else if (key == "vflip") {
            _vflip = (val == "1" || val == "true");
        } else if (key == "hflip") {
            _hflip = (val == "1" || val == "true");
        } else if (key == "wb") {
            if (val == "sunny")       _wb = CameraController::WB_SUNNY;
            else if (val == "cloudy") _wb = CameraController::WB_CLOUDY;
            else if (val == "office") _wb = CameraController::WB_OFFICE;
            else if (val == "home")   _wb = CameraController::WB_HOME;
            else                      _wb = CameraController::WB_AUTO;
        } else if (key == "aec") {
            _aecOn = (val == "on" || val == "1" || val == "true" || val == "auto");
        } else if (key == "shutter") {
            _shutter = (uint16_t)constrain(val.toInt(), 0, 1200);
        } else if (key == "gain") {
            _gain = (uint8_t)constrain(val.toInt(), 0, 30);
        } else if (key == "flash_gain") {
            _flashGain = (uint8_t)constrain(val.toInt(), 5, 30);
        } else if (key == "flash_shutter") {
            _flashShutter = (uint16_t)constrain(val.toInt(), 200, 1200);
        }

        start = comma + 1;
    }
}

String CameraBluetoothServer::_frameSizeToString() const {
    switch (_frameSize) {
        case CameraController::QQVGA: return "QQVGA";
        case CameraController::QVGA:  return "QVGA";
        case CameraController::VGA:   return "VGA";
        case CameraController::SVGA:  return "SVGA";
        case CameraController::XGA:   return "XGA";
        case CameraController::HD:    return "HD";
        case CameraController::SXGA:  return "SXGA";
        case CameraController::UXGA:  return "UXGA";
        default:                      return "QVGA";
    }
}

String CameraBluetoothServer::_wbToString() const {
    switch (_wb) {
        case CameraController::WB_SUNNY:  return "sunny";
        case CameraController::WB_CLOUDY: return "cloudy";
        case CameraController::WB_OFFICE: return "office";
        case CameraController::WB_HOME:   return "home";
        default:                          return "auto";
    }
}

// ── Params — driver-owned schema of all valid settings (compact, <512B) ─

String CameraBluetoothServer::_buildParamsJson() const {
    return String(
        "size:e:QQVGA,QVGA,VGA,SVGA,XGA,HD,SXGA,UXGA:QVGA\n"
        "quality:r:0..63:20\n"
        "brightness:r:-2..2:0\n"
        "contrast:r:-2..2:0\n"
        "saturation:r:-2..2:0\n"
        "effect:e:0=off,1=negative,2=b/w,3=red,4=green,5=blue,6=sepia:0\n"
        "vflip:b:0,1:1\n"
        "hflip:b:0,1:0\n"
        "wb:e:auto,sunny,cloudy,office,home:auto\n"
        "aec:e:on,off:on\n"
        "shutter:r:0..1200:0\n"
        "gain:r:0..30:0\n"
        "flash_gain:r:5..30:20\n"
        "flash_shutter:r:200..1200:800"
    );
}

// ── FreeRTOS task entries ──────────────────────────────────────────────

void CameraBluetoothServer::_cameraTaskEntry(void* pv) {
    static_cast<CameraBluetoothServer*>(pv)->_cameraTask();
}

void CameraBluetoothServer::_bleTaskEntry(void* pv) {
    static_cast<CameraBluetoothServer*>(pv)->_bleTask();
}

// ── camera_task — producer ─────────────────────────────────────────────

void CameraBluetoothServer::_cameraTask() {
    for (;;) {
        camera_fb_t* fb = _cam.capture();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        camera_fb_t* old = nullptr;
        xSemaphoreTake(_frameMutex, portMAX_DELAY);
        xQueuePeek(_frameQueue, &old, 0);
        xQueueOverwrite(_frameQueue, &fb);
        xSemaphoreGive(_frameMutex);

        if (old) {
            _cam.release(old);
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // ~20 fps
    }
}

// ── ble_task — consumer / event loop ───────────────────────────────────

void CameraBluetoothServer::_bleTask() {
    for (;;) {
        delay(10);

        if (_pendingSnapshot) {
            _pendingSnapshot = false;
            _sendFrameChunked();
        }
    }
}
