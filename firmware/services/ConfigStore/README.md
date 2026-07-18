# ConfigStore

Typed NVS configuration manager for device settings. Composes `StorageController` + `DisplayController`. Provides serial command interface and OLED live summary for String, int, bool, and float keys. All values persist across power cycles.

## Dependencies

| Library | Source |
|---|---|
| StorageController | `firmware/device_drivers/` |
| DisplayController | `firmware/device_drivers/` |
| U8g2 | External (`olikraus/U8g2`) |

## Setup

```bash
cd services/ConfigStore
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

| OLED Pin | ESP32-S3 Pin |
|---|---|
| SDA | GPIO 41 |
| SCL | GPIO 42 |

No other hardware — NVS is internal flash.

## API

```cpp
#include <ConfigStore.h>

ConfigStore config(41, 42);  // SDA, SCL

void setup() {
    config.begin("config");  // NVS namespace
}
```

### Methods

| Method | Returns | Description |
|---|---|---|
| `begin(ns)` | void | Open NVS namespace, load config, display on OLED |
| `getDeviceName()` | String | Device identifier (default: "ESP32-S3") |
| `getInterval()` | int | Sample interval in seconds (default: 5) |
| `isEnabled()` | bool | System enabled flag (default: true) |
| `getGain()` | float | Sensor gain multiplier (default: 1.0) |
| `setDeviceName(name)` | bool | Set device name |
| `setInterval(seconds)` | bool | Set interval (must be > 0) |
| `setEnabled(en)` | bool | Set enabled flag |
| `setGain(value)` | bool | Set gain multiplier |
| `resetToDefaults()` | void | Clear NVS, restore defaults |
| `handleSerial()` | void | Process serial commands (call in loop()) |

### Serial Commands

| Command | Action |
|---|---|
| `name <str>` | Set device name |
| `intv <n>` | Set interval seconds |
| `enab <0\|1>` | Enable/disable |
| `gain <n.n>` | Set gain |
| `show` | Display all values |
| `list` | List stored keys |
| `clear` | Reset to defaults |

## Expected Behavior

### OLED (128×32)

```
Line 0: Name: ESP32-S3
Line 1: Intv:5s  Gain:1.0
Line 2: Enabled: yes
Line 3: Free: 120
```

### Serial (921600 baud)

```
──── Config Store Commands ────
  Namespace: config
  name str     set device name (string)
  intv N       set interval seconds (int)
  enab 0|1     enable / disable (bool)
  gain N.N     set gain multiplier (float)
  show         display all values
  list         list stored keys
  clear        reset to defaults
──────────────────────────────────
```
