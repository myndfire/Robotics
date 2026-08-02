#pragma once

#include <Arduino.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include <CameraController.h>
#include <StorageController.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

/*
 * CameraBleModule — BLE camera service (CA00).
 *
 * FreeRTOS producer-consumer architecture:
 *   - _cameraTask  (Core 0): continuous capture → _frameQueue (depth-1)
 *   - _deliveryTask(Core 1): waits on _snapshotQueue → peeks frame → chunks via CA03
 *
 * BLE Service 0000CA00:
 *   CA01 Write  — snapshot, flash_on, flash_off
 *   CA02 Write+Read — settings string (quality, brightness, vflip, ...)
 *   CA03 Notify — chunked JPEG (240-byte chunks, 4-byte LE size header)
 *   CA04 Read   — frame info JSON
 *   CA05 Read   — settings params schema
 *
 * Settings are persisted to NVS namespace "cam".
 */

class CameraBleModule {
public:
    CameraBleModule();

    /* Camera init, task spawning. Does NOT touch BLE. */
    void begin();

    /* Register CA00 service + characteristics on shared server. */
    void attach(BLEServer* server);

private:
    // ── BLE UUIDs ─────────────────────────────────────────────────
    static const char* CAMERA_SERVICE_UUID;
    static const char* CONTROL_CHAR_UUID;
    static const char* SETTINGS_CHAR_UUID;
    static const char* FRAME_CHAR_UUID;
    static const char* INFO_CHAR_UUID;
    static const char* PARAMS_CHAR_UUID;

    // ── Task parameters ─────────────────────────────────────────
    static const uint16_t CAM_TASK_STACK  = 4096;
    static const uint16_t BLE_TASK_STACK  = 8192;
    static const uint8_t  CAM_TASK_PRIO   = 1;
    static const uint8_t  BLE_TASK_PRIO   = 2;
    static const uint16_t CHUNK_SIZE      = 240;

    // ── Helpers ───────────────────────────────────────────────────
    void _setupCamera();
    void _loadSettings();
    void _saveSettings();

    void _onControlWrite();
    void _onSettingsWrite(const String& input);
    void _onSettingsRead();

    void _sendFrameChunked();

    String _buildSettingsString() const;
    void   _parseSettingsString(const String& s);
    String _frameSizeToString() const;
    String _wbToString() const;
    String _buildParamsJson() const;

    // ── FreeRTOS task entries ───────────────────────────────────
    static void _cameraTaskEntry(void* pv);
    static void _bleTaskEntry(void* pv);
    void _cameraTask();
    void _bleTask();

    // ── Members ─────────────────────────────────────────────────
    CameraController     _cam{CameraController::NULLLAB_ESP32S3_CAM};
    StorageController    _store;

    QueueHandle_t        _snapshotQueue{nullptr};
    QueueHandle_t        _frameQueue{nullptr};
    SemaphoreHandle_t    _frameMutex{nullptr};
    TaskHandle_t         _camTaskHandle{nullptr};
    TaskHandle_t         _bleTaskHandle{nullptr};

    BLEService*          _cameraService{nullptr};
    BLECharacteristic*   _controlChr{nullptr};
    BLECharacteristic*   _settingsChr{nullptr};
    BLECharacteristic*   _frameChr{nullptr};
    BLECharacteristic*   _infoChr{nullptr};
    BLECharacteristic*   _paramsChr{nullptr};

    volatile bool        _pendingSnapshot{false};

    // ── Settings state ──────────────────────────────────────────
    CameraController::FrameSize    _frameSize{CameraController::QVGA};
    uint8_t                        _jpegQuality{20};
    int8_t                         _brightness{0};
    int8_t                         _contrast{0};
    int8_t                         _saturation{0};
    int                            _specialEffect{0};
    bool                           _vflip{true};
    bool                           _hflip{false};
    CameraController::WhiteBalance _wb{CameraController::WB_AUTO};
    bool                           _flashOn{false};
    bool                           _aecOn{true};
    uint16_t                       _shutter{0};
    uint8_t                        _gain{0};
    uint8_t                        _flashGain{20};
    uint16_t                       _flashShutter{800};
    bool                           _savedAecOn{true};
    uint16_t                       _savedShutter{0};
    uint8_t                        _savedGain{0};
};
