/*
 * RelayController.cpp
 *
 * See RelayController.h for the public API description.
 */

#include "RelayController.h"

RelayController::RelayController(uint8_t pin)
  : _pin(pin), _on(false), _manualOverride(false)
{}

void RelayController::begin() {
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);   // Relay OFF on boot
}

void RelayController::turnOn() {
  digitalWrite(_pin, HIGH);
  _on = true;
}

void RelayController::turnOff() {
  digitalWrite(_pin, LOW);
  _on = false;
}

void RelayController::autoControl(bool conditionDry) {
  // If the user has manually overridden the relay, do nothing.
  if (_manualOverride) return;

  // When the sensor reports dry soil, activate the pump / solenoid.
  if (conditionDry) turnOn();
  else              turnOff();
}

void RelayController::setManualOverride(bool enable) {
  _manualOverride = enable;
}
