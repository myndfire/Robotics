/*
 * AdcController.cpp
 *
 * See AdcController.h for the public API description and usage examples.
 */

#include "AdcController.h"

AdcController::AdcController(uint8_t pin)
    : _pin(pin)
    , _value(0)
    , _attenuation(static_cast<adc_attenuation_t>(DEFAULT_ATTENUATION))
    , _resolution(DEFAULT_RESOLUTION)
    , _beginCalled(false)
{}

void AdcController::begin() {
    analogReadResolution(_resolution);
    analogSetPinAttenuation(_pin, _attenuation);

    // Discard first reading after configuration change to allow ADC
    // internal capacitors to settle to the new voltage level.
    analogRead(_pin);

    _beginCalled = true;
}

int AdcController::read() {
    _value = analogRead(_pin);
    return _value;
}

uint32_t AdcController::readMilliVolts() {
    uint32_t mv = analogReadMilliVolts(_pin);
    _value = mv;
    return mv;
}

int AdcController::readAveraged(uint8_t n) {
    if (n == 0) n = 1;
    long sum = 0;
    for (uint8_t i = 0; i < n; i++) {
        sum += analogRead(_pin);
    }
    _value = static_cast<int>(sum / n);
    return _value;
}

int AdcController::getValue() const {
    return _value;
}

uint8_t AdcController::getPin() const {
    return _pin;
}

adc_attenuation_t AdcController::getAttenuation() const {
    return _attenuation;
}

uint8_t AdcController::getResolution() const {
    return _resolution;
}

void AdcController::setAttenuation(adc_attenuation_t atten) {
    _attenuation = atten;
    analogSetPinAttenuation(_pin, _attenuation);
}

void AdcController::setResolution(uint8_t bits) {
    _resolution = bits;
    analogReadResolution(_resolution);
}
