/*
 * BleApiServer.cpp — full implementation
 *
 * Two-characteristic BLE API: BA01 (Write) receives key=val commands,
 * BA02 (Read) returns full state string.
 *
 * NVS persistence (namespace "bluetooth_api"):
 *   — LED colour, pin, type always persisted.
 *   — GPIO pin modes + values persisted only when :persist suffix supplied.
 *
 * Command reference (all comma-separated, sent to BA01):
 *
 *   pin=<n>:<out|in|ain>[:persist]        Configure GPIO <n>
 *   set=<n>:<0|1>[:persist]               digitalWrite GPIO <n>
 *   get=<n>                               digitalRead or analogRead GPIO <n>
 *   led=<colour>                          NeoPixel colour
 *   onboard_RGBLed_pin=<n>                Re-map NeoPixel to GPIO <n>
 *   onboard_RGBLed_type=<rgb|grb|gbr>     Re-map colour byte order
 */

#include "BleApiServer.h"

// ── BLE Service & Characteristic UUIDs ─────────────────────────────────

#define API_SERVICE_UUID        "0000ba00-0000-1000-8000-00805f9b34fb"
#define CONTROL_CHAR_UUID       "0000ba01-0000-1000-8000-00805f9b34fb"
#define STATUS_CHAR_UUID        "0000ba02-0000-1000-8000-00805f9b34fb"

// ── Constructor ────────────────────────────────────────────────────────

BleApiServer::BleApiServer(uint8_t ledPin, neoPixelType ledType)
    : _ledPin(ledPin), _ledType(ledType)
{
}

// ── Public: begin() — one-shot setup ───────────────────────────────────

void BleApiServer::begin() {
    Serial.begin(921600);
    delay(1000);

    _led = new LedRGBController(_ledPin, 1, _ledType);
    _led->begin();
    _led->turn_Off();

    _loadState();

    _setupBLE();

    _bleServer->getAdvertising()->start();

    Serial.println();
    Serial.println("BleApiServer ready:");
    Serial.println("  Advertising as \"ESP32-API\"");
    Serial.println("  Service UUID: 0000BA00-0000-1000-8000-00805F9B34FB");
    Serial.println("  Write key=val commands to BA01, read state from BA02");
    Serial.println();
    Serial.println("  Setup:    pin=<n>:<out|in|ain>[:persist]");
    Serial.println("  Write:    set=<n>:<0|1>[:persist]");
    Serial.println("  Read:     get=<n>");
    Serial.println("  LED:      led=<red|green|blue|cyan|magenta|yellow|white|on|off>");
    Serial.println("  Config:   onboard_RGBLed_pin=<n>  onboard_RGBLed_type=<rgb|grb|gbr>");
}

// ── BLE setup ──────────────────────────────────────────────────────────

void BleApiServer::_setupBLE() {
    BLEDevice::init("ESP32-API");
    _bleServer = BLEDevice::createServer();

    class SrvCB : public BLEServerCallbacks {
        BleApiServer* _srv;
    public:
        SrvCB(BleApiServer* srv) : _srv(srv) {}
        void onConnect(BLEServer* s) override {
            (void)s; (void)_srv;
            Serial.println("BLE: client connected");
        }
        void onDisconnect(BLEServer* s) override {
            (void)s; (void)_srv;
            Serial.println("BLE: client disconnected, restarting advertising");
            s->getAdvertising()->start();
        }
    };
    _bleServer->setCallbacks(new SrvCB(this));

    _apiService = _bleServer->createService(API_SERVICE_UUID);

    // BA01 — Control (write)
    _controlChr = _apiService->createCharacteristic(
        CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    {
        class CtrlCB : public BLECharacteristicCallbacks {
            BleApiServer* _srv;
        public:
            CtrlCB(BleApiServer* srv) : _srv(srv) {}
            void onWrite(BLECharacteristic* c) override { (void)c; _srv->_onControlWrite(); }
        };
        _controlChr->setCallbacks(new CtrlCB(this));
    }

    // BA02 — Status (read)
    _statusChr = _apiService->createCharacteristic(
        STATUS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    {
        class StatCB : public BLECharacteristicCallbacks {
            BleApiServer* _srv;
        public:
            StatCB(BleApiServer* srv) : _srv(srv) {}
            void onRead(BLECharacteristic* c) override { (void)c; _srv->_onStatusRead(); }
        };
        _statusChr->setCallbacks(new StatCB(this));
    }
    _statusChr->setValue(_buildStatus());

    _apiService->start();

    Serial.println("BLE: API service started");
}

// ── BLE callbacks ──────────────────────────────────────────────────────

void BleApiServer::_onControlWrite() {
    String input = _controlChr->getValue();
    input.trim();

    Serial.printf("BLE: received \"%s\"\n", input.c_str());

    int start = 0;
    while (start < (int)input.length()) {
        int eq = input.indexOf('=', start);
        if (eq < 0 || eq <= start) break;

        int comma = input.indexOf(',', eq);
        if (comma < 0) comma = input.length();

        String key = input.substring(start, eq);
        String val = input.substring(eq + 1, comma);
        key.trim();
        val.trim();

        _handleCommand(key, val);

        start = comma + 1;
    }

    _statusChr->setValue(_buildStatus());
}

void BleApiServer::_onStatusRead() {
    _statusChr->setValue(_buildStatus());
}

// ── Command dispatcher ─────────────────────────────────────────────────

void BleApiServer::_handleCommand(const String& key, const String& val) {
    if (key == "pin")                       _handlePinConfig(val);
    else if (key == "set")                  _handleSet(val);
    else if (key == "get")                  _handleGet(val);
    else if (key == "led")                  _handleLed(val);
    else if (key == "onboard_RGBLed_pin")   _handleLedPin(val);
    else if (key == "onboard_RGBLed_type")  _handleLedType(val);
    else
        Serial.printf("BLE: unknown command \"%s=%s\"\n", key.c_str(), val.c_str());
}

// ── pin=<n>:<mode>[:persist] ────────────────────────────────────────────

void BleApiServer::_handlePinConfig(const String& val) {
    int colon = val.indexOf(':');
    if (colon < 0) return;

    uint8_t number = (uint8_t)val.substring(0, colon).toInt();
    String rest    = val.substring(colon + 1);
    rest.trim();

    // Parse mode vs mode:persist
    int colon2 = rest.indexOf(':');
    String mode;
    bool persist = false;
    if (colon2 > 0) {
        mode    = rest.substring(0, colon2);
        persist = (rest.substring(colon2 + 1) == "persist");
    } else {
        mode = rest;
    }

    GpioPin* p = _getOrCreatePin(number, 0);
    if (!p) return;

    if (mode == "out") {
        p->mode = 1;
        pinMode(number, OUTPUT);
        digitalWrite(number, LOW);
        p->value = 0;
        Serial.printf("  pin %d → OUTPUT%s\n", number, persist ? " (persisted)" : "");
    } else if (mode == "in") {
        p->mode = 2;
        pinMode(number, INPUT);
        Serial.printf("  pin %d → INPUT%s\n", number, persist ? " (persisted)" : "");
    } else if (mode == "ain") {
        p->mode = 3;
        analogRead(number);
        Serial.printf("  pin %d → ANALOG%s\n", number, persist ? " (persisted)" : "");
    } else {
        Serial.printf("  pin %d unknown mode \"%s\"\n", number, mode.c_str());
        return;
    }

    p->persistConfig = persist;
    if (persist) _savePins();
}

// ── set=<n>:<0|1>[:persist] ─────────────────────────────────────────────

void BleApiServer::_handleSet(const String& val) {
    int colon = val.indexOf(':');
    if (colon < 0) return;

    uint8_t number = (uint8_t)val.substring(0, colon).toInt();
    String rest    = val.substring(colon + 1);

    // Parse state vs state:persist
    int colon2 = rest.indexOf(':');
    uint8_t state;
    bool persist = false;
    if (colon2 > 0) {
        state   = (uint8_t)rest.substring(0, colon2).toInt();
        persist = (rest.substring(colon2 + 1) == "persist");
    } else {
        state = (uint8_t)rest.toInt();
    }

    GpioPin* p = _getOrCreatePin(number, 1);
    if (!p) return;

    digitalWrite(number, state ? HIGH : LOW);
    p->value = state;

    if (persist) p->persistValue = true;

    Serial.printf("  set pin %d → %d%s\n", number, state, persist ? " (persisted)" : "");

    if (p->persistConfig || p->persistValue) _savePins();
}

// ── get=<n> ────────────────────────────────────────────────────────────

void BleApiServer::_handleGet(const String& val) {
    uint8_t number = (uint8_t)val.toInt();
    if (number == 0 && val != "0") return;

    GpioPin* p = _getOrCreatePin(number, 2);
    if (!p) return;

    if (p->mode == 3) {
        p->value = analogRead(number);
        Serial.printf("  get pin %d (analog) → %d\n", number, p->value);
    } else {
        p->value = digitalRead(number);
        Serial.printf("  get pin %d (digital) → %d\n", number, p->value);
    }
}

// ── led=<colour> ───────────────────────────────────────────────────────

void BleApiServer::_handleLed(const String& val) {
    if (!_led) return;

    if (val == "red")           { _r = 255; _g = 0;   _b = 0;   _led->turn_Red();    }
    else if (val == "green")    { _r = 0;   _g = 255; _b = 0;   _led->turn_Green();  }
    else if (val == "blue")     { _r = 0;   _g = 0;   _b = 255; _led->turn_Blue();   }
    else if (val == "cyan")     { _r = 0;   _g = 255; _b = 255; _led->turn_Cyan();   }
    else if (val == "magenta")  { _r = 255; _g = 0;   _b = 255; _led->turn_Magenta();}
    else if (val == "yellow")   { _r = 255; _g = 255; _b = 0;   _led->turn_Yellow(); }
    else if (val == "white")    { _r = 255; _g = 255; _b = 255; _led->turn_White();  }
    else if (val == "on")       { _r = 0;   _g = 255; _b = 0;   _led->turn_Green();  }
    else if (val == "off")      { _r = 0;   _g = 0;   _b = 0;   _led->turn_Off();    }
    else { Serial.printf("BLE: unknown led colour \"%s\"\n", val.c_str()); return; }

    Serial.printf("  led → %s (%d,%d,%d)\n", val.c_str(), _r, _g, _b);
    _saveLedColor();
}

// ── onboard_RGBLed_pin=<n> ─────────────────────────────────────────────

void BleApiServer::_handleLedPin(const String& val) {
    uint8_t newPin = (uint8_t)val.toInt();
    if (newPin == 0 && val != "0") return;
    if (newPin == _ledPin) return;

    delete _led;
    _ledPin = newPin;
    _led = new LedRGBController(_ledPin, 1, _ledType);
    _led->begin();
    _led->turn_Color(_r, _g, _b);

    Serial.printf("  onboard_RGBLed_pin → %d\n", newPin);
    _saveLedPin();
}

// ── onboard_RGBLed_type=<rgb|grb|gbr> ──────────────────────────────────

void BleApiServer::_handleLedType(const String& val) {
    neoPixelType newType;
    if (val == "rgb")       newType = NEO_RGB + NEO_KHZ800;
    else if (val == "grb")  newType = NEO_GRB + NEO_KHZ800;
    else if (val == "gbr")  newType = NEO_GBR + NEO_KHZ800;
    else { Serial.printf("  unknown led type \"%s\"\n", val.c_str()); return; }

    if (newType == _ledType) return;

    delete _led;
    _ledType = newType;
    _led = new LedRGBController(_ledPin, 1, _ledType);
    _led->begin();
    _led->turn_Color(_r, _g, _b);

    Serial.printf("  onboard_RGBLed_type → %s\n", val.c_str());
    _saveLedType();
}

// ── Status builder ─────────────────────────────────────────────────────

String BleApiServer::_buildStatus() {
    String s = "led=" + String(_r) + ";" + String(_g) + ";" + String(_b);
    for (uint8_t i = 0; i < _pinCount; i++) {
        s += ",pin" + String(_pins[i].number) + "=" + String(_pins[i].value);
    }
    return s;
}

// ── GPIO pin registry ──────────────────────────────────────────────────

BleApiServer::GpioPin* BleApiServer::_findPin(uint8_t number) {
    for (uint8_t i = 0; i < _pinCount; i++) {
        if (_pins[i].number == number) return &_pins[i];
    }
    return nullptr;
}

BleApiServer::GpioPin* BleApiServer::_getOrCreatePin(uint8_t number, uint8_t mode) {
    GpioPin* p = _findPin(number);
    if (!p) {
        if (_pinCount >= MAX_PINS) {
            Serial.printf("  pin registry full (max %d)\n", MAX_PINS);
            return nullptr;
        }
        p = &_pins[_pinCount++];
        p->number = number;
        p->mode = mode;
        p->value = 0;
        p->persistConfig = false;
        p->persistValue  = false;

        if (mode == 1) {
            pinMode(number, OUTPUT);
            digitalWrite(number, LOW);
        } else if (mode == 2) {
            pinMode(number, INPUT);
        } else if (mode == 3) {
            analogRead(number);
        }
    } else if (p->mode == 0 && mode != 0) {
        p->mode = mode;
    }
    return p;
}

// ── NVS: load persisted state ──────────────────────────────────────────

void BleApiServer::_loadState() {
    _storage.begin("bluetooth_api");

    // LED colour
    _r = (uint8_t)_storage.get<int>("led_r", 0);
    _g = (uint8_t)_storage.get<int>("led_g", 0);
    _b = (uint8_t)_storage.get<int>("led_b", 0);
    _led->turn_Color(_r, _g, _b);

    // LED pin
    uint8_t savedPin = (uint8_t)_storage.get<int>("led_pin", _ledPin);
    if (savedPin != _ledPin) {
        delete _led;
        _ledPin = savedPin;
        _led = new LedRGBController(_ledPin, 1, _ledType);
        _led->begin();
        _led->turn_Color(_r, _g, _b);
    }

    // LED type
    unsigned long savedType = _storage.get<unsigned long>("led_type", (unsigned long)_ledType);
    if (savedType != (uint16_t)_ledType) {
        delete _led;
        _ledType = (neoPixelType)savedType;
        _led = new LedRGBController(_ledPin, 1, _ledType);
        _led->begin();
        _led->turn_Color(_r, _g, _b);
    }

    // Pins — format: "4:1:1:1:1,7:2:0:1:0"  (pin:mode:value:cfgFlag:valFlag)
    String pinsStr = _storage.get<String>("pins", "");
    if (pinsStr.length() > 0) {
        int start = 0;
        while (start < (int)pinsStr.length()) {
            int c1 = pinsStr.indexOf(':', start);
            int c2 = pinsStr.indexOf(':', c1 + 1);
            int c3 = pinsStr.indexOf(':', c2 + 1);
            int c4 = pinsStr.indexOf(':', c3 + 1);
            if (c1 < 0 || c2 < 0 || c3 < 0 || c4 < 0) break;

            int comma = pinsStr.indexOf(',', c4);
            if (comma < 0) comma = pinsStr.length();

            uint8_t  number = (uint8_t)pinsStr.substring(start, c1).toInt();
            uint8_t  mode   = (uint8_t)pinsStr.substring(c1 + 1, c2).toInt();
            uint16_t value  = (uint16_t)pinsStr.substring(c2 + 1, c3).toInt();
            bool     cfgF   = (pinsStr.substring(c3 + 1, c4).toInt() == 1);
            bool     valF   = (pinsStr.substring(c4 + 1, comma).toInt() == 1);

            GpioPin* p = nullptr;
            if (_pinCount < MAX_PINS) {
                p = &_pins[_pinCount++];
                p->number = number;
                p->mode   = mode;
                p->value  = cfgF ? value : 0;
                p->persistConfig = cfgF;
                p->persistValue  = valF;

                if (mode == 1) {
                    pinMode(number, OUTPUT);
                    digitalWrite(number, p->value ? HIGH : LOW);
                } else if (mode == 2) {
                    pinMode(number, INPUT);
                } else if (mode == 3) {
                    analogRead(number);
                }
                Serial.printf("  NVS restore pin %d mode=%d value=%d\n", number, mode, p->value);
            }

            start = comma + 1;
        }
    }

    _storage.end();

    if (_r || _g || _b)
        Serial.printf("  NVS restore LED (%d,%d,%d)\n", _r, _g, _b);
    if (_ledPin != 48)
        Serial.printf("  NVS restore LED pin=%d\n", _ledPin);
}

// ── NVS: save pins string ──────────────────────────────────────────────

void BleApiServer::_savePins() {
    String s;
    for (uint8_t i = 0; i < _pinCount; i++) {
        if (!_pins[i].persistConfig && !_pins[i].persistValue) continue;
        if (s.length() > 0) s += ",";
        s += String(_pins[i].number) + ":" + String(_pins[i].mode) + ":" +
             String(_pins[i].value) + ":" +
             String(_pins[i].persistConfig ? 1 : 0) + ":" +
             String(_pins[i].persistValue ? 1 : 0);
    }

    _storage.begin("bluetooth_api");
    if (s.length() > 0)
        _storage.put<String>("pins", s);
    else
        _storage.remove("pins");
    _storage.end();
}

// ── NVS: save LED helpers ──────────────────────────────────────────────

void BleApiServer::_saveLedColor() {
    _storage.begin("bluetooth_api");
    _storage.put<int>("led_r", (int)_r);
    _storage.put<int>("led_g", (int)_g);
    _storage.put<int>("led_b", (int)_b);
    _storage.end();
}

void BleApiServer::_saveLedPin() {
    _storage.begin("bluetooth_api");
    _storage.put<int>("led_pin", (int)_ledPin);
    _storage.end();
}

void BleApiServer::_saveLedType() {
    _storage.begin("bluetooth_api");
    _storage.put<unsigned long>("led_type", (unsigned long)_ledType);
    _storage.end();
}
