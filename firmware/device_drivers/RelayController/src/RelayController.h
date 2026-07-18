/*
 * RelayController.h — Controls a single GPIO-driven relay or solenoid
 *
 * Supports two operating modes:
 *   AUTO   — the relay is driven by sensor state (ON when dry, OFF when wet)
 *   MANUAL — the relay is fixed ON or OFF regardless of sensor readings,
 *            typically set via the web UI
 *
 * The manual-override flag is stored internally so the agent can query it
 * and display the current mode on the dashboard.
 */

#pragma once

#include <Arduino.h>

class RelayController {
public:
  // pin — GPIO number wired to the relay module's IN pin
  RelayController(uint8_t pin);

  // Set pin mode, ensure relay starts OFF.
  void begin();

  // Direct control (ignores override flag — callers should set it first).
  void turnOn();
  void turnOff();

  // Automatic control.  If manual override is active this is a no-op.
  // conditionDry — pass true when the sensor reports "dry".
  void autoControl(bool conditionDry);

  // Enable / disable manual override.  When enabled, autoControl() is
  // skipped and the relay stays at the last manually-set level.
  void setManualOverride(bool enable);

  // Query current state.
  bool isOn()             const { return _on; }
  bool isManualOverride() const { return _manualOverride; }

private:
  uint8_t _pin;
  bool    _on;
  bool    _manualOverride;
};
