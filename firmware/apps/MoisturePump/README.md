# MoisturePump

Integration test composing `MoistureMonitor` and `TimedCycleController` into a complete automated irrigation system.

## What it composes

| Service | From | Role |
|---|---|---|
| `MoistureMonitor` | `firmware/firmware/services/MoistureMonitor/` | Reads LM393 soil moisture, displays DRY/WET/OK status on OLED |
| `TimedCycleController` | `firmware/firmware/services/TimedCycleController/` | Controls relay-driven pump with configurable ON/OFF duty cycle |

## Setup

```bash
cd firmware/apps/MoisturePump
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

## Wiring

| Signal | ESP32-S3 Pin |
|---|---|
| LM393 AO | GPIO 1 |
| Relay IN | GPIO 4 |
| OLED SDA | GPIO 41 |
| OLED SCL | GPIO 42 |

## Expected Behavior

- **MoistureMonitor** reads the soil sensor every 5 seconds and updates the OLED with moisture %, raw ADC, calibration references, and DRY/WET/OK status.
- **TimedCycleController** runs the pump on a configurable duty cycle (default 3s ON / 7s OFF) independent of moisture state.
- The `loop()` ties them together: prints status every 10 seconds, passes serial commands to the pump controller.
- **Serial output** (921600 baud): moisture readings + pump state every 10 seconds.

```
MoisturePump — Automated Irrigation
  Moisture monitor: GPIO 1, relay pump: GPIO 4
  Reading every 5s, pump activates when DRY
[Moisture: 25% DRY] [Pump: ON, 3s remaining]
[Moisture: 65% OK] [Pump: OFF, 5s remaining]
```

## Serial Commands

Passthrough to `TimedCycleController`:

| Command | Action |
|---|---|
| `on=N` | Set pump ON duration (1-3600s) |
| `off=N` | Set pump OFF duration (1-3600s) |
| `s` | Show current duty cycle settings |

## External Consumer Pattern

This test's `platformio.ini` demonstrates the standard external-consumer import:

```ini
lib_extra_dirs =
    ../lib       # Hardware drivers
    ../services          # Service abstractions
```

No code from either library directory is duplicated — only imported.
