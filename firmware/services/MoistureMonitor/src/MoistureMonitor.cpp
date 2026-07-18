#include "MoistureMonitor.h"

MoistureMonitor::MoistureMonitor(uint8_t sensorPin, uint8_t sda, uint8_t scl,
                                 uint8_t oledAddr, uint8_t oledHeight)
    : _sensor(sensorPin)
    , _display(sda, scl, oledAddr, oledHeight)
    , _storage(nullptr)
    , _sensorTaskHandle(nullptr)
    , _displayTaskHandle(nullptr)
    , _analogValue(0)
    , _moisturePercent(0)
    , _dry(false)
    , _wet(false)
{}

void MoistureMonitor::begin(StorageController& storage) {
    _storage = &storage;

    _display.begin();
    _display.setFont(0, u8g2_font_5x8_tf);

    _sensor.begin(*_storage);

    _display.setLine(0, "Moisture Monitor");
    _display.setLine(1, "Starting...");

    xTaskCreatePinnedToCore(
        _sensorTaskEntry, "sensor_task", TASK_STACK_SIZE, this,
        SENSOR_TASK_PRIORITY, &_sensorTaskHandle, 1);

    xTaskCreatePinnedToCore(
        _displayTaskEntry, "display_task", TASK_STACK_SIZE, this,
        DISPLAY_TASK_PRIORITY, &_displayTaskHandle, 1);
}

int MoistureMonitor::getMoisturePercent() const { return _moisturePercent; }
int MoistureMonitor::getAnalogValue() const { return _analogValue; }
bool MoistureMonitor::isDry() const { return _dry; }
bool MoistureMonitor::isWet() const { return _wet; }

const char* MoistureMonitor::getStatus() const {
    return _dry ? "DRY" : (_wet ? "WET" : "OK ");
}

void MoistureMonitor::_sensorTaskEntry(void* pv) {
    static_cast<MoistureMonitor*>(pv)->_sensorTask();
}

void MoistureMonitor::_displayTaskEntry(void* pv) {
    static_cast<MoistureMonitor*>(pv)->_displayTask();
}

void MoistureMonitor::_sensorTask() {
    for (;;) {
        _sensor.read();
        _analogValue = _sensor.analogValue();
        _moisturePercent = _sensor.moisturePercent();

        _dry = (_moisturePercent < _sensor.dryTriggerPct());
        _wet = (_moisturePercent > _sensor.wetTriggerPct());

        const char* status = getStatus();

        _display.setLine(0, "Moist: %d%% %s", _moisturePercent, status);
        _display.setLine(1, "ADC: %d", _analogValue);
        _display.setLine(2, "Ref: D=%d W=%d", _sensor.dryRef(), _sensor.wetRef());
        _display.setLine(3, "Trig: dry<%d%% wet>%d%%",
            _sensor.dryTriggerPct(), _sensor.wetTriggerPct());

        Serial.printf("ADC=%d  Moisture=%d%%  Status=%s\n",
            _analogValue, _moisturePercent, status);

        vTaskDelay(pdMS_TO_TICKS(READ_INTERVAL_MS));
    }
}

void MoistureMonitor::_displayTask() {
    for (;;) {
        _display.processNext(portMAX_DELAY);
    }
}
