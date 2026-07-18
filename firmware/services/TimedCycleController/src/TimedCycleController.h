#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <RelayController.h>
#include <DisplayController.h>
#include <StorageController.h>

class TimedCycleController {
public:
    TimedCycleController(uint8_t relayPin, uint8_t sda, uint8_t scl,
                         uint8_t oledAddr = 0x3C, uint8_t oledHeight = 32);

    void begin(StorageController& storage);

    bool setOnDuration(int seconds);
    bool setOffDuration(int seconds);
    int  getOnDuration() const;
    int  getOffDuration() const;
    bool isRelayOn() const;
    int  getPhaseRemaining() const;
    unsigned long getCycleCount() const;

    void handleSerial();

private:
    RelayController  _relay;
    DisplayController _display;
    StorageController* _storage;

    TaskHandle_t _relayTaskHandle;
    TaskHandle_t _displayTaskHandle;

    static constexpr uint8_t  RELAY_TASK_PRIORITY   = 1;
    static constexpr uint8_t  DISPLAY_TASK_PRIORITY = 1;
    static constexpr uint16_t TASK_STACK_SIZE        = 4096;

    static constexpr int DEFAULT_ON_SECS  = 3;
    static constexpr int DEFAULT_OFF_SECS = 7;

    int  _onDurationSecs;
    int  _offDurationSecs;
    int  _phaseRemaining;
    bool _relayOnPhase;
    unsigned long _cycleCount;

    static void _relayTaskEntry(void* pv);
    static void _displayTaskEntry(void* pv);
    void _relayTask();
    void _displayTask();
    void _loadDurations();
    void _saveDurations();
};
