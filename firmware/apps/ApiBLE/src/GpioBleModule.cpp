#include "GpioBleModule.h"

// ── UUIDs ────────────────────────────────────────────────────────────────

const char* GpioBleModule::API_SERVICE_UUID    = "0000ba00-0000-1000-8000-00805f9b34fb";
const char* GpioBleModule::CONTROL_CHAR_UUID   = "0000ba01-0000-1000-8000-00805f9b34fb";
const char* GpioBleModule::STATUS_CHAR_UUID    = "0000ba02-0000-1000-8000-00805f9b34fb";

// ── Constructor / begin ──────────────────────────────────────────────────

GpioBleModule::GpioBleModule() {}

void GpioBleModule::begin() {
    memset(_pins, 0, sizeof(_pins));
    _loadState();

#ifdef HAS_NEOPIXEL
    _led = new LedRGBController(_ledPin, 1, _ledType);
    _led->begin();
    _led->turn_Off();
#endif

    _cmdQueue = xQueueCreate(QUEUE_DEPTH, sizeof(Command));
    xTaskCreatePinnedToCore(_taskEntry, "gpio_task", TASK_STACK,
                            this, TASK_PRIO, &_taskHandle, 1);

    Serial.println("GpioBleModule: started");
}

// ── BLE attach ───────────────────────────────────────────────────────────

void GpioBleModule::attach(BLEServer* server) {
    _apiService = server->createService(API_SERVICE_UUID);

    // BA01 — Control (write)
    _controlChr = _apiService->createCharacteristic(
        CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    {
        class CB : public BLECharacteristicCallbacks {
            GpioBleModule* _m;
        public:
            CB(GpioBleModule* m) : _m(m) {}
            void onWrite(BLECharacteristic* c) override { (void)c; _m->_onControlWrite(); }
        };
        _controlChr->setCallbacks(new CB(this));
    }

    // BA02 — Status (read)
    _statusChr = _apiService->createCharacteristic(
        STATUS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    {
        class CB : public BLECharacteristicCallbacks {
            GpioBleModule* _m;
        public:
            CB(GpioBleModule* m) : _m(m) {}
            void onRead(BLECharacteristic* c) override { (void)c; _m->_onStatusRead(); }
        };
        _statusChr->setCallbacks(new CB(this));
    }
    _statusChr->setValue(_buildStatus().c_str());

    _apiService->start();
    Serial.println("GpioBleModule: BA00 service attached");
}

// ── NVS persistence ──────────────────────────────────────────────────────

void GpioBleModule::_loadState() {
    _store.begin("gpio");
    String pinsStr = _store.get<String>("pins", "");
    if (pinsStr.length() > 0) {
        int start = 0;
        while (start < (int)pinsStr.length()) {
            int semi = pinsStr.indexOf(';', start);
            if (semi < 0) semi = pinsStr.length();
            String entry = pinsStr.substring(start, semi);
            int colon1 = entry.indexOf(':');
            int colon2 = entry.indexOf(':', colon1 + 1);
            if (colon1 > 0 && colon2 > colon1) {
                uint8_t num = entry.substring(0, colon1).toInt();
                uint8_t mode = entry.substring(colon1 + 1, colon2).toInt();
                uint16_t val = entry.substring(colon2 + 1).toInt();
                if (_pinCount < MAX_PINS) {
                    _pins[_pinCount] = {num, mode, val};
                    if (mode == 1) {
                        pinMode(num, OUTPUT);
                        digitalWrite(num, val ? HIGH : LOW);
                    } else if (mode == 2) {
                        pinMode(num, INPUT);
                    } else if (mode == 3) {
                        // analog input — no setup needed
                    }
                    _pinCount++;
                }
            }
            start = semi + 1;
        }
    }
#ifdef HAS_NEOPIXEL
    _ledPin  = _store.get<int>("led_pin", _ledPin);
    _ledType = _store.get<unsigned long>("led_type", _ledType);
#endif
    _store.end();
}

void GpioBleModule::_saveState() {
    _store.begin("gpio");
    String pinsStr;
    for (uint8_t i = 0; i < _pinCount; i++) {
        if (i > 0) pinsStr += ";";
        pinsStr += String(_pins[i].number) + ":" + String(_pins[i].mode) + ":" + String(_pins[i].value);
    }
    _store.put<String>("pins", pinsStr);
#ifdef HAS_NEOPIXEL
    _store.put<int>("led_pin", _ledPin);
    _store.put<unsigned long>("led_type", _ledType);
#endif
    _store.end();
}

// ── Pin helpers ──────────────────────────────────────────────────────────

GpioBleModule::GpioPin* GpioBleModule::_findPin(uint8_t number) {
    for (uint8_t i = 0; i < _pinCount; i++) {
        if (_pins[i].number == number) return &_pins[i];
    }
    return nullptr;
}

GpioBleModule::GpioPin* GpioBleModule::_getOrCreatePin(uint8_t number, uint8_t mode) {
    GpioPin* p = _findPin(number);
    if (p) {
        p->mode = mode;
        return p;
    }
    if (_pinCount < MAX_PINS) {
        _pins[_pinCount] = {number, mode, 0};
        return &_pins[_pinCount++];
    }
    return nullptr;
}

// ── Status string ─────────────────────────────────────────────────────────

String GpioBleModule::_buildStatus() {
    String s;
#ifdef HAS_NEOPIXEL
    s += "led=" + String(_ledR) + ";" + String(_ledG) + ";" + String(_ledB);
#else
    s += "led=no_led";
#endif
    for (uint8_t i = 0; i < _pinCount; i++) {
        s += ",pin" + String(_pins[i].number) + "=" + String(_pins[i].value);
    }
    return s;
}

// ── BLE callbacks ──────────────────────────────────────────────────────────

void GpioBleModule::_onControlWrite() {
    String raw = _controlChr->getValue().c_str();
    raw.trim();
    Serial.printf("GPIO BLE: received \"%s\"\n", raw.c_str());

    int start = 0;
    while (start < (int)raw.length()) {
        int eq = raw.indexOf('=', start);
        if (eq < 0 || eq <= start) break;
        int comma = raw.indexOf(',', eq);
        if (comma < 0) comma = raw.length();
        String key = raw.substring(start, eq);
        String val = raw.substring(eq + 1, comma);
        key.trim(); val.trim();

        if (key == "get") {
            _handleGet(val.c_str());
        } else {
            Command cmd;
            strncpy(cmd.key, key.c_str(), sizeof(cmd.key) - 1);
            cmd.key[sizeof(cmd.key) - 1] = '\0';
            strncpy(cmd.val, val.c_str(), sizeof(cmd.val) - 1);
            cmd.val[sizeof(cmd.val) - 1] = '\0';
            xQueueSend(_cmdQueue, &cmd, portMAX_DELAY);
        }
        start = comma + 1;
    }
}

void GpioBleModule::_onStatusRead() {
    _statusChr->setValue(_buildStatus().c_str());
}

// ── Command handlers ───────────────────────────────────────────────────────

void GpioBleModule::_handleCommand(const char* key, const char* val) {
    if (strcmp(key, "pin") == 0)    _handlePinConfig(val);
    else if (strcmp(key, "set") == 0) _handleSet(val);
    else if (strcmp(key, "led") == 0) _handleLed(val);
}

void GpioBleModule::_handlePinConfig(const char* val) {
    String v(val);
    int colon = v.indexOf(':');
    if (colon <= 0) return;
    uint8_t num = v.substring(0, colon).toInt();
    String modeStr = v.substring(colon + 1);
    modeStr.trim();

    uint8_t mode = 0;
    if (modeStr == "out") mode = 1;
    else if (modeStr == "in") mode = 2;
    else if (modeStr == "ain") mode = 3;

    GpioPin* p = _getOrCreatePin(num, mode);
    if (!p) return;

    if (mode == 1) pinMode(num, OUTPUT);
    else if (mode == 2) pinMode(num, INPUT);
    else if (mode == 3) { /* analog input, no setup */ }

    _saveState();
}

void GpioBleModule::_handleSet(const char* val) {
    String v(val);
    int colon = v.indexOf(':');
    if (colon <= 0) return;
    uint8_t num = v.substring(0, colon).toInt();
    uint8_t value = v.substring(colon + 1).toInt();

    GpioPin* p = _findPin(num);
    if (p && p->mode == 1) {
        digitalWrite(num, value ? HIGH : LOW);
        p->value = value;
    }
    _saveState();
}

void GpioBleModule::_handleGet(const char* val) {
    uint8_t num = String(val).toInt();
    GpioPin* p = _findPin(num);
    if (!p) return;

    uint16_t value = 0;
    if (p->mode == 2) {
        value = digitalRead(num);
    } else if (p->mode == 3) {
        value = analogRead(num);
    }
    p->value = value;

    // Immediate status update so BA02 reflects the new value
    _statusChr->setValue(_buildStatus().c_str());
    Serial.printf("GPIO: get pin %d = %d\n", num, value);
}

void GpioBleModule::_handleLed(const char* val) {
#ifdef HAS_NEOPIXEL
    String color(val);
    if (color == "red")     { _ledR = 255; _ledG = 0;   _ledB = 0;   _led->turn_Red(); }
    else if (color == "green")  { _ledR = 0;   _ledG = 255; _ledB = 0;   _led->turn_Green(); }
    else if (color == "blue")   { _ledR = 0;   _ledG = 0;   _ledB = 255; _led->turn_Blue(); }
    else if (color == "cyan")   { _ledR = 0;   _ledG = 255; _ledB = 255; _led->turn_Cyan(); }
    else if (color == "magenta"){ _ledR = 255; _ledG = 0;   _ledB = 255; _led->turn_Magenta(); }
    else if (color == "yellow") { _ledR = 255; _ledG = 200; _ledB = 0;   _led->turn_Yellow(); }
    else if (color == "white")  { _ledR = 255; _ledG = 255; _ledB = 255; _led->turn_White(); }
    else if (color == "on")     { _ledR = 255; _ledG = 255; _ledB = 255; _led->turn_On(); }
    else if (color == "off")    { _ledR = 0;   _ledG = 0;   _ledB = 0;   _led->turn_Off(); }
    _saveState();
#endif
}

// ── FreeRTOS task ────────────────────────────────────────────────────────

void GpioBleModule::_taskEntry(void* pv) {
    static_cast<GpioBleModule*>(pv)->_runTask();
}

void GpioBleModule::_runTask() {
    Command cmd;
    for (;;) {
        if (_cmdQueue && xQueueReceive(_cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            _handleCommand(cmd.key, cmd.val);
            if (_statusChr) _statusChr->setValue(_buildStatus().c_str());
        }
    }
}
