# MultiSensor

Integration test composing 3 `AdcController` instances with `DisplayController` for a multi-channel analog sensor monitor with scrolling OLED display.

## What it composes

| Component | From | Role |
|---|---|---|
| `AdcController` (×3) | `firmware/device_drivers/AdcController/` | Reads analog channels on GPIO 1, 2, 3 (ADC1) |
| `DisplayController` | `firmware/device_drivers/DisplayController/` | SSD1306 OLED with scrolling text |

## Setup

```bash
cd firmware/apps/MultiSensor
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

## Wiring

| Signal | ESP32-S3 Pin |
|---|---|
| Analog sensor 0 (e.g., light) | GPIO 1 (ADC1_CH0) |
| Analog sensor 1 (e.g., potentiometer) | GPIO 2 (ADC1_CH1) |
| Analog sensor 2 (e.g., aux) | GPIO 3 (ADC1_CH2) |
| OLED SDA | GPIO 41 |
| OLED SCL | GPIO 42 |

All 3 pins are on ADC1 — still available when WiFi is active.

## Expected Behavior

### OLED Display (128×32, scrolling)

```
Line 0 (scroll 200ms): CH0:30% CH1:50% CH2:80%
Line 1: R0:1234 R1:2048 R2:3276
Line 2: mV0:995mV mV1:1650mV mV2:2640mV
Line 3: ADC1:3ch 12bit 11dB
```

Updates every 1 second via FreeRTOS sensor task. Line 0 scrolls horizontally for longer text.

### Serial Output (921600 baud)

```
MultiSensor — 3-channel ADC monitor
  CH0: GPIO1 | CH1: GPIO2 | CH2: GPIO3
  OLED: SDA=41 SCL=42 | updates every 1s
```

Raw values, percentages, and calibrated millivolt readings for all 3 channels.

## External Consumer Pattern

This test imports drivers directly from `firmware/device_drivers/`:

```ini
lib_extra_dirs = ../lib
```

3 independent `AdcController` instances share one `DisplayController`. The FreeRTOS sensor task reads all three channels in sequence and updates the display with thread-safe `setLine()` calls.
