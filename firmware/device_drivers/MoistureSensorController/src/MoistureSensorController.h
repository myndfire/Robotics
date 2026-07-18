/*
 * MoistureSensorController.h — Self-contained LM393 soil moisture sensor
 *
 * Owns both the ADC hardware layer and all NVS-persisted calibration data:
 *   dryRef / wetRef           — ADC endpoints for 0% / 100% mapping
 *   dryTriggerPct / wetTriggerPct — hysteresis thresholds for relay control
 *
 * All settings are loaded from NVS on begin() and written back on every
 * setter call.  The internal mutex protects the read-validate-write
 * sequence in setters so they are safe from the web task.  Getters return
 * atomic ints — no lock needed for reads.
 */
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <StorageController.h>

class MoistureSensorController {
public:
  static constexpr int DEFAULT_DRY_REF         = 3000;
  static constexpr int DEFAULT_WET_REF         = 800;
  static constexpr int DEFAULT_DRY_TRIGGER_PCT = 25;
  static constexpr int DEFAULT_WET_TRIGGER_PCT = 35;

  MoistureSensorController(uint8_t aoPin);

  void begin(StorageController& storage);
  void read();

  int  analogValue() const           { return _analog; }
  int  moisturePercent() const;

  int  dryRef()         const { return _dryRef; }
  int  wetRef()         const { return _wetRef; }
  int  dryTriggerPct()  const { return _dryTriggerPct; }
  int  wetTriggerPct()  const { return _wetTriggerPct; }

  bool setDryRef(int value);
  bool setWetRef(int value);
  bool setDryTriggerPct(int value);
  bool setWetTriggerPct(int value);

private:
  uint8_t _aoPin;
  int     _analog;

  StorageController*          _storage;
  SemaphoreHandle_t _mutex;

  int _dryRef;
  int _wetRef;
  int _dryTriggerPct;
  int _wetTriggerPct;
};
