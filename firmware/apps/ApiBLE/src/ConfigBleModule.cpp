#include "ConfigBleModule.h"

// ── UUIDs ────────────────────────────────────────────────────────────────

const char* ConfigBleModule::CONFIG_SERVICE_UUID = "0000cf00-0000-1000-8000-00805f9b34fb";
const char* ConfigBleModule::CONTROL_CHAR_UUID   = "0000cf01-0000-1000-8000-00805f9b34fb";
const char* ConfigBleModule::STATE_CHAR_UUID     = "0000cf02-0000-1000-8000-00805f9b34fb";
const char* ConfigBleModule::SCHEMA_CHAR_UUID    = "0000cf03-0000-1000-8000-00805f9b34fb";

// ── Constructor / begin ──────────────────────────────────────────────────

ConfigBleModule::ConfigBleModule() {}

void ConfigBleModule::begin(const char* nvsNamespace) {
    _namespace = nvsNamespace;
    _store.begin(nvsNamespace);
    _loadRegistry();
    _store.end();

    _cmdQueue = xQueueCreate(QUEUE_DEPTH, sizeof(Command));
    xTaskCreatePinnedToCore(_taskEntry, "cfg_task", TASK_STACK,
                            this, TASK_PRIO, &_taskHandle, 1);

    Serial.println("ConfigBleModule: started");
    _printMenu();
}

// ── BLE attach ───────────────────────────────────────────────────────────

void ConfigBleModule::attach(BLEServer* server) {
    _configService = server->createService(CONFIG_SERVICE_UUID);

    // CF01 — Control (write)
    _controlChr = _configService->createCharacteristic(
        CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    {
        class CB : public BLECharacteristicCallbacks {
            ConfigBleModule* _m;
        public:
            CB(ConfigBleModule* m) : _m(m) {}
            void onWrite(BLECharacteristic* c) override { (void)c; _m->_onControlWrite(); }
        };
        _controlChr->setCallbacks(new CB(this));
    }

    // CF02 — State (read)
    _stateChr = _configService->createCharacteristic(
        STATE_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    {
        class CB : public BLECharacteristicCallbacks {
            ConfigBleModule* _m;
        public:
            CB(ConfigBleModule* m) : _m(m) {}
            void onRead(BLECharacteristic* c) override { (void)c; _m->_onStateRead(); }
        };
        _stateChr->setCallbacks(new CB(this));
    }
    _stateChr->setValue(_buildStateString().c_str());

    // CF03 — Schema (read)
    _schemaChr = _configService->createCharacteristic(
        SCHEMA_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    {
        class CB : public BLECharacteristicCallbacks {
            ConfigBleModule* _m;
        public:
            CB(ConfigBleModule* m) : _m(m) {}
            void onRead(BLECharacteristic* c) override { (void)c; _m->_onSchemaRead(); }
        };
        _schemaChr->setCallbacks(new CB(this));
    }
    _schemaChr->setValue(_buildSchemaString().c_str());

    _configService->start();
    Serial.println("ConfigBleModule: CF00 service attached");
}

// ── Type detection ───────────────────────────────────────────────────────

ConfigBleModule::KeyType ConfigBleModule::_detectType(const String& val) {
    String v = val;
    v.trim();
    v.toLowerCase();
    if (v == "true" || v == "false") return TYPE_BOOL;

    bool hasDot = false;
    bool allNum = true;
    int start = 0;
    if (v.length() > 0 && v[0] == '-') start = 1;
    for (int i = start; i < (int)v.length(); i++) {
        if (v[i] == '.') {
            if (hasDot) { allNum = false; break; }
            hasDot = true;
        } else if (!isdigit(v[i])) {
            allNum = false; break;
        }
    }
    if (!allNum || v.length() == 0 || (v.length() == 1 && v[0] == '-')) return TYPE_STRING;
    if (hasDot) return TYPE_FLOAT;
    return TYPE_INT;
}

String ConfigBleModule::_typeToString(KeyType t) {
    switch (t) {
        case TYPE_BOOL:   return "bool";
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_STRING: return "String";
        default:          return "unknown";
    }
}

// ── Registry helpers ─────────────────────────────────────────────────────

void ConfigBleModule::_loadRegistry() {
    String reg = _store.get<String>("__registry", "");
    if (reg.length() == 0) return;

    int start = 0;
    _keyCount = 0;
    while (start < (int)reg.length() && _keyCount < MAX_KEYS) {
        int comma = reg.indexOf(',', start);
        if (comma < 0) comma = reg.length();
        String entry = reg.substring(start, comma);
        int colon = entry.indexOf(':');
        if (colon > 0) {
            String name = entry.substring(0, colon);
            String typeStr = entry.substring(colon + 1);
            name.trim(); typeStr.trim();

            KeyType t = TYPE_STRING;
            if (typeStr == "bool") t = TYPE_BOOL;
            else if (typeStr == "int") t = TYPE_INT;
            else if (typeStr == "float") t = TYPE_FLOAT;

            strncpy(_registry[_keyCount].name, name.c_str(), sizeof(_registry[0].name) - 1);
            _registry[_keyCount].name[sizeof(_registry[0].name) - 1] = '\0';
            _registry[_keyCount].type = t;
            _registry[_keyCount].defaultVal[0] = '\0';
            _keyCount++;
        }
        start = comma + 1;
    }
}

void ConfigBleModule::_saveRegistry() {
    String reg;
    for (uint8_t i = 0; i < _keyCount; i++) {
        if (i > 0) reg += ",";
        reg += String(_registry[i].name) + ":" + _typeToString(_registry[i].type);
    }
    _store.begin(_namespace.c_str());
    _store.put<String>("__registry", reg);
    _store.end();
}

ConfigBleModule::KeyEntry* ConfigBleModule::_findKey(const char* key) {
    for (uint8_t i = 0; i < _keyCount; i++) {
        if (strcmp(_registry[i].name, key) == 0) return &_registry[i];
    }
    return nullptr;
}

void ConfigBleModule::_registerKey(const char* key, KeyType type) {
    if (_findKey(key)) return;
    if (_keyCount >= MAX_KEYS) return;

    strncpy(_registry[_keyCount].name, key, sizeof(_registry[0].name) - 1);
    _registry[_keyCount].name[sizeof(_registry[0].name) - 1] = '\0';
    _registry[_keyCount].type = type;
    _registry[_keyCount].defaultVal[0] = '\0';
    _keyCount++;
    _saveRegistry();
}

// ── NVS value helpers ─────────────────────────────────────────────────────

void ConfigBleModule::_putValue(const char* key, KeyType type, const String& val) {
    _store.begin(_namespace.c_str());
    switch (type) {
        case TYPE_BOOL:   _store.put<bool>(key, val.equalsIgnoreCase("true")); break;
        case TYPE_INT:    _store.put<int>(key, val.toInt()); break;
        case TYPE_FLOAT:  _store.put<float>(key, val.toFloat()); break;
        case TYPE_STRING: _store.put<String>(key, val); break;
        default: break;
    }
    _store.end();
}

String ConfigBleModule::_getValue(const char* key, KeyType type) {
    _store.begin(_namespace.c_str());
    String result;
    switch (type) {
        case TYPE_BOOL:   result = _store.get<bool>(key, false) ? "true" : "false"; break;
        case TYPE_INT:    result = String(_store.get<int>(key, 0)); break;
        case TYPE_FLOAT:  result = String(_store.get<float>(key, 0.0f), 2); break;
        case TYPE_STRING: result = _store.get<String>(key, ""); break;
        default: result = ""; break;
    }
    _store.end();
    return result;
}

// ── State / schema builders ──────────────────────────────────────────────

String ConfigBleModule::_buildStateString() {
    String s;
    for (uint8_t i = 0; i < _keyCount; i++) {
        if (i > 0) s += ",";
        s += String(_registry[i].name) + "=" + _getValue(_registry[i].name, _registry[i].type);
    }
    return s;
}

String ConfigBleModule::_buildSchemaString() {
    String s;
    for (uint8_t i = 0; i < _keyCount; i++) {
        if (i > 0) s += ",";
        s += String(_registry[i].name) + ":" + _typeToString(_registry[i].type) + ":";
        // default value
        String def = _getValue(_registry[i].name, _registry[i].type);
        s += def;
    }
    return s;
}

// ── BLE callbacks ────────────────────────────────────────────────────────

void ConfigBleModule::_onControlWrite() {
    String raw = _controlChr->getValue().c_str();
    raw.trim();
    Serial.printf("CFG BLE: received \"%s\"\n", raw.c_str());

    int start = 0;
    while (start < (int)raw.length()) {
        int eq = raw.indexOf('=', start);
        if (eq < 0 || eq <= start) break;
        int comma = raw.indexOf(',', eq);
        if (comma < 0) comma = raw.length();
        String key = raw.substring(start, eq);
        String val = raw.substring(eq + 1, comma);
        key.trim(); val.trim();

        Command cmd;
        strncpy(cmd.key, key.c_str(), sizeof(cmd.key) - 1);
        cmd.key[sizeof(cmd.key) - 1] = '\0';
        strncpy(cmd.val, val.c_str(), sizeof(cmd.val) - 1);
        cmd.val[sizeof(cmd.val) - 1] = '\0';
        xQueueSend(_cmdQueue, &cmd, portMAX_DELAY);

        start = comma + 1;
    }
}

void ConfigBleModule::_onStateRead() {
    _stateChr->setValue(_buildStateString().c_str());
}

void ConfigBleModule::_onSchemaRead() {
    _schemaChr->setValue(_buildSchemaString().c_str());
}

// ── FreeRTOS task ────────────────────────────────────────────────────────

void ConfigBleModule::_taskEntry(void* pv) {
    static_cast<ConfigBleModule*>(pv)->_runTask();
}

void ConfigBleModule::_runTask() {
    Command cmd;
    for (;;) {
        if (!_cmdQueue) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (xQueueReceive(_cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            String key(cmd.key);
            String val(cmd.val);

            if (key == "__clear") {
                _store.begin(_namespace.c_str());
                _store.clear();
                _store.end();
                _keyCount = 0;
                _saveRegistry();
                Serial.println("CFG: all config cleared");
            } else {
                KeyEntry* entry = _findKey(key.c_str());
                if (!entry) {
                    KeyType detected = _detectType(val);
                    _registerKey(key.c_str(), detected);
                    entry = _findKey(key.c_str());
                }
                if (entry) {
                    _putValue(key.c_str(), entry->type, val);
                    Serial.printf("CFG: %s = %s (%s)\n", key.c_str(), val.c_str(),
                                    _typeToString(entry->type).c_str());
                }
            }

            // Refresh state + schema after any change
            if (_stateChr) _stateChr->setValue(_buildStateString().c_str());
            if (_schemaChr) _schemaChr->setValue(_buildSchemaString().c_str());
        }
    }
}

// ── Serial interface ─────────────────────────────────────────────────────

void ConfigBleModule::handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    _enqueueSerialCommand(line);
}

void ConfigBleModule::_enqueueSerialCommand(const String& line) {
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));

    if (line.startsWith("name ")) {
        strncpy(cmd.key, "dev_name", sizeof(cmd.key));
        strncpy(cmd.val, line.substring(5).c_str(), sizeof(cmd.val));
    }
    else if (line.startsWith("intv ")) {
        strncpy(cmd.key, "interval", sizeof(cmd.key));
        strncpy(cmd.val, line.substring(5).c_str(), sizeof(cmd.val));
    }
    else if (line.startsWith("enab ")) {
        strncpy(cmd.key, "enabled", sizeof(cmd.key));
        strncpy(cmd.val, line.substring(5).toInt() != 0 ? "true" : "false", sizeof(cmd.val));
    }
    else if (line.startsWith("gain ")) {
        strncpy(cmd.key, "gain", sizeof(cmd.key));
        strncpy(cmd.val, line.substring(5).c_str(), sizeof(cmd.val));
    }
    else if (line.startsWith("cfg ")) {
        // cfg key=val,key=val,...
        String rest = line.substring(4);
        rest.trim();
        int start = 0;
        while (start < (int)rest.length()) {
            int eq = rest.indexOf('=', start);
            if (eq < 0 || eq <= start) break;
            int comma = rest.indexOf(',', eq);
            if (comma < 0) comma = rest.length();
            String k = rest.substring(start, eq);
            String v = rest.substring(eq + 1, comma);
            k.trim(); v.trim();
            Command c;
            strncpy(c.key, k.c_str(), sizeof(c.key) - 1);
            c.key[sizeof(c.key) - 1] = '\0';
            strncpy(c.val, v.c_str(), sizeof(c.val) - 1);
            c.val[sizeof(c.val) - 1] = '\0';
            xQueueSend(_cmdQueue, &c, portMAX_DELAY);
            start = comma + 1;
        }
        return;  // already sent
    }
    else if (line == "show") {
        Serial.println("── Current Config ──");
        for (uint8_t i = 0; i < _keyCount; i++) {
            Serial.printf("  %s = %s (%s)\n",
                _registry[i].name,
                _getValue(_registry[i].name, _registry[i].type).c_str(),
                _typeToString(_registry[i].type).c_str());
        }
        Serial.println("────────────────────");
        _printMenu();
        return;
    }
    else if (line == "clear") {
        strncpy(cmd.key, "__clear", sizeof(cmd.key));
    }
    else {
        Serial.println("Unknown command");
        _printMenu();
        return;
    }

    xQueueSend(_cmdQueue, &cmd, portMAX_DELAY);
}

void ConfigBleModule::_printMenu() {
    Serial.println();
    Serial.println("──── Config Commands ────");
    Serial.println("  name <str>     set device name (string)");
    Serial.println("  intv <n>       set interval seconds (int)");
    Serial.println("  enab 0|1       enable / disable (bool)");
    Serial.println("  gain <n.n>     set gain multiplier (float)");
    Serial.println("  cfg key=val    set any config key (auto-detects type)");
    Serial.println("  show           display all values");
    Serial.println("  clear          reset to defaults (clear all keys)");
    Serial.println("─────────────────────────");
    Serial.println();
}
