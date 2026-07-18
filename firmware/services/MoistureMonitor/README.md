# MoistureMonitor

Soil moisture monitoring service with DRY/WET/OK status display. Composes `MoistureSensorController` + `DisplayController` + `StorageController` (NVS calibration). Runs FreeRTOS sensor and display tasks.

## Dependencies

| Library | Source |
|---|---|
| MoistureSensorController | `firmware/device_drivers/` |
| DisplayController | `firmware/device_drivers/` |
| StorageController | `firmware/device_drivers/` |
| U8g2 | External (`olikraus/U8g2`) |

## Setup

```bash
cd services/MoistureMonitor
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
| LM393 AO | GPIO 1 |
| OLED SDA | GPIO 41 |
| OLED SCL | GPIO 42 |

## API

```cpp
#include <MoistureMonitor.h>

MoistureMonitor monitor(1, 41, 42);  // sensorPin, SDA, SCL

void setup() {
    StorageController storage;
    storage.begin("moisture");
    monitor.begin(storage);
}
```

### Methods

| Method | Returns | Description |
|---|---|---|
| `begin(StorageController&)` | void | Initialize sensor, display, NVS calibration, start FreeRTOS tasks |
| `getMoisturePercent()` | int | 0-100% moisture (uses NVS-persisted calibration) |
| `getAnalogValue()` | int | Raw ADC value (0-4095 at 12-bit) |
| `isDry()` | bool | True when moisture < dryTriggerPct |
| `isWet()` | bool | True when moisture > wetTriggerPct |
| `getStatus()` | const char* | "DRY", "WET", or "OK " |

### FreeRTOS Tasks

- **sensor task** (Core 1, 5s interval): reads sensor, updates OLED lines, prints to serial
- **display task** (Core 1): drains render queue

## Expected Behavior

### OLED (128×32, updates every 5s)

```
Line 0: Moist: 25% DRY
Line 1: ADC: 3100
Line 2: Ref: D=3100 W=750
Line 3: Trig: dry<25% wet>35%
```

### Serial (921600 baud)

```
ADC=3100  Moisture=25%  Status=DRY
ADC=1500  Moisture=65%  Status=OK
ADC=700   Moisture=80%  Status=WET
```

Calibration from NVS (or defaults: dryRef=3000, wetRef=800, dryTrigger=25%, wetTrigger=35%).
