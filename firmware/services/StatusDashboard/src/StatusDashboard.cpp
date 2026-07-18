#include "StatusDashboard.h"

StatusDashboard::StatusDashboard(uint8_t sda, uint8_t scl,
                                 uint8_t oledAddr, uint8_t oledHeight)
    : _display(sda, scl, oledAddr, oledHeight)
    , _timerTaskHandle(nullptr)
    , _sensorTaskHandle(nullptr)
    , _statsTaskHandle(nullptr)
    , _displayTaskHandle(nullptr)
    , _uptimeSeconds(0)
    , _firmwareVersion("v1.0.0")
{}

void StatusDashboard::begin() {
    _display.begin();
    WiFi.mode(WIFI_STA);

    // Enable scrolling on lines 0–2
    _display.setScrollEnabled(0, true);
    _display.setScrollSpeed(0, 200);
    _display.setScrollEnabled(1, true);
    _display.setScrollSpeed(1, 250);
    _display.setScrollEnabled(2, true);
    _display.setScrollSpeed(2, 300);

    _display.setFont(0, u8g2_font_5x8_tf);
    _display.setLine(0, "Starting...");

    xTaskCreatePinnedToCore(
        _timerTaskEntry, "timer_task", TASK_STACK_SIZE, this,
        TIMER_TASK_PRIORITY, &_timerTaskHandle, 0);

    xTaskCreatePinnedToCore(
        _sensorTaskEntry, "sensor_task", TASK_STACK_SIZE, this,
        SENSOR_TASK_PRIORITY, &_sensorTaskHandle, 1);

    xTaskCreatePinnedToCore(
        _statsTaskEntry, "stats_task", TASK_STACK_SIZE, this,
        STATS_TASK_PRIORITY, &_statsTaskHandle, 1);

    xTaskCreatePinnedToCore(
        _displayTaskEntry, "display_task", TASK_STACK_SIZE, this,
        DISPLAY_TASK_PRIORITY, &_displayTaskHandle, 1);
}

void StatusDashboard::setFirmwareVersion(const char* version) {
    _firmwareVersion = version;
}

void StatusDashboard::setScrollSpeed(uint8_t line, uint16_t msPerStep) {
    _display.setScrollSpeed(line, msPerStep);
}

void StatusDashboard::_timerTaskEntry(void* pv) { static_cast<StatusDashboard*>(pv)->_timerTask(); }
void StatusDashboard::_sensorTaskEntry(void* pv) { static_cast<StatusDashboard*>(pv)->_sensorTask(); }
void StatusDashboard::_statsTaskEntry(void* pv) { static_cast<StatusDashboard*>(pv)->_statsTask(); }
void StatusDashboard::_displayTaskEntry(void* pv) { static_cast<StatusDashboard*>(pv)->_displayTask(); }

void StatusDashboard::_timerTask() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        _uptimeSeconds++;

        unsigned long h = _uptimeSeconds / 3600;
        unsigned long m = (_uptimeSeconds % 3600) / 60;
        unsigned long s = _uptimeSeconds % 60;

        _display.setLine(0,
            "Uptime: %luh %lum %lus | Tasks: %lu",
            h, m, s, uxTaskGetNumberOfTasks());
    }
}

void StatusDashboard::_sensorTask() {
    for (;;) {
        int sim = random(0, 100);

        _display.setLine(1,
            "Sensor: %d%% | Ref: D=3100 W=750 | Trig: dry<25%% wet>35%% | ADC=12bit 11dB",
            sim);

        vTaskDelay(pdMS_TO_TICKS(7000));
    }
}

void StatusDashboard::_statsTask() {
    for (;;) {
        _display.setLine(2,
            "Heap: %d/%d | PSRAM: %d | RSSI: %d dBm | SDK: %s",
            ESP.getFreeHeap(),
            ESP.getHeapSize(),
            ESP.getPsramSize(),
            WiFi.RSSI(),
            ESP.getSdkVersion());

        _display.setLine(3, _firmwareVersion.c_str());

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void StatusDashboard::_displayTask() {
    for (;;) {
        _display.processNext(portMAX_DELAY);
    }
}
