#include "CameraBleModule.h"

// ── UUIDs ────────────────────────────────────────────────────────────────

const char* CameraBleModule::CAMERA_SERVICE_UUID = "0000ca00-0000-1000-8000-00805f9b34fb";
const char* CameraBleModule::CONTROL_CHAR_UUID   = "0000ca01-0000-1000-8000-00805f9b34fb";
const char* CameraBleModule::SETTINGS_CHAR_UUID  = "0000ca02-0000-1000-8000-00805f9b34fb";
const char* CameraBleModule::FRAME_CHAR_UUID       = "0000ca03-0000-1000-8000-00805f9b34fb";
const char* CameraBleModule::INFO_CHAR_UUID        = "0000ca04-0000-1000-8000-00805f9b34fb";
const char* CameraBleModule::PARAMS_CHAR_UUID      = "0000ca05-0000-1000-8000-00805f9b34fb";

// ── Constructor / begin ──────────────────────────────────────────────────

CameraBleModule::CameraBleModule() {}

void CameraBleModule::begin() {
    _setupCamera();
    _loadSettings();

    // Depth-1 queues: only latest matters
    _snapshotQueue = xQueueCreate(1, sizeof(bool));
    _frameQueue    = xQueueCreate(1, sizeof(camera_fb_t*));
    _frameMutex    = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(_cameraTaskEntry, "cam_task", CAM_TASK_STACK,
                            this, CAM_TASK_PRIO, &_camTaskHandle, 0);
    xTaskCreatePinnedToCore(_bleTaskEntry,    "cam_ble",  BLE_TASK_STACK,
                            this, BLE_TASK_PRIO, &_bleTaskHandle, 1);

    Serial.println("CameraBleModule: tasks started");
}

// ── BLE attach ───────────────────────────────────────────────────────────

void CameraBleModule::attach(BLEServer* server) {
    _cameraService = server->createService(CAMERA_SERVICE_UUID);

    // CA01 — Control (write)
    _controlChr = _cameraService->createCharacteristic(
        CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    {
        class CB : public BLECharacteristicCallbacks {
            CameraBleModule* _m;
        public:
            CB(CameraBleModule* m) : _m(m) {}
            void onWrite(BLECharacteristic* c) override { (void)c; _m->_onControlWrite(); }
        };
        _controlChr->setCallbacks(new CB(this));
    }

    // CA02 — Settings (write + read)
    _settingsChr = _cameraService->createCharacteristic(
        SETTINGS_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
    {
        class CB : public BLECharacteristicCallbacks {
            CameraBleModule* _m;
        public:
            CB(CameraBleModule* m) : _m(m) {}
            void onWrite(BLECharacteristic* c) override {
                _m->_onSettingsWrite(String(c->getValue().c_str()));
            }
            void onRead(BLECharacteristic* c) override { (void)c; _m->_onSettingsRead(); }
        };
        _settingsChr->setCallbacks(new CB(this));
    }
    _settingsChr->setValue(std::string(_buildSettingsString().c_str()));

    // CA03 — Frame (notify) + CCCD descriptor
    _frameChr = _cameraService->createCharacteristic(
        FRAME_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    _frameChr->addDescriptor(new BLE2902());

    // CA04 — Info (read)
    _infoChr = _cameraService->createCharacteristic(
        INFO_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _infoChr->setValue("{\"size\":0,\"w\":0,\"h\":0,\"q\":0}");

    // CA05 — Params (read)
    _paramsChr = _cameraService->createCharacteristic(
        PARAMS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _paramsChr->setValue(std::string(_buildParamsJson().c_str()));

    _cameraService->start();
    Serial.println("CameraBleModule: CA00 service attached");
}

// ── Camera setup ─────────────────────────────────────────────────────────

void CameraBleModule::_setupCamera() {
    _cam.setFbCount(4);
    _cam.setFrameSize(_frameSize);
    if (!_cam.begin()) {
        Serial.println("Camera: init FAILED — check ribbon cable");
        return;
    }
    _cam.setJpegQuality(_jpegQuality);
    _cam.setVFlip(_vflip);
    _cam.setHFlip(_hflip);
    _cam.setBrightness(_brightness);
    _cam.setContrast(_contrast);
    _cam.setSaturation(_saturation);
    _cam.setSpecialEffect(_specialEffect);
    _cam.setWhiteBalance(_wb);
    Serial.println("Camera: initialized");
}

// ── NVS settings ─────────────────────────────────────────────────────────

void CameraBleModule::_loadSettings() {
    _store.begin("cam");
    _jpegQuality  = _store.get<int>("quality", _jpegQuality);
    _brightness   = _store.get<int>("brightness", _brightness);
    _contrast     = _store.get<int>("contrast", _contrast);
    _saturation   = _store.get<int>("saturation", _saturation);
    _specialEffect= _store.get<int>("effect", _specialEffect);
    _vflip        = _store.get<bool>("vflip", _vflip);
    _hflip        = _store.get<bool>("hflip", _hflip);
    _aecOn        = _store.get<bool>("aec", _aecOn);
    _shutter      = _store.get<int>("shutter", _shutter);
    _gain         = _store.get<int>("gain", _gain);
    _flashGain    = _store.get<int>("flash_gain", _flashGain);
    _flashShutter = _store.get<int>("flash_shutter", _flashShutter);
    _store.end();
}

void CameraBleModule::_saveSettings() {
    _store.begin("cam");
    _store.put<int>("quality", _jpegQuality);
    _store.put<int>("brightness", _brightness);
    _store.put<int>("contrast", _contrast);
    _store.put<int>("saturation", _saturation);
    _store.put<int>("effect", _specialEffect);
    _store.put<bool>("vflip", _vflip);
    _store.put<bool>("hflip", _hflip);
    _store.put<bool>("aec", _aecOn);
    _store.put<int>("shutter", _shutter);
    _store.put<int>("gain", _gain);
    _store.put<int>("flash_gain", _flashGain);
    _store.put<int>("flash_shutter", _flashShutter);
    _store.end();
}

// ── BLE callbacks ──────────────────────────────────────────────────────

void CameraBleModule::_onControlWrite() {
    String cmd = _controlChr->getValue().c_str();
    cmd.trim();
    Serial.printf("CAM BLE: control \"%s\"\n", cmd.c_str());

    if (cmd == "snapshot") {
        bool flag = true;
        xQueueOverwrite(_snapshotQueue, &flag);
    }
    else if (cmd == "flash_on") {
        _flashOn = true;
        _savedAecOn = _aecOn;
        _savedShutter = _shutter;
        _savedGain = _gain;
        _cam.setAecMode(false);
        _cam.setAecValue(_flashShutter);
        _cam.setAgcGain(_flashGain);
        _cam.flashOn();
    }
    else if (cmd == "flash_off") {
        _flashOn = false;
        _cam.flashOff();
        _cam.setAecMode(_savedAecOn);
        _cam.setAecValue(_savedShutter);
        _cam.setAgcGain(_savedGain);
    }
}

void CameraBleModule::_onSettingsWrite(const String& input) {
    _parseSettingsString(input);
    _saveSettings();
    _settingsChr->setValue(std::string(_buildSettingsString().c_str()));
}

void CameraBleModule::_onSettingsRead() {
    _settingsChr->setValue(std::string(_buildSettingsString().c_str()));
}

// ── Frame delivery ───────────────────────────────────────────────────────

void CameraBleModule::_sendFrameChunked() {
    if (!_frameChr) return;   // guard: not attached yet

    camera_fb_t* fb = nullptr;
    bool ownBuffer = false;

    if (_frameMutex && xSemaphoreTake(_frameMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (_frameQueue) xQueuePeek(_frameQueue, &fb, 0);
        if (!fb) {
            xSemaphoreGive(_frameMutex);
        }
    }

    if (!fb) {
        fb = _cam.capture();
        ownBuffer = true;
        if (!fb) {
            Serial.println("CAM BLE: no frame available");
            if (_infoChr) _infoChr->setValue("{\"size\":0,\"w\":0,\"h\":0,\"q\":0}");
            return;
        }
    }

    uint32_t totalSize = fb->len;
    uint8_t* copyBuf = (uint8_t*)malloc(totalSize);
    if (!copyBuf) {
        if (!ownBuffer) xSemaphoreGive(_frameMutex);
        if (ownBuffer)  _cam.release(fb);
        Serial.println("CAM BLE: malloc failed");
        return;
    }
    memcpy(copyBuf, fb->buf, totalSize);

    if (!ownBuffer && _frameMutex) xSemaphoreGive(_frameMutex);
    if (ownBuffer)  _cam.release(fb);

    // Update info (capture w/h before releasing the frame buffer)
    uint32_t fw = fb->width;
    uint32_t fh = fb->height;
    if (_infoChr) {
        char infoBuf[64];
        snprintf(infoBuf, sizeof(infoBuf),
                 "{\"size\":%lu,\"w\":%lu,\"h\":%lu,\"q\":%d}",
                 totalSize, fw, fh, _jpegQuality);
        _infoChr->setValue(infoBuf);
    }

    // Chunked delivery
    uint8_t chunk[CHUNK_SIZE + 4];
    uint32_t header = totalSize;
    memcpy(chunk, &header, 4);

    uint32_t offset = 0;
    uint32_t dataLen = (totalSize < CHUNK_SIZE) ? totalSize : CHUNK_SIZE;
    memcpy(chunk + 4, copyBuf + offset, dataLen);

    if (_frameChr) {
        _frameChr->setValue(chunk, dataLen + 4);
        _frameChr->notify();
    }
    offset += dataLen;
    vTaskDelay(pdMS_TO_TICKS(8));

    while (offset < totalSize) {
        dataLen = totalSize - offset;
        if (dataLen > CHUNK_SIZE) dataLen = CHUNK_SIZE;
        if (_frameChr) {
            _frameChr->setValue(copyBuf + offset, dataLen);
            _frameChr->notify();
        }
        offset += dataLen;
        vTaskDelay(pdMS_TO_TICKS(8));
    }

    free(copyBuf);
    Serial.printf("CAM BLE: sent %lu bytes\n", totalSize);
}

// ── Settings helpers ─────────────────────────────────────────────────────

String CameraBleModule::_buildSettingsString() const {
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

void CameraBleModule::_parseSettingsString(const String& input) {
    int start = 0;
    while (start < (int)input.length()) {
        int eq = input.indexOf('=', start);
        if (eq < 0 || eq <= start) break;
        int comma = input.indexOf(',', eq);
        if (comma < 0) comma = input.length();
        String key = input.substring(start, eq);
        String val = input.substring(eq + 1, comma);
        key.trim(); val.trim();

        if (key == "size") {
            if      (val == "96x96")    _frameSize = CameraController::SIZE_96X96;
            else if (val == "QQVGA")    _frameSize = CameraController::QQVGA;
            else if (val == "QCIF")     _frameSize = CameraController::QCIF;
            else if (val == "HQVGA")    _frameSize = CameraController::HQVGA;
            else if (val == "240x240")  _frameSize = CameraController::SIZE_240X240;
            else if (val == "QVGA")     _frameSize = CameraController::QVGA;
            else if (val == "CIF")      _frameSize = CameraController::CIF;
            else if (val == "HVGA")     _frameSize = CameraController::HVGA;
            else if (val == "VGA")      _frameSize = CameraController::VGA;
            else if (val == "SVGA")     _frameSize = CameraController::SVGA;
            else if (val == "XGA")      _frameSize = CameraController::XGA;
            else if (val == "HD")       _frameSize = CameraController::HD;
            else if (val == "SXGA")     _frameSize = CameraController::SXGA;
            else if (val == "UXGA")     _frameSize = CameraController::UXGA;
            else if (val == "FHD")      _frameSize = CameraController::FHD;
            else if (val == "QXGA")     _frameSize = CameraController::QXGA;
        }
        else if (key == "quality")   _jpegQuality = val.toInt();
        else if (key == "brightness") _brightness = val.toInt();
        else if (key == "contrast")   _contrast = val.toInt();
        else if (key == "saturation") _saturation = val.toInt();
        else if (key == "effect")     _specialEffect = val.toInt();
        else if (key == "vflip")      _vflip = (val.toInt() != 0);
        else if (key == "hflip")      _hflip = (val.toInt() != 0);
        else if (key == "wb") {
            if (val == "auto") _wb = CameraController::WB_AUTO;
            else if (val == "sunny") _wb = CameraController::WB_SUNNY;
            else if (val == "cloudy") _wb = CameraController::WB_CLOUDY;
            else if (val == "office") _wb = CameraController::WB_OFFICE;
            else if (val == "home") _wb = CameraController::WB_HOME;
        }
        else if (key == "aec")        _aecOn = (val == "on");
        else if (key == "shutter")    _shutter = val.toInt();
        else if (key == "gain")       _gain = val.toInt();
        else if (key == "flash_gain") _flashGain = val.toInt();
        else if (key == "flash_shutter") _flashShutter = val.toInt();

        start = comma + 1;
    }

    // Apply to sensor
    _cam.setJpegQuality(_jpegQuality);
    _cam.setBrightness(_brightness);
    _cam.setContrast(_contrast);
    _cam.setSaturation(_saturation);
    _cam.setSpecialEffect(_specialEffect);
    _cam.setVFlip(_vflip);
    _cam.setHFlip(_hflip);
    _cam.setWhiteBalance(_wb);
    _cam.setAecMode(_aecOn);
    _cam.setAecValue(_shutter);
    _cam.setAgcGain(_gain);
}

String CameraBleModule::_frameSizeToString() const {
    switch (_frameSize) {
        case CameraController::SIZE_96X96: return "96x96";
        case CameraController::QQVGA: return "QQVGA";
        case CameraController::QCIF: return "QCIF";
        case CameraController::HQVGA: return "HQVGA";
        case CameraController::SIZE_240X240: return "240x240";
        case CameraController::QVGA: return "QVGA";
        case CameraController::CIF: return "CIF";
        case CameraController::HVGA: return "HVGA";
        case CameraController::VGA: return "VGA";
        case CameraController::SVGA: return "SVGA";
        case CameraController::XGA: return "XGA";
        case CameraController::HD: return "HD";
        case CameraController::SXGA: return "SXGA";
        case CameraController::UXGA: return "UXGA";
        case CameraController::FHD: return "FHD";
        case CameraController::QXGA: return "QXGA";
        default: return "QVGA";
    }
}

String CameraBleModule::_wbToString() const {
    switch (_wb) {
        case CameraController::WB_SUNNY:  return "sunny";
        case CameraController::WB_CLOUDY: return "cloudy";
        case CameraController::WB_OFFICE: return "office";
        case CameraController::WB_HOME:   return "home";
        default: return "auto";
    }
}

String CameraBleModule::_buildParamsJson() const {
    return "{\"quality\":{\"min\":0,\"max\":63,\"def\":20},"
           "\"brightness\":{\"min\":-2,\"max\":2,\"def\":0},"
           "\"contrast\":{\"min\":-2,\"max\":2,\"def\":0},"
           "\"saturation\":{\"min\":-2,\"max\":2,\"def\":0},"
           "\"effect\":{\"min\":0,\"max\":6,\"def\":0},"
           "\"vflip\":{\"type\":\"bool\",\"def\":true},"
           "\"hflip\":{\"type\":\"bool\",\"def\":false},"
           "\"wb\":{\"type\":\"enum\",\"values\":[\"auto\",\"sunny\",\"cloudy\",\"office\",\"home\"]},"
           "\"aec\":{\"type\":\"bool\",\"def\":true},"
           "\"shutter\":{\"min\":0,\"max\":1200,\"def\":0},"
           "\"gain\":{\"min\":0,\"max\":30,\"def\":0}}";
}

// ── FreeRTOS tasks ───────────────────────────────────────────────────────

void CameraBleModule::_cameraTaskEntry(void* pv) {
    static_cast<CameraBleModule*>(pv)->_cameraTask();
}

void CameraBleModule::_bleTaskEntry(void* pv) {
    static_cast<CameraBleModule*>(pv)->_bleTask();
}

void CameraBleModule::_cameraTask() {
    camera_fb_t* oldFb = nullptr;
    for (;;) {
        camera_fb_t* fb = _cam.capture();
        if (fb && _frameQueue && _frameMutex) {
            xSemaphoreTake(_frameMutex, portMAX_DELAY);
            xQueueOverwrite(_frameQueue, &fb);
            xSemaphoreGive(_frameMutex);
            if (oldFb) _cam.release(oldFb);
            oldFb = fb;
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // ~20 FPS
    }
}

void CameraBleModule::_bleTask() {
    for (;;) {
        bool req = false;
        if (_snapshotQueue && xQueueReceive(_snapshotQueue, &req, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (req) _sendFrameChunked();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
