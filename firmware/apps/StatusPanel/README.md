# StatusPanel

Integration test composing `StatusDashboard` and `LedRGBController` into a system health monitoring panel with visual + display feedback.

## What it composes

| Component | From | Role |
|---|---|---|
| `StatusDashboard` | `firmware/firmware/services/StatusDashboard/` | Scrolling OLED dashboard: uptime, heap, PSRAM, WiFi RSSI, SDK version |
| `LedRGBController` | `firmware/device_drivers/LedRGBController/` | On-board NeoPixel color-coded by free heap |

## Setup

```bash
cd firmware/apps/StatusPanel
uv run pio run --target upload
uv run pio run --target upload --target monitor
```

No external wiring — uses on-board NeoPixel (GPIO 48) and I2C OLED (SDA=41, SCL=42).

## Expected Behavior

### OLED Display (scrolling, updates continuously)

```
Line 0 (scroll 200ms): Uptime: 0h 5m 30s | Tasks: 6
Line 1 (scroll 250ms): Sensor: 42% | Ref: D=3100 W=750 | Trig: dry<25% wet>35%
Line 2 (scroll 300ms): Heap: 250000/400000 | PSRAM: 8388608 | RSSI: -65 dBm | SDK: v5.1.4
Line 3 (static):       StatusPanel v1.0
```

### NeoPixel (GPIO 48)

| Heap Free | Color | Meaning |
|---|---|---|
| > 100 KB | Green | Healthy — plenty of memory |
| 50–100 KB | Yellow | Warning — memory getting tight |
| < 50 KB | Red | Critical — near memory exhaustion |

Updates every second based on `ESP.getFreeHeap()`.

### Serial Output (921600 baud, every 15s)

```
StatusPanel — System Health Monitor
  OLED: scrolling dashboard, NeoPixel: heap status
[Heap: 287456/400000 free] [WiFi: -62 dBm]
```

## External Consumer Pattern

This test imports one service and one driver directly:

```ini
lib_extra_dirs =
    ../lib
    ../services
```

```cpp
#include <StatusDashboard.h>    // from firmware/services/
#include <LedRGBController.h>   // from firmware/device_drivers/
```
