/*
 * LedRGBController.cpp
 *
 * See LedRGBController.h for the public API description.
 */
#include "LedRGBController.h"

LedRGBController::LedRGBController(uint8_t pin, uint8_t numPixels, neoPixelType type)
  : _pixel(numPixels, pin, type), _numPixels(numPixels)
{
  _pixel.begin();
}

void LedRGBController::begin() {
  _pixel.begin();
  _pixel.setBrightness(10);
}

void LedRGBController::setBrightness(uint8_t brightness) {
  _pixel.setBrightness(brightness);
}

void LedRGBController::turn_Red()         { turn_Color(255, 0, 0); }
void LedRGBController::turn_Green()        { turn_Color(0, 255, 0); }
void LedRGBController::turn_Blue()        { turn_Color(0, 0, 255); }
void LedRGBController::turn_Cyan()        { turn_Color(0, 255, 255); }
void LedRGBController::turn_Magenta()     { turn_Color(255, 0, 255); }
void LedRGBController::turn_Yellow()      { turn_Color(255, 255, 0); }
void LedRGBController::turn_White()       { turn_Color(255, 255, 255); }
void LedRGBController::turn_Off()         { turn_Color(0, 0, 0); }

void LedRGBController::turn_Color(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < _numPixels; i++) {
    _pixel.setPixelColor(i, _pixel.Color(r, g, b));
  }
  _pixel.show();
}

void LedRGBController::turn_HSV(uint16_t hue, uint8_t sat, uint8_t val) {
  uint32_t color = _pixel.ColorHSV(hue, sat, val);
  for (uint8_t i = 0; i < _numPixels; i++) {
    _pixel.setPixelColor(i, color);
  }
  _pixel.show();
}

void LedRGBController::setRelayState(bool relayOn) {
  if (relayOn) turn_Green();
  else         turn_Red();
}

void LedRGBController::indicateBoot() {
  turn_White();
  delay(500);
  turn_Off();
}
