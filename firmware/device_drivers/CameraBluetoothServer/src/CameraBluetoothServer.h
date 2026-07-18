/*
 * CameraBluetoothServer.h — BLE camera server
 *
 * Composes CameraController with the Arduino-ESP32 BLE stack
 * (BLEDevice / BLEServer / BLECharacteristic) to serve JPEG
 * snapshots over Bluetooth Low Energy.
 *
 * FreeRTOS producer-consumer architecture:
 *
 *   camera_task (Core 0):  captures → xQueueOverwrite (depth=1)
 *   ble_task    (Core 1):  runs BLE event loop, callbacks fire here
 *
 * BLE Service (0000CA00-0000-1000-8000-00805F9B34FB):
 *
 *   CA01  Control   Write    "snapshot", "flash_on", "flash_off", "save", "reset"
 *   CA02  Settings  Write+Read  "key=val,key=val,..." partial updates
 *   CA03  Frame     Notify   Chunked JPEG (240-byte chunks)
 *   CA04  Info      Read     Frame info ({"size":N,"w":W,"h":H,"q":Q})
 *   CA05  Params    Read     JSON schema of valid settings keys/ranges
 *
 * Frame chunking protocol:
 *   Notification 1: [4 bytes total_size LE] + first ≤240 bytes of JPEG
 *   Notifications 2+: ≤240 bytes each continuation data
 *   Client accumulates until total_size bytes received.
 *
 * ==================== Usage ====================
 *
 *   #include <CameraBluetoothServer.h>
 *   CameraBluetoothServer server;
 *   void setup() { server.begin(); }
 *   void loop()  { vTaskDelay(pdMS_TO_TICKS(1000)); }
 *
 * ==================== Hardware ====================
 *
 *   nulllaborg ESP32-S3 CAM (OV2640 / OV3660).
 *   USB-C for power + programming + serial monitor.
 *   Board preset: NULLLAB_ESP32S3_CAM.
 */

#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <CameraController.h>
#include <StorageController.h>

class CameraBluetoothServer {
public:
    CameraBluetoothServer();
    void begin();

private:
    // ── Task parameters ────────────────────────────────────────────

    static const uint16_t CAM_TASK_STACK  = 4096;
    static const uint16_t BLE_TASK_STACK  = 8192;
    static const uint8_t  CAM_TASK_PRIO   = 1;
    static const uint8_t  BLE_TASK_PRIO   = 2;
    static const uint16_t CHUNK_SIZE      = 240;

    // ── Private helpers ────────────────────────────────────────────

    void _setupCamera();
    void _setupBLE();
    void _loadSettings();
    void _saveSettings();

    // ── BLE callbacks ──────────────────────────────────────────────

    void _onControlWrite();
    void _onSettingsWrite(const String& input);
    void _onSettingsRead();

    // ── Frame delivery ─────────────────────────────────────────────

    void _sendFrameChunked();

    // ── Settings serialisation ─────────────────────────────────────

    String _buildSettingsString() const;
    void _parseSettingsString(const String& s);
    String _frameSizeToString() const;
    String _wbToString() const;
    String _buildParamsJson() const;

    // ── FreeRTOS task entries ──────────────────────────────────────

    static void _cameraTaskEntry(void* pv);
    static void _bleTaskEntry(void* pv);
    void _cameraTask();
    void _bleTask();

    // ── Deferred snapshot flag (BLE callback cannot do chunked send) ─

    volatile bool        _pendingSnapshot{false};

    // ── Members ────────────────────────────────────────────────────

    CameraController     _cam{CameraController::NULLLAB_ESP32S3_CAM};
    StorageController    _store;
    QueueHandle_t        _frameQueue{nullptr};
    SemaphoreHandle_t    _frameMutex{nullptr};
    TaskHandle_t         _camTaskHandle{nullptr};
    TaskHandle_t         _bleTaskHandle{nullptr};

    BLEServer*           _bleServer{nullptr};
    BLEService*          _cameraService{nullptr};
    BLECharacteristic*   _controlChr{nullptr};
    BLECharacteristic*   _settingsChr{nullptr};
    BLECharacteristic*   _frameChr{nullptr};
    BLECharacteristic*   _infoChr{nullptr};
    BLECharacteristic*   _paramsChr{nullptr};

    // ── Current settings state ─────────────────────────────────────

    CameraController::FrameSize      _frameSize{CameraController::QVGA};
    uint8_t                          _jpegQuality{20};
    int8_t                           _brightness{0};
    int8_t                           _contrast{0};
    int8_t                           _saturation{0};
    int                              _specialEffect{0};
    bool                             _vflip{true};
    bool                             _hflip{false};
    CameraController::WhiteBalance   _wb{CameraController::WB_AUTO};
    bool                             _flashOn{false};
    bool                             _aecOn{true};
    uint16_t                         _shutter{0};
    uint8_t                          _gain{0};

    // Flash exposure — user-configurable via BLE CA02 (persisted to NVS)
    uint8_t                          _flashGain{20};
    uint16_t                         _flashShutter{800};

    // Saved state for flash exposure lock/unlock
    bool                             _savedAecOn{true};
    uint16_t                         _savedShutter{0};
    uint8_t                          _savedGain{0};

    // Static queue reference for camera_task access
    static QueueHandle_t          s_frameQueue;
};
