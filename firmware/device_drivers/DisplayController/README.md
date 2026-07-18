# DisplayController

SSD1306 OLED display wrapper over the U8g2 library. Provides a thread-safe, queue-based, multi-producer 4-line (128×32) or 8-line (128×64) text display. Each line is an independent FreeRTOS message queue, allowing different tasks to safely call `setLine()` without mutual exclusion. Uses U8g2 full-buffer mode for flicker-free rendering.

## What it contains

| File | Role |
|---|---|
| `src/DisplayController.h` | Class declaration — constructor, setLine, scroll/font configuration, layout queries |
| `src/DisplayController.cpp` | Implementation — FreeRTOS queues, U8g2 rendering, marquee scrolling |
| `library.json` | PlatformIO library manifest |

## Setup

### PlatformIO

```ini
lib_extra_dirs = /path/to/Robotics/firmware/device_drivers
lib_deps = olikraus/U8g2
```

### Dependencies

| Dependency | Source |
|---|---|
| `U8g2` (`olikraus/U8g2`) | External — PlatformIO resolves from `library.json` |
| `Wire.h` (I2C) | Bundled with Arduino framework |
| FreeRTOS (`queue`, `queue set`) | Bundled with Arduino-ESP32 framework |

### Wiring

| Display Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| SDA | GPIO 41 | I2C data |
| SCL | GPIO 42 | I2C clock |
| VCC | 3.3V | — |
| GND | GND | — |

Default I2C address: `0x3C` (most SSD1306 modules). Some modules use `0x3D`.

## Configuration

### Constructor

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `sda` | `uint8_t` | Any GPIO | _required_ | I2C data pin. Typically GPIO 41 on DevKitC-1. |
| `scl` | `uint8_t` | Any GPIO | _required_ | I2C clock pin. Typically GPIO 42 on DevKitC-1. |
| `addr` | `uint8_t` | Any I2C address | `0x3C` | I2C address of the SSD1306. Most modules use 0x3C; some use 0x3D. Check your module's datasheet or run an I2C scanner. |
| `height` | `uint8_t` | 32 or 64 | 32 | Display height in pixels. **32 = 4 lines** (128×32 display), **64 = 8 lines** (128×64 display). Determines the number of available text lines via `getNumLines()`. |

### Per-Line Configuration

| Option | Type | Range | Default | Description |
|---|---|---|---|---|
| `setScrollEnabled(line, enabled)` | `uint8_t`, `bool` | 0 – (getNumLines()-1), true/false | false | Enable/disable horizontal marquee scrolling for text wider than the display. When enabled, text scrolls left by `scrollOffset` pixels per step. |
| `setScrollSpeed(line, msPerStep)` | `uint8_t`, `uint16_t` | 0 – (getNumLines()-1), 50–2000 typical | 250 ms | Scroll step interval in milliseconds. Lower = faster scrolling. Values below 50 may be too fast to read. Values above 2000 are very slow. Scroll is disabled by default — no speed is used until `setScrollEnabled` is called. |
| `setFont(line, font)` | `uint8_t`, `const uint8_t*` | 0 – (getNumLines()-1), U8g2 font pointer | `u8g2_font_5x7_tf` | Set the U8g2 font for a specific line. Different lines can use different fonts. Fonts taller than LINE_HEIGHT (8 pixels) will be clipped. Common options: `u8g2_font_5x7_tf` (default, ~7px), `u8g2_font_5x8_tf` (~8px), `u8g2_font_6x10_tf` (~10px, clipped), `u8g2_font_t0_12b_tf` (12px). |

### Internal Constants

| Constant | Value | Description |
|---|---|---|
| `MAX_LINES` | 8 | Maximum supported lines (matches 128×64 display) |
| `LINE_HEIGHT` | 8 | Pixel height allocated per text line |
| `TEXT_BUFFER_SIZE` | 96 | Max characters per line (fit for 128px width at 5px font) |
| `QUEUE_SIZE` | 2 | Depth of each per-line FreeRTOS queue |
| `DEFAULT_SCROLL_SPEED` | 250 | Default scroll step interval in milliseconds |
| `SCROLL_PAUSE_TICKS` | 6 | Pause at start/end of scroll cycle (in render ticks) |

### Layout Queries (getters)

| Method | Returns | Description |
|---|---|---|
| `getNumLines()` | `uint8_t` | Number of available lines: 4 for 128×32, 8 for 128×64 |
| `getHeight()` | `uint8_t` | Display height in pixels (32 or 64) |
| `getWidth()` | `uint8_t` | Display width in pixels (128) |
| `getScrollEnabled(line)` | `bool` | Whether scrolling is enabled for a line |
| `getFont(line)` | `const uint8_t*` | Current font for a line |

## Usage

```cpp
#include <DisplayController.h>

DisplayController display(41, 42, 0x3C, 32);  // SDA=41, SCL=42, addr=0x3C, 128x32

void displayTask(void*) {
    display.begin();

    for (;;) {
        display.setLine(0, "Line 0: %d", millis() / 1000);
        display.setLine(1, "Uptime: %lus", millis() / 1000);

        // Process next queued update (or block with timeout)
        display.processNext(pdMS_TO_TICKS(100));
    }
}

void setup() {
    xTaskCreatePinnedToCore(displayTask, "disp", 4096, NULL, 1, NULL, 0);
}
```

### Process Model

`setLine()` is a non-blocking queue push. It will never block the caller — if the queue is full, the oldest update is dropped.

`processNext()` drains all queues and renders one frame. It should be called from a dedicated display task in a loop. The FreeRTOS QueueSet pattern waits efficiently on all per-line queues simultaneously.

## Hardware

- SSD1306 OLED display, 128×32 or 128×64 pixels
- I2C interface
- 3.3V logic level (ESP32-S3 compatible)

## Expected Behavior

- `begin()` initializes the I2C bus, configures the U8g2 display, and creates per-line FreeRTOS queues.
- **Multi-producer safe**: Any FreeRTOS task can call `setLine()` without locking. If two tasks set the same line simultaneously, the last one to arrive wins (queue depth 2 means one may be dropped).
- **Rendering**: `processNext()` redraws the entire frame into U8g2's internal buffer and sends it to the display in one `sendBuffer()` call — no per-line flicker.
- **Queue timeout**: If no `setLine()` calls have been made, `processNext()` blocks for up to `timeout` ticks, then returns `false`. If an update is available, it renders and returns `true`.
- **Scrolling**: When enabled, text wider than 128 pixels scrolls horizontally at the configured speed. The text pauses briefly at the start and end of each scroll cycle (controlled by `SCROLL_PAUSE_TICKS`).
- **Font changes**: Take effect on the next `processNext()` call.
- **Line overflow**: Text longer than `TEXT_BUFFER_SIZE` (96 chars) is truncated. On a 128-pixel display with the default 5×7 font, approximately 25 characters fit per line.
- At boot, the display briefly shows an empty frame until the first `setLine()` + `processNext()` cycle.
