#pragma once

#include <Arduino.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <StorageController.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/*
 * ConfigBleModule — Extensible typed NVS configuration over BLE + Serial (CF00).
 *
 * BLE Service 0000CF00:
 *   CF01 Write  — key=value,key=value,... (partial updates, any key)
 *   CF02 Read   — full state string of all stored keys
 *   CF03 Read   — schema: key:type:default,key2:type2:default2,...
 *
 * Type is auto-detected on first write:
 *   "true"/"false" → bool
 *   all digits     → int
 *   digits + "."   → float
 *   else           → String
 *
 * The type registry is persisted under key "__registry" in NVS.
 * Serial CLI commands are enqueued and processed by the same task.
 */

class ConfigBleModule {
public:
    ConfigBleModule();

    /* Load existing registry from NVS. Does NOT touch BLE. */
    void begin(const char* nvsNamespace);

    /* Register CF00 service + characteristics on shared server. */
    void attach(BLEServer* server);

    /* Call from loop() to process Serial input. */
    void handleSerial();

private:
    // ── BLE UUIDs ─────────────────────────────────────────────────
    static const char* CONFIG_SERVICE_UUID;
    static const char* CONTROL_CHAR_UUID;
    static const char* STATE_CHAR_UUID;
    static const char* SCHEMA_CHAR_UUID;

    // ── Constants ─────────────────────────────────────────────────
    static const uint16_t QUEUE_DEPTH = 8;
    static const uint16_t TASK_STACK  = 4096;
    static const uint8_t  TASK_PRIO   = 2;
    static const uint8_t  MAX_KEYS     = 16;

    struct Command {
        char key[16];
        char val[32];
    };

    enum KeyType { TYPE_NONE, TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING };

    struct KeyEntry {
        char     name[16];
        KeyType  type;
        char     defaultVal[32];
    };

    // ── Type detection ────────────────────────────────────────────
    KeyType _detectType(const String& val);
    String  _typeToString(KeyType t);

    // ── NVS helpers ───────────────────────────────────────────────
    void _loadRegistry();
    void _saveRegistry();
    void _putValue(const char* key, KeyType type, const String& val);
    String _getValue(const char* key, KeyType type);
    void _registerKey(const char* key, KeyType type);
    KeyEntry* _findKey(const char* key);

    // ── State / schema builders ───────────────────────────────────
    String _buildStateString();
    String _buildSchemaString();

    // ── BLE callbacks ───────────────────────────────────────────
    void _onControlWrite();
    void _onStateRead();
    void _onSchemaRead();

    // ── Task ──────────────────────────────────────────────────────
    static void _taskEntry(void* pv);
    void _runTask();

    // ── Serial handlers ───────────────────────────────────────────
    void _enqueueSerialCommand(const String& line);
    void _printMenu();

    // ── Members ───────────────────────────────────────────────────
    StorageController    _store;
    String               _namespace;
    QueueHandle_t        _cmdQueue{nullptr};
    TaskHandle_t         _taskHandle{nullptr};

    BLEService*          _configService{nullptr};
    BLECharacteristic*   _controlChr{nullptr};
    BLECharacteristic*   _stateChr{nullptr};
    BLECharacteristic*   _schemaChr{nullptr};

    KeyEntry             _registry[MAX_KEYS];
    uint8_t              _keyCount{0};
};
