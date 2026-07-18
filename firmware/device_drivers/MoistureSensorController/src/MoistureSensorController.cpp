/*
 * MoistureSensorController.cpp
 *
 * See MoistureSensorController.h for the public API description.
 */
#include "MoistureSensorController.h"

#define MUTEX_TIMEOUT_MS 100

MoistureSensorController::MoistureSensorController(uint8_t aoPin)
  : _aoPin(aoPin), _analog(0)
  , _storage(nullptr), _mutex(nullptr)
  , _dryRef(DEFAULT_DRY_REF), _wetRef(DEFAULT_WET_REF)
  , _dryTriggerPct(DEFAULT_DRY_TRIGGER_PCT), _wetTriggerPct(DEFAULT_WET_TRIGGER_PCT)
{}

void MoistureSensorController::begin(StorageController& storage) {
  _storage = &storage;
  _mutex   = xSemaphoreCreateMutex();

  _storage->begin("moisture");
  _dryRef         = _storage->get<int>("dryRef",         DEFAULT_DRY_REF);
  _wetRef         = _storage->get<int>("wetRef",         DEFAULT_WET_REF);
  _dryTriggerPct  = _storage->get<int>("dryTriggerPct",  DEFAULT_DRY_TRIGGER_PCT);
  _wetTriggerPct  = _storage->get<int>("wetTriggerPct",  DEFAULT_WET_TRIGGER_PCT);
  _storage->end();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogRead(_aoPin);   // discard first reading for ADC settling

  Serial.printf("Calibration loaded: dry_ref=%d, wet_ref=%d\n", _dryRef, _wetRef);
  Serial.printf("Triggers: dry=%d%%, wet=%d%%\n", _dryTriggerPct, _wetTriggerPct);
}

void MoistureSensorController::read() {
  _analog = analogRead(_aoPin);
}

int MoistureSensorController::moisturePercent() const {
  int pct = map(_analog, _dryRef, _wetRef, 0, 100);
  return constrain(pct, 0, 100);
}

bool MoistureSensorController::setDryRef(int value) {
  if (!_mutex) return false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return false;
  if (value < 0 || value > 4095 || value <= _wetRef) { xSemaphoreGive(_mutex); return false; }
  _dryRef = value;
  _storage->begin("moisture");
  _storage->put<int>("dryRef", value);
  _storage->end();
  xSemaphoreGive(_mutex);
  return true;
}

bool MoistureSensorController::setWetRef(int value) {
  if (!_mutex) return false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return false;
  if (value < 0 || value > 4095 || value >= _dryRef) { xSemaphoreGive(_mutex); return false; }
  _wetRef = value;
  _storage->begin("moisture");
  _storage->put<int>("wetRef", value);
  _storage->end();
  xSemaphoreGive(_mutex);
  return true;
}

bool MoistureSensorController::setDryTriggerPct(int value) {
  if (!_mutex) return false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return false;
  if (value < 0 || value >= _wetTriggerPct || value > 100) { xSemaphoreGive(_mutex); return false; }
  _dryTriggerPct = value;
  _storage->begin("moisture");
  _storage->put<int>("dryTriggerPct", value);
  _storage->end();
  xSemaphoreGive(_mutex);
  return true;
}

bool MoistureSensorController::setWetTriggerPct(int value) {
  if (!_mutex) return false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return false;
  if (value <= _dryTriggerPct || value > 100) { xSemaphoreGive(_mutex); return false; }
  _wetTriggerPct = value;
  _storage->begin("moisture");
  _storage->put<int>("wetTriggerPct", value);
  _storage->end();
  xSemaphoreGive(_mutex);
  return true;
}
