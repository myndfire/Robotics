#include "ConfigStore.h"

ConfigStore::ConfigStore(uint8_t sda, uint8_t scl, uint8_t oledAddr, uint8_t oledHeight)
    : _display(sda, scl, oledAddr, oledHeight)
    , _deviceName(DEFAULT_NAME)
    , _interval(DEFAULT_INTERVAL)
    , _enabled(DEFAULT_ENABLED)
    , _gain(DEFAULT_GAIN)
{}

void ConfigStore::begin(const char* nvsNamespace) {
    _display.begin();
    _display.setFont(0, u8g2_font_5x8_tf);

    _store.begin(nvsNamespace);
    _loadConfig();

    _updateDisplay();
    _display.processNext(0);

    Serial.println("ConfigStore ready");
    _showConfig();
    _printMenu();
}

String ConfigStore::getDeviceName() const { return _deviceName; }
int    ConfigStore::getInterval() const   { return _interval; }
bool   ConfigStore::isEnabled() const     { return _enabled; }
float  ConfigStore::getGain() const       { return _gain; }

bool ConfigStore::setDeviceName(const String& name) {
    _deviceName = name;
    _store.put<String>("dev_name", _deviceName);
    Serial.printf("Dev name set: %s\n", _deviceName.c_str());
    return true;
}

bool ConfigStore::setInterval(int seconds) {
    if (seconds <= 0) return false;
    _interval = seconds;
    _store.put<int>("interval", _interval);
    Serial.printf("Interval: %ds\n", _interval);
    return true;
}

bool ConfigStore::setEnabled(bool en) {
    _enabled = en;
    _store.put<bool>("enabled", _enabled);
    Serial.printf("Enabled: %s\n", _enabled ? "true" : "false");
    return true;
}

bool ConfigStore::setGain(float value) {
    _gain = value;
    _store.put<float>("gain", _gain);
    Serial.printf("Gain: %.2f\n", _gain);
    return true;
}

void ConfigStore::resetToDefaults() {
    _store.clear();
    _deviceName = DEFAULT_NAME;
    _interval   = DEFAULT_INTERVAL;
    _enabled    = DEFAULT_ENABLED;
    _gain       = DEFAULT_GAIN;
    Serial.println("Config reset to defaults");
}

void ConfigStore::handleSerial() {
    _updateDisplay();
    _display.processNext(0);

    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("name ")) {
        setDeviceName(line.substring(5));
    }
    else if (line.startsWith("intv ")) {
        setInterval(line.substring(5).toInt());
    }
    else if (line.startsWith("enab ")) {
        setEnabled(line.substring(5).toInt() != 0);
    }
    else if (line.startsWith("gain ")) {
        setGain(line.substring(5).toFloat());
    }
    else if (line == "show") {
        _showConfig();
    }
    else if (line == "list") {
        _listKeys();
    }
    else if (line == "clear") {
        resetToDefaults();
    }

    _updateDisplay();
    _printMenu();
}

void ConfigStore::_loadConfig() {
    _deviceName = _store.get<String>("dev_name", DEFAULT_NAME);
    _interval   = _store.get<int>("interval", DEFAULT_INTERVAL);
    _enabled    = _store.get<bool>("enabled", DEFAULT_ENABLED);
    _gain       = _store.get<float>("gain", DEFAULT_GAIN);
}

void ConfigStore::_updateDisplay() {
    _display.setLine(0, "Name: %s", _deviceName.c_str());
    _display.setLine(1, "Intv:%ds  Gain:%.1f", _interval, _gain);
    _display.setLine(2, "Enabled: %s", _enabled ? "yes" : "no");
    _display.setLine(3, "Free: %d", _store.freeEntries());
}

void ConfigStore::_showConfig() {
    Serial.println("── Current Config ──");
    Serial.printf("  dev_name:  %s\n", _deviceName.c_str());
    Serial.printf("  interval:  %d\n", _interval);
    Serial.printf("  enabled:   %s\n", _enabled ? "true" : "false");
    Serial.printf("  gain:      %.2f\n", _gain);
    Serial.printf("  Free NVS:  %d entries\n", _store.freeEntries());
    Serial.println("──────────────────────");
}

void ConfigStore::_listKeys() {
    const char* keys[] = {"dev_name", "interval", "enabled", "gain"};
    const char* types[] = {"String", "int", "bool", "float"};
    Serial.println("── Stored Keys ──");
    for (uint8_t i = 0; i < 4; i++) {
        Serial.printf("  %-12s  %s  %s\n",
            keys[i], types[i],
            _store.exists(keys[i]) ? "OK" : "MISSING");
    }
    Serial.println("──────────────────");
}

void ConfigStore::_printMenu() {
    Serial.println();
    Serial.println("──── Config Store Commands ────");
    Serial.printf ("  Namespace: %s\n", _store.getNamespace());
    Serial.println("  name str     set device name (string)");
    Serial.println("  intv N       set interval seconds (int)");
    Serial.println("  enab 0|1     enable / disable (bool)");
    Serial.println("  gain N.N     set gain multiplier (float)");
    Serial.println("  show         display all values");
    Serial.println("  list         list stored keys");
    Serial.println("  clear        reset to defaults");
    Serial.println("──────────────────────────────────");
    Serial.println();
}
