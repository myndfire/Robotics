#pragma once

#include <Arduino.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <StorageController.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifdef HAS_NEOPIXEL
#include <LedRGBController.h>
#endif

/*
 * GpioBleModule — BLE GPIO / LED control service (BA00).
 *
 * BLE Service 0000BA00:
 *   BA01 Write  — key=val command pairs (pin=, set=, get=, led=)
 *   BA02 Read   — full state string
 *
 * Supported pins on ESP32-S3-CAM: 1, 14, 40, 41, 42
 *   - 40/41/42 are strapping pins; avoid external pull-down at boot.
 *
 * NeoPixel is compile-time optional via HAS_NEOPIXEL.
 *
 * Command queue decouples BLE callback from real work.
 * get= is handled synchronously in the callback (fast, non-blocking).
 */

class GpioBleModule {
public:
    GpioBleModule();

    /* GPIO / LED / NVS init. Does NOT touch BLE. */
    void begin();

    /* Register BA00 service + characteristics on shared server. */
    void attach(BLEServer* server);

private:
    // ── BLE UUIDs ─────────────────────────────────────────────────
    static const char* API_SERVICE_UUID;
    static const char* CONTROL_CHAR_UUID;
    static const char* STATUS_CHAR_UUID;

    // ── Constants ─────────────────────────────────────────────────
    static const uint8_t MAX_PINS = 5;
    static const uint16_t QUEUE_DEPTH = 8;
    static const uint16_t TASK_STACK = 4096;
    static const uint8_t  TASK_PRIO  = 2;

    struct GpioPin {
        uint8_t  number;
        uint8_t  mode;      // 0=unconfigured, 1=OUTPUT, 2=INPUT, 3=ANALOG
        uint16_t value;
    };

    struct Command {
        char key[8];
        char val[32];
    };

    // ── Helpers ─────────────────────────────────────────────────
    void _loadState();
    void _saveState();
    GpioPin* _findPin(uint8_t number);
    GpioPin* _getOrCreatePin(uint8_t number, uint8_t mode);
    String _buildStatus();

    void _handleCommand(const char* key, const char* val);
    void _handlePinConfig(const char* val);
    void _handleSet(const char* val);
    void _handleGet(const char* val);
    void _handleLed(const char* val);

    // ── Task ──────────────────────────────────────────────────────
    static void _taskEntry(void* pv);
    void _runTask();

    // ── BLE callbacks ───────────────────────────────────────────
    void _onControlWrite();
    void _onStatusRead();

    // ── Members ───────────────────────────────────────────────────
    StorageController    _store;
    QueueHandle_t        _cmdQueue{nullptr};
    TaskHandle_t         _taskHandle{nullptr};

    BLEService*          _apiService{nullptr};
    BLECharacteristic*   _controlChr{nullptr};
    BLECharacteristic*   _statusChr{nullptr};

    GpioPin              _pins[MAX_PINS];
    uint8_t              _pinCount{0};

    // LED state
    uint8_t              _ledR{0};
    uint8_t              _ledG{0};
    uint8_t              _ledB{0};

#ifdef HAS_NEOPIXEL
    LedRGBController*    _led{nullptr};
    uint8_t              _ledPin{48};
    neoPixelType         _ledType{NEO_GRB + NEO_KHZ800};
#endif
};
