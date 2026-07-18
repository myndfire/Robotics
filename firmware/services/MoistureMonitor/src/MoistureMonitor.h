#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <DisplayController.h>
#include <MoistureSensorController.h>
#include <StorageController.h>

class MoistureMonitor {
public:
    MoistureMonitor(uint8_t sensorPin, uint8_t sda, uint8_t scl,
                    uint8_t oledAddr = 0x3C, uint8_t oledHeight = 32);

    void begin(StorageController& storage);

    int getMoisturePercent() const;
    int getAnalogValue() const;
    bool isDry() const;
    bool isWet() const;
    const char* getStatus() const;

private:
    MoistureSensorController _sensor;
    DisplayController       _display;
    StorageController*      _storage;

    TaskHandle_t _sensorTaskHandle;
    TaskHandle_t _displayTaskHandle;

    static constexpr uint8_t  SENSOR_TASK_PRIORITY  = 1;
    static constexpr uint8_t  DISPLAY_TASK_PRIORITY = 1;
    static constexpr uint16_t TASK_STACK_SIZE        = 4096;
    static constexpr uint16_t READ_INTERVAL_MS       = 5000;

    int  _analogValue;
    int  _moisturePercent;
    bool _dry;
    bool _wet;

    static void _sensorTaskEntry(void* pv);
    static void _displayTaskEntry(void* pv);
    void _sensorTask();
    void _displayTask();
};
