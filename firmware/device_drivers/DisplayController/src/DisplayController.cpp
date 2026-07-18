#include "DisplayController.h"

DisplayController::DisplayController(uint8_t sda, uint8_t scl,
                                     uint8_t addr, uint8_t height)
    : _sda(sda)
    , _scl(scl)
    , _addr(addr)
    , _width(128)
    , _height(height)
    , _numLines(height / LINE_HEIGHT)
{
    for (uint8_t i = 0; i < MAX_LINES; i++) {
        _queues[i]        = nullptr;
        _cache[i][0]      = '\0';
        _scrollEnabled[i]  = false;
        _scrollOffset[i]   = 0;
        _scrollStepMs[i]   = DEFAULT_SCROLL_SPEED;
        _lastScrollTick[i] = 0;
        _scrollPause[i]    = SCROLL_PAUSE_TICKS;
        _fonts[i]          = u8g2_font_5x7_tf;
    }
    _queueSet = nullptr;
}

DisplayController::~DisplayController() {
    if (_queueSet) {
        for (uint8_t i = 0; i < _numLines; i++) {
            if (_queues[i]) {
                xQueueRemoveFromSet(_queues[i], _queueSet);
                vQueueDelete(_queues[i]);
            }
        }
        vQueueDelete(_queueSet);
    }
}

void DisplayController::begin() {
    Wire.begin(_sda, _scl);
    Wire.setClock(400000);

    // Set up the U8g2 C struct with the appropriate display geometry
    if (_height == 64) {
        u8g2_Setup_ssd1306_i2c_128x64_noname_f(
            &_u8g2, U8G2_R0,
            u8x8_byte_arduino_hw_i2c,
            u8x8_gpio_and_delay_arduino);
    } else {
        u8g2_Setup_ssd1306_i2c_128x32_univision_f(
            &_u8g2, U8G2_R0,
            u8x8_byte_arduino_hw_i2c,
            u8x8_gpio_and_delay_arduino);
    }

    // U8g2 stores the I2C address shifted (e.g. 0x3C → 0x78).
    // u8g2_SetI2CAddress expects the pre-shifted value.
    u8g2_SetI2CAddress(&_u8g2, _addr * 2);

    u8g2_InitDisplay(&_u8g2);
    u8g2_SetPowerSave(&_u8g2, 0);
    u8g2_SetFont(&_u8g2, u8g2_font_5x7_tf);
    u8g2_ClearDisplay(&_u8g2);

    for (uint8_t i = 0; i < _numLines; i++) {
        _queues[i] = xQueueCreate(QUEUE_SIZE, sizeof(LineUpdate));
    }

    _queueSet = xQueueCreateSet(_numLines * QUEUE_SIZE);
    for (uint8_t i = 0; i < _numLines; i++) {
        xQueueAddToSet(_queues[i], _queueSet);
    }
}

void DisplayController::setLine(uint8_t line, const char* fmt, ...) {
    if (line >= _numLines || !_queues[line]) return;

    LineUpdate update;
    va_list args;
    va_start(args, fmt);
    vsnprintf(update.text, sizeof(update.text), fmt, args);
    va_end(args);

    xQueueSend(_queues[line], &update, 0);
}

bool DisplayController::processNext(TickType_t timeout) {
    if (!_queueSet) return false;

    // Cap timeout to the shortest scroll step so _advanceScrolling
    // runs at the right frequency.
    if (_anyScrollEnabled()) {
        TickType_t minStep = pdMS_TO_TICKS(_minScrollStepMs());
        if (minStep == 0) minStep = 1;
        if (timeout == portMAX_DELAY || timeout > minStep) {
            timeout = minStep;
        }
    }

    QueueHandle_t ready = xQueueSelectFromSet(_queueSet, timeout);
    bool hadUpdate = false;

    if (ready) {
        LineUpdate update;
        xQueueReceive(ready, &update, 0);

        for (uint8_t i = 0; i < _numLines; i++) {
            if (_queues[i] == ready) {
                if (strcmp(update.text, _cache[i]) != 0) {
                    strncpy(_cache[i], update.text, TEXT_BUFFER_SIZE - 1);
                    _cache[i][TEXT_BUFFER_SIZE - 1] = '\0';
                    _scrollOffset[i] = 0;
                    _scrollPause[i]  = SCROLL_PAUSE_TICKS;
                }
                hadUpdate = true;
                break;
            }
        }
    } else {
        _advanceScrolling();
    }

    // Single buffer-to-display transfer — no per-line flicker
    _renderFrame();

    return hadUpdate;
}

// ── Frame rendering ──────────────────────────────────────────────────

void DisplayController::_renderFrame() {
    u8g2_ClearBuffer(&_u8g2);

    for (uint8_t i = 0; i < _numLines; i++) {
        if (_cache[i][0] == '\0') continue;

        int y       = i * LINE_HEIGHT;
        int16_t off = _scrollEnabled[i] ? _scrollOffset[i] : 0;

        // Set this line's font and compute its baseline.  U8g2's
        // baseline convention: glyphs extend upward by GetAscent()
        // and downward by GetDescent() (both positive).  Placing the
        // baseline at y + ascent aligns the glyph top with the line
        // top, centering the font within LINE_HEIGHT.
        u8g2_SetFont(&_u8g2, _fonts[i]);
        int16_t ascent   = u8g2_GetAscent(&_u8g2);
        int16_t baseline = y + ascent;

        // Clear the line area (draw black box)
        u8g2_SetDrawColor(&_u8g2, 0);
        u8g2_DrawBox(&_u8g2, 0, y, _width, LINE_HEIGHT);

        // Draw text at the font-determined baseline
        u8g2_SetDrawColor(&_u8g2, 1);
        u8g2_DrawStr(&_u8g2, -off, baseline, _cache[i]);
    }

    u8g2_SendBuffer(&_u8g2);
}

// ── Scroll public API ─────────────────────────────────────────────────

void DisplayController::setScrollEnabled(uint8_t line, bool enabled) {
    if (line >= _numLines) return;
    _scrollEnabled[line] = enabled;
    _scrollOffset[line]  = 0;
    _scrollPause[line]   = enabled ? SCROLL_PAUSE_TICKS : 0;
}

void DisplayController::setScrollSpeed(uint8_t line, uint16_t msPerStep) {
    if (line >= _numLines) return;
    _scrollStepMs[line] = constrain(msPerStep, (uint16_t)50, (uint16_t)2000);
}

bool DisplayController::getScrollEnabled(uint8_t line) const {
    if (line >= _numLines) return false;
    return _scrollEnabled[line];
}

// ── Font public API ──────────────────────────────────────────────────

void DisplayController::setFont(uint8_t line, const uint8_t* font) {
    if (line >= _numLines || !font) return;
    _fonts[line] = font;
    // Reset scroll offset when font changes — old measurements invalid
    _scrollOffset[line] = 0;
    _scrollPause[line]  = SCROLL_PAUSE_TICKS;
}

const uint8_t* DisplayController::getFont(uint8_t line) const {
    if (line >= _numLines) return nullptr;
    return _fonts[line];
}

// ── Scroll helpers ────────────────────────────────────────────────────

bool DisplayController::_anyScrollEnabled() const {
    for (uint8_t i = 0; i < _numLines; i++) {
        if (_scrollEnabled[i]) return true;
    }
    return false;
}

uint16_t DisplayController::_minScrollStepMs() const {
    uint16_t min = 2000;
    for (uint8_t i = 0; i < _numLines; i++) {
        if (_scrollEnabled[i] && _scrollStepMs[i] < min) {
            min = _scrollStepMs[i];
        }
    }
    return min == 2000 ? DEFAULT_SCROLL_SPEED : min;
}

int16_t DisplayController::_textPixelWidth(uint8_t line) {
    u8g2_SetFont(&_u8g2, _fonts[line]);
    return (int16_t)u8g2_GetStrWidth(&_u8g2, _cache[line]);
}

void DisplayController::_advanceScrolling() {
    unsigned long now = millis();

    for (uint8_t i = 0; i < _numLines; i++) {
        if (!_scrollEnabled[i]) continue;

        int16_t textWidth = _textPixelWidth(i);
        if (textWidth <= _width) {
            _scrollOffset[i] = 0;
            _scrollPause[i]  = 0;
            continue;
        }

        if (now - _lastScrollTick[i] < _scrollStepMs[i]) continue;
        _lastScrollTick[i] = now;

        if (_scrollPause[i] > 0) {
            _scrollPause[i]--;
            if (_scrollPause[i] == 0 && _scrollOffset[i] > 0) {
                _scrollOffset[i] = 0;
                _scrollPause[i]  = SCROLL_PAUSE_TICKS;
            }
            continue;
        }

        _scrollOffset[i]++;

        if (_scrollOffset[i] >= textWidth) {
            _scrollPause[i] = SCROLL_PAUSE_TICKS;
        }
    }
}
