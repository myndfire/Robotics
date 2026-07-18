#include "TimedCycleController.h"

TimedCycleController::TimedCycleController(uint8_t relayPin, uint8_t sda, uint8_t scl,
                                           uint8_t oledAddr, uint8_t oledHeight)
    : _relay(relayPin)
    , _display(sda, scl, oledAddr, oledHeight)
    , _storage(nullptr)
    , _relayTaskHandle(nullptr)
    , _displayTaskHandle(nullptr)
    , _onDurationSecs(DEFAULT_ON_SECS)
    , _offDurationSecs(DEFAULT_OFF_SECS)
    , _phaseRemaining(0)
    , _relayOnPhase(true)
    , _cycleCount(0)
{}

void TimedCycleController::begin(StorageController& storage) {
    _storage = &storage;

    _relay.begin();
    _display.begin();
    _display.setFont(0, u8g2_font_5x8_tf);

    _loadDurations();

    _display.setLine(0, "Timed Cycle");
    _display.setLine(1, "Starting...");

    xTaskCreatePinnedToCore(
        _relayTaskEntry, "relay_task", TASK_STACK_SIZE, this,
        RELAY_TASK_PRIORITY, &_relayTaskHandle, 1);

    xTaskCreatePinnedToCore(
        _displayTaskEntry, "display_task", TASK_STACK_SIZE, this,
        DISPLAY_TASK_PRIORITY, &_displayTaskHandle, 1);
}

bool TimedCycleController::setOnDuration(int seconds) {
    if (seconds < 1 || seconds > 3600) return false;
    _onDurationSecs = seconds;
    _saveDurations();
    return true;
}

bool TimedCycleController::setOffDuration(int seconds) {
    if (seconds < 1 || seconds > 3600) return false;
    _offDurationSecs = seconds;
    _saveDurations();
    return true;
}

int  TimedCycleController::getOnDuration() const    { return _onDurationSecs; }
int  TimedCycleController::getOffDuration() const   { return _offDurationSecs; }
bool TimedCycleController::isRelayOn() const        { return _relayOnPhase; }
int  TimedCycleController::getPhaseRemaining() const { return _phaseRemaining; }
unsigned long TimedCycleController::getCycleCount() const { return _cycleCount; }

void TimedCycleController::handleSerial() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("on=")) {
        int val = cmd.substring(3).toInt();
        if (setOnDuration(val)) {
            Serial.printf("ON duration set: %ds\n", _onDurationSecs);
        } else {
            Serial.println("Invalid: must be 1–3600 seconds");
        }
    }
    else if (cmd.startsWith("off=")) {
        int val = cmd.substring(4).toInt();
        if (setOffDuration(val)) {
            Serial.printf("OFF duration set: %ds\n", _offDurationSecs);
        } else {
            Serial.println("Invalid: must be 1–3600 seconds");
        }
    }
    else if (cmd == "s") {
        Serial.println("── Timed Cycle Settings ──");
        Serial.printf("  ON duration:  %ds\n", _onDurationSecs);
        Serial.printf("  OFF duration: %ds\n", _offDurationSecs);
        Serial.printf("  Relay state:  %s\n", _relayOnPhase ? "ON" : "OFF");
        Serial.printf("  Phase left:   %ds\n", _phaseRemaining);
        Serial.printf("  Cycles:       %lu\n", _cycleCount);
        Serial.println("──────────────────────────");
    }
}

void TimedCycleController::_loadDurations() {
    _storage->begin("timedcycle");
    _onDurationSecs  = _storage->get<int>("onSecs",  DEFAULT_ON_SECS);
    _offDurationSecs = _storage->get<int>("offSecs", DEFAULT_OFF_SECS);
    _storage->end();
}

void TimedCycleController::_saveDurations() {
    _storage->begin("timedcycle");
    _storage->put<int>("onSecs", _onDurationSecs);
    _storage->put<int>("offSecs", _offDurationSecs);
    _storage->end();
}

void TimedCycleController::_relayTaskEntry(void* pv) {
    static_cast<TimedCycleController*>(pv)->_relayTask();
}

void TimedCycleController::_displayTaskEntry(void* pv) {
    static_cast<TimedCycleController*>(pv)->_displayTask();
}

void TimedCycleController::_relayTask() {
    for (;;) {
        _relay.turnOn();
        _relayOnPhase = true;

        for (int i = _onDurationSecs; i > 0; i--) {
            _phaseRemaining = i;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        _relay.turnOff();
        _relayOnPhase = false;

        for (int i = _offDurationSecs; i > 0; i--) {
            _phaseRemaining = i;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void TimedCycleController::_displayTask() {
    for (;;) {
        if (_relayOnPhase && _phaseRemaining == _onDurationSecs) {
            _cycleCount++;
        }

        _display.setLine(0, "Relay: %s", _relayOnPhase ? "ON " : "OFF");
        _display.setLine(1, "Phase: %ds", _phaseRemaining);
        _display.setLine(2, "ON dur:%ds OFF:%ds", _onDurationSecs, _offDurationSecs);

        unsigned long hrs = _cycleCount * (_onDurationSecs + _offDurationSecs) / 3600;
        unsigned long min = (_cycleCount * (_onDurationSecs + _offDurationSecs) % 3600) / 60;
        unsigned long sec = _cycleCount * (_onDurationSecs + _offDurationSecs) % 60;
        _display.setLine(3, "Cycles: %lu (%luh%lum%lus)", _cycleCount, hrs, min, sec);

        _display.processNext(portMAX_DELAY);
    }
}
