# StatusDashboard

Multi-task system status dashboard with scrolling OLED display. Composes `DisplayController` + `WiFi` for RSSI. Runs 4 FreeRTOS tasks (timer, sensor, stats, display) with per-line horizontal marquee scrolling.

## Dependencies

| Library | Source |
|---|---|
| DisplayController | `firmware/device_drivers/` |
| U8g2 | External (`olikraus/U8g2`) |

## Setup

```bash
cd services/StatusDashboard
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

No other hardware — WiFi initialized in STA mode for RSSI.

## API

```cpp
#include <StatusDashboard.h>

StatusDashboard dashboard(41, 42);  // SDA, SCL

void setup() {
    dashboard.setFirmwareVersion("MyApp v2.0");
    dashboard.begin();  // Starts 4 FreeRTOS tasks
}
```

### Methods

| Method | Description |
|---|---|
| `begin()` | Initialize display + WiFi STA, enable scrolling, start FreeRTOS tasks |
| `setFirmwareVersion(version)` | Set the static version string on line 3 (default: "v1.0.0") |
| `setScrollSpeed(line, msPerStep)` | Override scroll speed for a line (defaults: 200/250/300ms) |

### FreeRTOS Tasks

| Task | Core | Interval | Output |
|---|---|---|---|
| timer task | 0 | 1s | Line 0 (scroll 200ms): Uptime + task count |
| sensor task | 1 | 7s | Line 1 (scroll 250ms): Simulated sensor reading |
| stats task | 1 | 5s | Line 2 (scroll 300ms): Heap, PSRAM, RSSI, SDK version |
| display task | 1 | continuous | Drains render queue, advances scroll animation |

## Expected Behavior

### OLED (128×32, scrolling)

```
Line 0 (scroll 200ms): Uptime: 0h 5m 30s | Tasks: 6
Line 1 (scroll 250ms): Sensor: 42% | Ref: D=3100 W=750 | Trig: dry<25% wet>35%
Line 2 (scroll 300ms): Heap: 250000/400000 | PSRAM: 8388608 | RSSI: -65 dBm | SDK: v5.1.4
Line 3 (static):       v1.0.0
```

Line 0 uses `u8g2_font_5x8_tf` (taller font). Lines 1-3 use default `u8g2_font_5x7_tf`.

### Serial (921600 baud)

```
StatusDashboard service demo — 4 tasks, 3 scrolling lines
  Line 0: u8g2_font_5x8_tf, lines 1-3: default u8g2_font_5x7_tf
```
