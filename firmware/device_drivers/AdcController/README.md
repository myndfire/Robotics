# AdcController

General-purpose ESP32-S3 ADC wrapper. Each instance owns one GPIO pin and provides configurable resolution, per-pin attenuation, e-fuse-calibrated millivolt reads, and multi-sample averaging. No external dependencies beyond the Arduino framework.

## What it contains

| File | Role |
|---|---|
| `src/AdcController.h` | Class declaration with full API documentation |
| `src/AdcController.cpp` | Implementation — analogRead, analogReadMilliVolts, averaging |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
```

No external dependencies needed.

### Wiring

Connect an analog signal source (potentiometer, sensor, voltage divider) to an ADC-capable GPIO:

| ADC Unit | GPIOs | Notes |
|---|---|---|
| ADC1 | 1 – 10 | Always available. GPIOs 3, 4 may be strapping pins. |
| ADC2 | 11 – 20 | **Unavailable when WiFi is active.** If your application uses WiFi, use ADC1 pins only. |

## Configuration

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `pin` (constructor) | `uint8_t` | 1 – 20 (ADC-capable) | _required_ | GPIO number for the analog input. Must be on ADC1 (1–10) or ADC2 (11–20). |
| `DEFAULT_RESOLUTION` | `constexpr uint8_t` | 12 | 12 | Default ADC resolution in bits. Used by `begin()`. |
| `DEFAULT_ATTENUATION` | `constexpr uint16_t` | `ADC_0db` / `ADC_2_5db` / `ADC_6db` / `ADC_11db` | `ADC_11db` | Default input attenuation. Used by `begin()`. |
| `setResolution(bits)` | `uint8_t` | 9 – 12 | 12 | **Global setting.** Affects ALL ADC reads on ALL pins. 12-bit → 0–4095, 11-bit → 0–2047, 10-bit → 0–1023, 9-bit → 0–511. Values outside 9–12 are accepted by the Arduino API but results will be shifted. Calling this on any instance changes the entire ADC peripheral. |
| `setAttenuation(atten)` | `adc_attenuation_t` | `ADC_0db` (0–1.1V), `ADC_2_5db` (0–1.5V), `ADC_6db` (0–2.2V), `ADC_11db` (0–3.3V) | `ADC_11db` | **Per-pin setting.** Different pins can have different attenuation levels simultaneously. Higher attenuation = wider voltage range but more noise. `ADC_0db` gives the lowest noise for low-voltage signals. |

### Getters

| Method | Returns | Description |
|---|---|---|
| `getValue()` | `int` | Last read value (0 if no conversion performed) |
| `getPin()` | `uint8_t` | GPIO pin number from constructor |
| `getAttenuation()` | `adc_attenuation_t` | Current attenuation for this pin |
| `getResolution()` | `uint8_t` | Current global ADC resolution |

### Operations

| Method | Returns | Description |
|---|---|---|
| `read()` | `int` | Single ADC conversion. Range depends on resolution (0–4095 at 12-bit). |
| `readMilliVolts()` | `uint32_t` | Single conversion using factory e-fuse calibration. Returns millivolts (e.g., 1650 = 1.650 V). Calibration curve is created on first call and cached for this ADC unit. More accurate than raw × Vref / 2^bits. |
| `readAveraged(n=8)` | `int` | Performs `n` conversions and returns the arithmetic mean. Higher `n` reduces noise at the cost of ~1 ms per sample. Must be ≥ 1. |

## Usage

```cpp
#include <AdcController.h>

AdcController adc(1);  // GPIO 1 (ADC1)

void setup() {
    adc.begin();            // 12-bit, ADC_11db, discard read
    Serial.begin(921600);
}

void loop() {
    int raw      = adc.read();             // single conversion
    uint32_t mV  = adc.readMilliVolts();   // e-fuse calibrated
    int avg      = adc.readAveraged(16);   // 16-sample average

    Serial.printf("raw=%d  mV=%u  avg=%d\n", raw, mV, avg);
    delay(1000);
}
```

## Hardware

- **ESP32-S3** with any ADC-capable GPIO (1–20)
- Input voltage must not exceed 3.3V. Use a voltage divider for higher-voltage signals.

## Expected Behavior

- `begin()` performs a discard read — the first `read()` after `begin()` returns a valid, settled value.
- `read()` blocks for one ADC conversion cycle (~micoseconds).
- `readMilliVolts()` returns a more accurate voltage than manually computing `raw × 3300 / 4096`, because it compensates for per-chip manufacturing variance using factory calibration data.
- `readAveraged(16)` takes approximately 16× the time of a single `read()` — roughly 1 ms for 16 samples at 12-bit resolution.
- Output range for `read()` at 12-bit: 0–4095. At `ADC_11db`, 4095 ≈ 3.3V.
- Serial output at 921600 baud: `raw=2047  mV=1650  avg=2043.5`.
- **Critical**: Calling `setResolution()` on any `AdcController` instance changes resolution for all ADC reads system-wide.
