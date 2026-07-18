# TimedCycleController

Duty-cycle pump/solenoid/valve controller. Composes `RelayController` + `DisplayController` + `StorageController` (NVS). Configurable ON/OFF durations persisted to NVS, live OLED countdown timers, and serial command interface.

## Dependencies

| Library | Source |
|---|---|
| RelayController | `firmware/device_drivers/` |
| DisplayController | `firmware/device_drivers/` |
| StorageController | `firmware/device_drivers/` |
| U8g2 | External (`olikraus/U8g2`) |

## Setup

```bash
cd services/TimedCycleController
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

External project:

```ini
lib_extra_dirs =
    /path/to/Robotics/firmware/device_drivers
    /path/to/Robotics/firmware/services
```

## Wiring

| Signal | ESP32-S3 Pin |
|---|---|
| Relay IN | GPIO 4 |
| OLED SDA | GPIO 41 |
| OLED SCL | GPIO 42 |

## API

```cpp
#include <TimedCycleController.h>

TimedCycleController controller(4, 41, 42);  // relayPin, SDA, SCL

void setup() {
    StorageController storage;
    controller.begin(storage);
}
```

### Methods

| Method | Returns | Description |
|---|---|---|
| `begin(StorageController&)` | void | Initialize relay, display, load NVS durations, start FreeRTOS tasks |
| `setOnDuration(seconds)` | bool | Set ON duration (1-3600s). Persisted to NVS. |
| `setOffDuration(seconds)` | bool | Set OFF duration (1-3600s). Persisted to NVS. |
| `getOnDuration()` | int | Current ON duration |
| `getOffDuration()` | int | Current OFF duration |
| `isRelayOn()` | bool | Current relay state |
| `getPhaseRemaining()` | int | Seconds remaining in current phase |
| `getCycleCount()` | unsigned long | Total completed cycles |
| `handleSerial()` | void | Process serial commands (call in loop()) |

### Serial Commands

| Command | Action |
|---|---|
| `on=N` | Set ON duration (1-3600s) |
| `off=N` | Set OFF duration (1-3600s) |
| `s` | Show current settings |

### FreeRTOS Tasks

- **relay task** (Core 1): cycles relay ON/OFF with configurable durations
- **display task** (Core 1): OLED countdown timers, cycle count

## Expected Behavior

### OLED (128×32)

```
Line 0: Relay: ON
Line 1: Phase: 3s
Line 2: ON dur:3s OFF:7s
Line 3: Cycles: 12 (0h2m0s)
```

### Serial (921600 baud)

```
Cycle: ON=3s OFF=7s
Commands: on=N  off=N  s (status)
```
