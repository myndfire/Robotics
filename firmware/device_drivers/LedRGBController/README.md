# LedRGBController

Self-contained WS2812/SK6812 NeoPixel RGB LED driver wrapping Adafruit_NeoPixel. Provides both low-level color helpers and high-level status indicators for use as a drop-in status LED in any project.

## What it contains

| File | Role |
|---|---|
| `src/LedRGBController.h` | Class declaration — constructor, color methods, application-level helpers |
| `src/LedRGBController.cpp` | Implementation — Adafruit_NeoPixel wrapper, color set calls |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
lib_deps = adafruit/Adafruit NeoPixel
```

### Dependencies

| Dependency | Source |
|---|---|
| Adafruit NeoPixel (`adafruit/Adafruit NeoPixel`) | External — PlatformIO resolves from `library.json` |

### Wiring

| NeoPixel Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| DIN (Data In) | GPIO 48 | Built-in on ESP32-S3 DevKitC-1 |
| VCC | 5V or 3.3V | 3.3V works for single pixels; use 5V for strips |
| GND | GND | — |

For the DORHEA Mini board variant, the NeoPixel is on **GPIO 21** with `NEO_RGB` color order.

## Configuration

### Constructor

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `pin` | `uint8_t` | Any GPIO | _required_ | GPIO pin connected to the NeoPixel data input (DIN). Use GPIO 48 for the on-board NeoPixel on ESP32-S3 DevKitC-1. |
| `numPixels` | `uint8_t` | 1 – 255 | 1 | Number of NeoPixels in the chain. Each pixel draws up to 60 mA at full brightness white. Default 1 for a status LED. |
| `type` | `neoPixelType` | `NEO_RGB`, `NEO_RBG`, `NEO_GRB`, `NEO_GBR`, `NEO_BRG`, `NEO_BGR` + `NEO_KHZ400` or `NEO_KHZ800` | `NEO_GRB + NEO_KHZ800` | Color byte order and communication speed. Most WS2812/SK6812 use `NEO_GRB + NEO_KHZ800`. DORHEA Mini uses `NEO_RGB`. `NEO_KHZ800` (800 kHz) is the standard — use `NEO_KHZ400` only for very old or long-chain pixels. |

### Brightness

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setBrightness(brightness)` | `uint8_t` | 0 – 255 | 255 (full) | Master brightness applied to all subsequent color calls. 0 = off, 255 = full brightness. Does not affect already-displayed colors — you must call a color method after changing brightness to see the effect. Reducing brightness significantly lowers current draw (important for USB-powered boards). This is a software brightness — the NeoPixel's actual PWM is scaled, not a lower LED current. |

### Color Methods

| Method | Description |
|---|---|
| `turn_Red()` | Set all pixels to red (255, 0, 0) |
| `turn_Green()` | Set all pixels to green (0, 255, 0) |
| `turn_Blue()` | Set all pixels to blue (0, 0, 255) |
| `turn_Cyan()` | Set all pixels to cyan (0, 255, 255) |
| `turn_Magenta()` | Set all pixels to magenta (255, 0, 255) |
| `turn_Yellow()` | Set all pixels to yellow (255, 255, 0) |
| `turn_White()` | Set all pixels to white (255, 255, 255) |
| `turn_Off()` | Turn all pixels off (0, 0, 0) |

### Custom Color Methods

| Method | Type | Range | Description |
|---|---|---|---|
| `turn_Color(r, g, b)` | `uint8_t` × 3 | 0 – 255 each | Set all pixels to a custom RGB color. (0, 0, 0) = off, (255, 255, 255) = white. Use an RGB color picker for precise values. |
| `turn_HSV(hue, sat, val)` | `uint16_t`, `uint8_t`, `uint8_t` | hue: 0–65535, sat: 0–255, val: 0–255 | Set all pixels using HSV color space. `hue` wraps around (0 = 65536 = red, 10922 = green, 21845 = blue). `sat` = 0 for white/desaturated, 255 for fully saturated. `val` = brightness, 0 = off, 255 = full. |

### Application-Level Semantics

| Method | Description |
|---|---|
| `setRelayState(relayOn)` | Status indicator: **green** when relay is ON, **red** when relay is OFF. Convenience method that maps application state to LED color. |
| `indicateBoot()` | White flash for 500ms, then off. Standard boot indicator — call in `setup()` after `begin()`. |

## Usage

```cpp
#include <LedRGBController.h>

LedRGBController led(48);  // GPIO 48, single pixel, NEO_GRB+NEO_KHZ800

void setup() {
    led.begin();
    led.setBrightness(25);   // 10% brightness (25/255)
    led.indicateBoot();      // White flash 500ms → off
}

void loop() {
    led.turn_Red();     delay(500);
    led.turn_Green();   delay(500);
    led.turn_Blue();    delay(500);
    led.turn_Off();     delay(500);
}
```

## Hardware

- WS2812, WS2812B, SK6812, or compatible NeoPixel LEDs
- Single pixel on ESP32-S3 DevKitC-1 (GPIO 48), NEO_GRB color order
- DORHEA Mini (GPIO 21), NEO_RGB color order
- 3.3V logic compatible with ESP32-S3 — no level shifter needed for single pixels

## Expected Behavior

- `begin()` initializes the NeoPixel strip and turns all pixels off.
- `indicateBoot()` shows a single white flash lasting 500ms, then turns the LED off. Visually confirms the MCU has booted and the NeoPixel is functional.
- **Color changes are immediate** — calling any `turn_*` method updates the LED within microseconds.
- `setBrightness(25)` sets brightness to ~10%. The LED is still driven with the same current per PWM cycle, but the average apparent brightness is reduced. Full-white at brightness 255 on a single pixel draws ~60 mA from 5V.
- **Current draw** at various settings (single pixel, 5V supply):
  - Off (0,0,0): ~0 mA
  - Red/Grn/Blu at brightness 255: ~20 mA
  - White at brightness 255: ~60 mA
  - Any color at brightness 25 (10%): ~2–6 mA
- `setRelayState(true)` — LED turns green. `setRelayState(false)` — LED turns red. Used as a visual indicator of pump/solenoid state in moisture controller applications.
- The last color set persists until changed. There is no auto-off timeout.
- For multi-pixel chains, all pixels receive the same color. Use `turn_Color()` for full control or extend the class for per-pixel patterns.
