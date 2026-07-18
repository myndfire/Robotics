#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <DisplayController.h>
#include <WiFi.h>

class StatusDashboard {
public:
    StatusDashboard(uint8_t sda, uint8_t scl,
                    uint8_t oledAddr = 0x3C, uint8_t oledHeight = 32);

    void begin();

    void setFirmwareVersion(const char* version);
    void setScrollSpeed(uint8_t line, uint16_t msPerStep);

private:
    DisplayController _display;

    TaskHandle_t _timerTaskHandle;
    TaskHandle_t _sensorTaskHandle;
    TaskHandle_t _statsTaskHandle;
    TaskHandle_t _displayTaskHandle;

    static constexpr uint8_t  TIMER_TASK_PRIORITY   = 1;
    static constexpr uint8_t  SENSOR_TASK_PRIORITY  = 1;
    static constexpr uint8_t  STATS_TASK_PRIORITY   = 1;
    static constexpr uint8_t  DISPLAY_TASK_PRIORITY = 1;
    static constexpr uint16_t TASK_STACK_SIZE        = 4096;

    unsigned long _uptimeSeconds;
    String        _firmwareVersion;

    static void _timerTaskEntry(void* pv);
    static void _sensorTaskEntry(void* pv);
    static void _statsTaskEntry(void* pv);
    static void _displayTaskEntry(void* pv);
    void _timerTask();
    void _sensorTask();
    void _statsTask();
    void _displayTask();
};
