# MoistureSensorController

Self-contained LM393 analog soil moisture sensor driver. Owns both the ADC hardware layer and all NVS-persisted calibration data. Provides two-point calibration (dry reference and wet reference ADC values), maps raw ADC to moisture percentage (0–100%), and supports hysteresis thresholds for relay/pump control.

## What it contains

| File | Role |
|---|---|
| `src/MoistureSensorController.h` | Class declaration — constants, constructor, read/percentage methods, calibration setters with mutex |
| `src/MoistureSensorController.cpp` | Implementation — ADC read, percentage mapping, NVS load/save, validation |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

### Dependencies

| Dependency | Source |
|---|---|
| `StorageController` | Internal (same Drivers folder) |
| FreeRTOS (`semphr.h` — mutex) | Bundled with Arduino-ESP32 framework |

### Wiring

| LM393 Sensor Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| VCC | 3.3V or 5V | — |
| GND | GND | — |
| AO (Analog Out) | GPIO 1 (ADC1) | Analog signal proportional to moisture — higher voltage = drier |
| DO (Digital Out) | Not used | Digital threshold comparator output — ignore |

The sensor typically outputs a voltage between 0V and 3.3V inversely proportional to moisture: **high voltage = dry, low voltage = wet**. The LM393 comparator also provides a digital output (DO) with adjustable threshold via the on-board potentiometer — this library uses only the analog output (AO).

## Configuration

### Constructor

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `aoPin` | `uint8_t` | ADC1: 1–10, ADC2: 11–20 | _required_ | GPIO pin connected to the LM393 AO output. Must be an ADC-capable pin. Use ADC1 pins (1–10) if your application also uses WiFi. |

### Calibration Constants

| Constant | Value | Description |
|---|---|---|
| `DEFAULT_DRY_REF` | 3000 | Default ADC reading for completely dry soil (0% moisture) |
| `DEFAULT_WET_REF` | 800 | Default ADC reading for fully wet soil (100% moisture) |
| `DEFAULT_DRY_TRIGGER_PCT` | 25 | Default moisture % below which the soil is considered "dry" (triggers pump ON) |
| `DEFAULT_WET_TRIGGER_PCT` | 35 | Default moisture % above which the soil is considered "wet" (triggers pump OFF) |

### Calibration Parameters (persisted to NVS)

| Option | Type | Range | Default | Validation | Description |
|---|---|---|---|---|---|
| `setDryRef(value)` | `int` | 0 – 4095 (12-bit ADC) | 3000 | Must be > `wetRef` | ADC reading when the sensor is in completely dry air (open air, sensor not touching anything). This is the upper endpoint — maps to 0% moisture. A typical dry sensor in air reads ~2500–3200 on a 12-bit ADC. |
| `setWetRef(value)` | `int` | 0 – 4095 (12-bit ADC) | 800 | Must be < `dryRef` | ADC reading when the sensor is fully submerged in water. This is the lower endpoint — maps to 100% moisture. A typical wet sensor in water reads ~600–1200 on a 12-bit ADC. |
| `setDryTriggerPct(value)` | `int` | 0 – 100 | 25 | Must be < `wetTriggerPct` | Moisture percentage below which the soil is considered "dry" and needs watering. When `moisturePercent()` falls below this value, the pump/relay should turn ON. This is the lower hysteresis threshold. |
| `setWetTriggerPct(value)` | `int` | 0 – 100 | 35 | Must be > `dryTriggerPct` | Moisture percentage above which the soil is considered "wet enough". When `moisturePercent()` rises above this value, the pump/relay should turn OFF. This is the upper hysteresis threshold. The gap between dry and wet triggers (e.g., 25%–35%) prevents rapid on/off cycling. |

### Getters

| Method | Returns | Description |
|---|---|---|
| `analogValue()` | `int` | Most recent raw ADC reading (0–4095 at 12-bit). Updated only when `read()` is called. |
| `moisturePercent()` | `int` | Moisture mapped to 0–100% using `dryRef` and `wetRef` as endpoints. Constrained to 0–100 range. 0% = bone dry (at or above `dryRef`), 100% = fully wet (at or below `wetRef`). Linear interpolation between endpoints. |
| `dryRef()` | `int` | Current dry reference ADC value (from NVS or default) |
| `wetRef()` | `int` | Current wet reference ADC value (from NVS or default) |
| `dryTriggerPct()` | `int` | Current dry trigger threshold percentage (from NVS or default) |
| `wetTriggerPct()` | `int` | Current wet trigger threshold percentage (from NVS or default) |

## Usage

```cpp
#include <MoistureSensorController.h>
#include <StorageController.h>

StorageController store;
MoistureSensorController sensor(1);  // AO pin on GPIO 1

void setup() {
    Serial.begin(921600);
    store.begin("moisture");   // NVS namespace for calibration data
    sensor.begin(store);       // Loads calibration from NVS (or uses defaults)
}

void loop() {
    sensor.read();
    int raw = sensor.analogValue();
    int pct = sensor.moisturePercent();

    Serial.printf("ADC=%d  Moisture=%d%%\n", raw, pct);

    // Hysteresis pump control
    if (pct < sensor.dryTriggerPct()) {
        // Too dry — turn pump ON
    } else if (pct > sensor.wetTriggerPct()) {
        // Wet enough — turn pump OFF
    }

    delay(2000);
}
```

## Calibration Workflow

1. Place sensor in open air (completely dry). Call `read()` and note the ADC value. Call `setDryRef(value)` to store it. This maps to 0% moisture.
2. Submerge sensor in water (completely wet). Call `read()` and note the ADC value. Call `setWetRef(value)` to store it. This maps to 100% moisture.
3. Set trigger thresholds: `setDryTriggerPct(25)` (pump ON below 25%) and `setWetTriggerPct(35)` (pump OFF above 35%).
4. All values are immediately persisted to NVS and survive power cycles.

**Validation rules**: `setDryRef()` rejects values ≤ the current `wetRef`. `setWetRef()` rejects values ≥ the current `dryRef`. `setDryTriggerPct()` rejects values ≥ `wetTriggerPct`. `setWetTriggerPct()` rejects values ≤ `dryTriggerPct`. All setters return `false` on validation failure and do not modify the stored value.

## Hardware

- LM393-based soil moisture sensor (analog output)
- Corrosion-resistant probes recommended for long-term soil contact
- The sensor uses resistive measurement — probe current flow causes electrolysis over time. For permanent installations, consider powering the sensor only during readings (via a GPIO-controlled transistor) to extend probe life.

## Expected Behavior

- `begin(storage)` loads calibration from NVS namespace `"moisture"`. If no calibration exists, defaults are used and `moisturePercent()` returns 0% (dry).
- `read()` performs a single ADC conversion and updates `analogValue()`. Does not update `moisturePercent()` separately — call `moisturePercent()` after `read()` to get the updated percentage.
- **Percentage mapping**: `pct = map(adc, dryRef, wetRef, 0, 100)`, constrained to 0–100. Since higher ADC = drier, the mapping is inverted: ADC at `dryRef` (high value) → 0%, ADC at `wetRef` (low value) → 100%.
- **Default calibration** (no NVS data): `dryRef=3000, wetRef=800`. At these defaults, an ADC reading of 1900 (midpoint) maps to 50% moisture. A typical wet sensor reads ~800–1200, dry ~2500–3200.
- **Mutex behavior**: Setters acquire a mutex during the validate-and-write sequence, making them safe to call from multiple FreeRTOS tasks (e.g., a web server task and a sensor task).
- Getters (`analogValue()`, `moisturePercent()`, `dryRef()`, etc.) are lock-free — they read atomic ints and are safe from any context.
- **Serial output** at 921600 baud: `ADC=2047  Moisture=52%`. Dry soil: `ADC=3100  Moisture=0%`. Wet soil/sensor in water: `ADC=750  Moisture=100%`.
- **Hysteresis example**: With `dryTriggerPct=25` and `wetTriggerPct=35`: Moisture dropping below 25% triggers watering → pump runs → moisture rises → pump stops at 35%. The 10% gap prevents rapid on/off oscillation.
