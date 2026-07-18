#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/*
 * DisplayController — SSD1306 OLED display wrapper over U8g2
 *
 * Provides a thread-safe, queue-based, multi-producer 4-line (128x32) or
 * 8-line (128x64) text display.  Each line is an independent FreeRTOS
 * message queue, so different tasks can safely call setLine() without
 * mutual exclusion.  A single consumer (processNext, typically called from
 * a dedicated display task) drains all queues and renders the frame.
 *
 * U8g2 full-buffer mode is used: all lines are redrawn into the internal
 * buffer and transferred to the display in a single sendBuffer() call,
 * eliminating per-line flicker.
 *
 * Supports per-line horizontal marquee scrolling for text wider than the
 * display.  Scroll is disabled by default.
 *
 * Constructor:
 *   DisplayController d(sda, scl);                   // 128x32, addr 0x3C
 *   DisplayController d(sda, scl, 0x3C, 64);          // 128x64
 */

class DisplayController {
public:
    DisplayController(uint8_t sda, uint8_t scl,
                      uint8_t addr = 0x3C,
                      uint8_t height = 32);
    ~DisplayController();

    void begin();

    void setLine(uint8_t line, const char* fmt, ...)
        __attribute__((format(printf, 3, 4)));

    bool processNext(TickType_t timeout = portMAX_DELAY);

    // ── Per-line horizontal marquee scrolling ─────────────────────

    void setScrollEnabled(uint8_t line, bool enabled);
    void setScrollSpeed(uint8_t line, uint16_t msPerStep);
    bool getScrollEnabled(uint8_t line) const;

    // ── Per-line font ────────────────────────────────────────────
    // Each line can use a different U8g2 font.  The default for all
    // lines is u8g2_font_5x7_tf.  Larger fonts will be clipped to
    // the line height (LINE_HEIGHT pixels); choose fonts that fit.
    //
    // Common font references (all proportional, transparent bg):
    //   u8g2_font_5x7_tf     ~7 px  (default)
    //   u8g2_font_5x8_tf     ~8 px
    //   u8g2_font_6x10_tf    ~10 px  (clipped in 8 px lines)
    //   u8g2_font_t0_12b_tf  12 px

    void setFont(uint8_t line, const uint8_t* font);
    const uint8_t* getFont(uint8_t line) const;

    // ── Layout queries ────────────────────────────────────────────

    uint8_t       getNumLines()   const { return _numLines; }
    uint8_t       getHeight()     const { return _height; }
    uint8_t       getWidth()      const { return _width; }

    static const uint8_t MAX_LINES          = 8;
    static const uint8_t LINE_HEIGHT        = 8;
    static const uint16_t DEFAULT_SCROLL_SPEED = 250;
    static const uint16_t SCROLL_PAUSE_TICKS   = 6;
    static const uint8_t  TEXT_BUFFER_SIZE     = 96;

private:
    struct LineUpdate {
        char text[TEXT_BUFFER_SIZE];
    };

    static const uint8_t QUEUE_SIZE = 2;

    u8g2_t  _u8g2;
    uint8_t _sda;
    uint8_t _scl;
    uint8_t _addr;
    uint8_t _width;
    uint8_t _height;
    uint8_t _numLines;

    QueueHandle_t    _queues[MAX_LINES];
    QueueSetHandle_t _queueSet;
    char             _cache[MAX_LINES][TEXT_BUFFER_SIZE];

    // Scroll state (per line)
    bool          _scrollEnabled[MAX_LINES];
    int16_t       _scrollOffset[MAX_LINES];
    uint16_t      _scrollStepMs[MAX_LINES];
    unsigned long _lastScrollTick[MAX_LINES];
    uint8_t       _scrollPause[MAX_LINES];

    // Font per line
    const uint8_t* _fonts[MAX_LINES];

    void _renderFrame();

    // Scroll helpers
    bool     _anyScrollEnabled() const;
    uint16_t _minScrollStepMs() const;
    int16_t  _textPixelWidth(uint8_t line);
    void     _advanceScrolling();
};
