# RelayController

Controls a single GPIO-driven relay or solenoid valve. Supports two operating modes: AUTO (relay driven by sensor state — ON when dry, OFF when wet) and MANUAL (relay fixed ON/OFF regardless of sensor readings, typically set via a web UI or serial command).

## What it contains

| File | Role |
|---|---|
| `src/RelayController.h` | Class declaration — constructor, auto/manual control, state query |
| `src/RelayController.cpp` | Implementation — pinMode, digitalWrite, control logic |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

No external dependencies — uses only the Arduino framework (`pinMode`, `digitalWrite`).

### Wiring

| Relay Module Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| IN (Control) | GPIO 4 | Signal pin — connect directly to ESP32 GPIO |
| VCC | 5V (if module uses 5V) or 3.3V | Check your relay module's voltage requirement |
| GND | GND | Common ground |

For solenoid valves or pumps, wire the device through the relay's normally-open (NO) and common (COM) terminals. Ensure the relay's contact rating exceeds the load current.

## Configuration

### Constructor

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `pin` | `uint8_t` | Any GPIO | _required_ | GPIO pin connected to the relay module's IN (control signal) pin. Most relay modules are active HIGH — setting the pin HIGH activates the relay, LOW deactivates it. |

### Operating Modes

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setManualOverride(enable)` | `bool` | true / false | false | Enable/disable manual override mode. When enabled, `autoControl()` is skipped and the relay stays at its last manually set level (via `turnOn()`/`turnOff()`). When disabled, `autoControl()` resumes normal sensor-driven operation. **The manual override flag is NOT persisted to NVS** — it resets to false on reboot, meaning the relay will be in AUTO mode and OFF on power-up unless `setManualOverride(true)` is called again in `setup()`. |

### State Queries

| Method | Returns | Description |
|---|---|---|
| `isOn()` | `bool` | Whether the relay is currently activated (HIGH). Returns the last state set by any method. |
| `isManualOverride()` | `bool` | Whether manual override mode is currently active. |

## Usage

```cpp
#include <RelayController.h>

RelayController relay(4);  // GPIO 4 controls the relay

void setup() {
    relay.begin();  // Set pin mode, ensure relay starts OFF
}

void loop() {
    // AUTO mode: sensor-driven
    bool soilIsDry = (moisturePercent < 25);
    relay.autoControl(soilIsDry);  // ON when dry, OFF when wet

    delay(2000);
}

// Example: manual override from serial command
void handleSerialCommand(String cmd) {
    if (cmd == "manual_on") {
        relay.setManualOverride(true);
        relay.turnOn();
    } else if (cmd == "manual_off") {
        relay.setManualOverride(true);
        relay.turnOff();
    } else if (cmd == "auto") {
        relay.setManualOverride(false);  // Resume sensor-driven control
    }
}
```

## Control Logic

- `turnOn()` — Sets relay ON (HIGH) regardless of override state. Intended for manual control scenarios.
- `turnOff()` — Sets relay OFF (LOW) regardless of override state.
- `autoControl(conditionDry)` — If manual override is enabled, this is a **no-op** (does nothing). If in AUTO mode, turns relay ON when `conditionDry` is true (sensor dry → need water), OFF when `conditionDry` is false (sensor wet → enough water).

```
autoControl(conditionDry):
  if manualOverride:  return (no-op)
  if conditionDry:    turnOn()
  else:               turnOff()
```

## Hardware

- Standard 1/2/4/8-channel relay module (5V or 3.3V logic compatible)
- Most relay modules are active HIGH — confirm with your module's datasheet
- Solenoid valves, pumps, or other inductive loads should use a flyback diode across the load terminals to protect the relay contacts

## Expected Behavior

- `begin()` configures the GPIO as OUTPUT and writes LOW — the relay starts **OFF**. This is important: the relay will not activate on power-up.
- **Serial output** (if logging is added): `Relay ON` / `Relay OFF` on state transitions.
- **Manual override caveat**: If manual override is enabled and the board reboots (power loss, watchdog reset), the override flag resets to `false`. The relay will start OFF and in AUTO mode. If your application needs persistent manual state, store the override flag in NVS using `StorageController` and restore it in `setup()`.
- **autoControl in AUTO mode**: Relay clicks ON when `conditionDry` is true, OFF when false. No software debouncing — the hardware relay has mechanical debounce via its own switching time (~5–10ms).
- **Relay switching sound**: An audible click when the relay changes state. This is normal mechanical relay behavior. Solid-state relays (SSRs) are silent but require different wiring.
