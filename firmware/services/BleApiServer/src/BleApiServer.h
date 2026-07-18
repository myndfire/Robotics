/*
 * BleApiServer.h — BLE GPIO / LED API server
 *
 * Generic command interface over Bluetooth Low Energy.
 * Two-characteristic design: all commands via BA01 (Write),
 * full state via BA02 (Read).
 *
 * State persisted to NVS (namespace "bluetooth_api"):
 *   — LED colour (led_r, led_g, led_b)           always
 *   — LED GPIO (led_pin)                         always
 *   — LED type (led_type)                        always
 *   — Pin modes + values (pins string)           only when :persist suffix used
 *
 * BLE Service (0000BA00-0000-1000-8000-00805F9B34FB):
 *
 *   BA01  Control   Write    key=val commands
 *   BA02  Status    Read     "led=<r>;<g>;<b>,pin<n>=<val>,..."
 *
 * ==================== Usage ====================
 *
 *   #include <BleApiServer.h>
 *
 *   // DevKitC-1 (defaults)
 *   BleApiServer server;
 *
 *   // DORHEA / Supermini
 *   BleApiServer server(21, NEO_RGB + NEO_KHZ800);
 *
 *   void setup() { server.begin(); }
 *   void loop()  { delay(100); }
 *
 * ==================== Hardware ====================
 *
 *   ESP32-S3.  NeoPixel pin and colour order via constructor.
 *   GPIO pin modes (OUTPUT / INPUT / ANALOG) configured at
 *   runtime via BA01.
 */

#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <LedRGBController.h>
#include <StorageController.h>

class BleApiServer {
public:
    BleApiServer(uint8_t ledPin = 48,
                       neoPixelType ledType = NEO_GRB + NEO_KHZ800);
    void begin();

private:
    static const uint8_t MAX_PINS = 16;

    struct GpioPin {
        uint8_t  number;
        uint8_t  mode;          // 1=OUTPUT, 2=INPUT, 3=ANALOG
        uint16_t value;
        bool     persistConfig; // restore mode from NVS after reboot?
        bool     persistValue;  // restore value from NVS after reboot?
    };

    // ── Private helpers ────────────────────────────────────────────

    void _setupBLE();
    GpioPin* _findPin(uint8_t number);
    GpioPin* _getOrCreatePin(uint8_t number, uint8_t mode);
    void _loadState();
    void _savePins();
    void _saveLedPin();
    void _saveLedType();
    void _saveLedColor();

    // ── BLE callbacks ──────────────────────────────────────────────

    void _onControlWrite();
    void _onStatusRead();

    // ── Command handlers ───────────────────────────────────────────

    void _handleCommand(const String& key, const String& val);
    void _handlePinConfig(const String& val);
    void _handleSet(const String& val);
    void _handleGet(const String& val);
    void _handleLed(const String& val);
    void _handleLedPin(const String& val);
    void _handleLedType(const String& val);
    String _buildStatus();

    // ── LED state ──────────────────────────────────────────────────

    uint8_t      _r{0};
    uint8_t      _g{0};
    uint8_t      _b{0};
    uint8_t      _ledPin;
    neoPixelType _ledType;

    // ── GPIO tracking ──────────────────────────────────────────────

    GpioPin  _pins[MAX_PINS];
    uint8_t  _pinCount{0};

    // ── Members ────────────────────────────────────────────────────

    StorageController     _storage;
    LedRGBController*     _led{nullptr};
    BLEServer*            _bleServer{nullptr};
    BLEService*           _apiService{nullptr};
    BLECharacteristic*    _controlChr{nullptr};
    BLECharacteristic*    _statusChr{nullptr};
};
