/*
 * LedRGBController.h — NeoPixel RGB LED wrapper with application-level semantics
 *
 * A self-contained WS2812 / SK6812 driver that wraps Adafruit_NeoPixel
 * directly.  Provides both low-level colour helpers and high-level status
 * indicators so it can serve as a drop-in status LED in any project.
 */
#pragma once

#include <Adafruit_NeoPixel.h>

class LedRGBController {
public:
  LedRGBController(uint8_t pin, uint8_t numPixels = 1,
                   neoPixelType type = NEO_GRB + NEO_KHZ800);

  void begin();
  void setBrightness(uint8_t brightness);

  void turn_Red();
  void turn_Green();
  void turn_Blue();
  void turn_Cyan();
  void turn_Magenta();
  void turn_Yellow();
  void turn_White();
  void turn_Off();

  void turn_Color(uint8_t r, uint8_t g, uint8_t b);
  void turn_HSV(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);

  void setRelayState(bool relayOn);
  void indicateBoot();

private:
  Adafruit_NeoPixel _pixel;
  uint8_t _numPixels;
};
