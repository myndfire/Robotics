#pragma once

#include <Arduino.h>
#include <StorageController.h>
#include <DisplayController.h>

class ConfigStore {
public:
    ConfigStore(uint8_t sda, uint8_t scl,
                uint8_t oledAddr = 0x3C, uint8_t oledHeight = 32);

    void begin(const char* nvsNamespace = "config");

    String getDeviceName() const;
    int    getInterval() const;
    bool   isEnabled() const;
    float  getGain() const;

    bool setDeviceName(const String& name);
    bool setInterval(int seconds);
    bool setEnabled(bool en);
    bool setGain(float value);

    void resetToDefaults();
    void handleSerial();

private:
    StorageController  _store;
    DisplayController  _display;

    String _deviceName;
    int    _interval;
    bool   _enabled;
    float  _gain;

    static constexpr const char* DEFAULT_NAME     = "ESP32-S3";
    static constexpr int        DEFAULT_INTERVAL  = 5;
    static constexpr bool       DEFAULT_ENABLED   = true;
    static constexpr float      DEFAULT_GAIN      = 1.0f;

    void _loadConfig();
    void _updateDisplay();
    void _showConfig();
    void _listKeys();
    void _printMenu();
};
