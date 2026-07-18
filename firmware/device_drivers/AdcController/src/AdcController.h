/*
 * AdcController.h — General-purpose ESP32-S3 ADC wrapper
 *
 * Single-responsibility wrapper around the ESP32 Arduino ADC API. Each
 * instance owns one GPIO pin.  Supports configurable resolution, per-pin
 * attenuation, e-fuse-calibrated millivolt reads, and multi-sample averaging.
 * No external dependencies beyond the Arduino framework.
 *
 * ==================== ESP32-S3 ADC Units ====================
 * The ESP32-S3 has two SAR ADC units:
 *
 *   ADC1: GPIOs  1 – 10  (8 channels usable; GPIOs 3,4 may be strapping)
 *   ADC2: GPIOs 11 – 20  (10 channels)
 *
 * IMPORTANT: ADC2 is unavailable when WiFi is active.  If your application
 * uses WiFi, place all ADC reads on ADC1 pins (GPIO 1–10).
 *
 * ==================== Attenuation (Input Voltage Range) ====================
 * Values passed to setAttenuation() / getAttenuation():
 *
 *   ADC_0db    ~ 0 – 1.1 V   (default in some Arduino cores; lowest noise)
 *   ADC_2_5db  ~ 0 – 1.5 V
 *   ADC_6db    ~ 0 – 2.2 V
 *   ADC_11db   ~ 0 – 3.3 V   (default in this library; full range)
 *
 * Use setAttenuation() to change per pin.  Different pins can have
 * different attenuation levels simultaneously.
 *
 * ==================== Resolution ====================
 *   setResolution(12)  →  0 – 4095  (default, 12-bit)
 *   setResolution(11)  →  0 – 2047
 *   setResolution(10)  →  0 – 1023
 *   setResolution(9)   →  0 – 511
 *
 * Resolution is GLOBAL on ESP32 — calling setResolution() affects ALL ADC
 * reads on ALL pins, not just this instance.  This is a hardware limitation
 * documented here so callers are aware of the side effect.
 *
 * ==================== E-Fuse Calibrated Millivolt Reading ====================
 * readMilliVolts() uses per-chip factory calibration data burned into the
 * ESP32-S3 e-fuses by Espressif.  It compensates for manufacturing variance
 * and provides a more accurate voltage reading than raw * Vref / 2^bits.
 * The calibration is automatic — no setup required.
 *
 * ==================== Usage Example ====================
 *
 *   AdcController adc(1);               // GPIO 1
 *
 *   void setup() {
 *     adc.begin();                       // 12-bit, ADC_11db, discard read
 *     Serial.begin(921600);
 *   }
 *
 *   void loop() {
 *     int   raw  = adc.read();           // single conversion
 *     uint32_t mV = adc.readMilliVolts(); // e-fuse calibrated
 *     int   avg = adc.readAveraged(16);  // 16-sample average
 *
 *     Serial.printf("raw=%d  mV=%u  avg=%d\n", raw, mV, avg);
 *     delay(1000);
 *   }
 */

#pragma once

#include <Arduino.h>

class AdcController {
public:
    static constexpr uint8_t  DEFAULT_RESOLUTION  = 12;
    static constexpr uint16_t DEFAULT_ATTENUATION = ADC_11db;

    /*
     * Constructor.
     *
     * pin — GPIO number for the analog input.  Must be ADC-capable:
     *       ADC1: GPIOs 1–10   ADC2: GPIOs 11–20
     */
    AdcController(uint8_t pin);

    /*
     * Initialise the ADC subsystem.
     *
     * Sets the global ADC resolution to DEFAULT_RESOLUTION (12-bit) and
     * configures this pin's attenuation to DEFAULT_ATTENUATION (ADC_11db,
     * 0–3.3 V range).  Performs a single discard read to allow the ADC to
     * settle after configuration changes.
     *
     * Call once during setup() — can be safely called on multiple
     * AdcController instances; the global resolution will match the last
     * begin() or setResolution() call.
     */
    void begin();

    // ── Operations (trigger new hardware conversion) ──────────────────

    /*
     * Perform a single ADC conversion and return the raw value.
     *
     * Range depends on the current resolution:
     *   12-bit → 0–4095   11-bit → 0–2047
     *   10-bit → 0–1023    9-bit → 0–511
     */
    int read();

    /*
     * Perform a single ADC conversion and return the calibrated voltage
     * in millivolts (mV).
     *
     * Uses factory e-fuse calibration data to compensate for per-chip
     * manufacturing variance.  More accurate than raw * Vref / 2^bits.
     *
     * Example:  readMilliVolts() returning 1650 means 1.650 V.
     *
     * The calibration curve is created automatically on first call and
     * reused for subsequent reads on the same ADC unit.
     */
    uint32_t readMilliVolts();

    /*
     * Perform n ADC conversions and return the arithmetic mean.
     *
     * Useful for reducing noise on slow-changing signals.  Each sample
     * is a full hardware conversion; higher n gives smoother results at
     * the cost of more CPU time.
     *
     * n — number of samples (default 8).  Must be >= 1.
     */
    int readAveraged(uint8_t n = 8);

    // ── Getters (return last known state — no hardware conversion) ────

    /*
     * Value from the most recent read() / readMilliVolts() /
     * readAveraged() call.  Returns 0 if no conversion has been performed.
     */
    int getValue() const;

    /*
     * GPIO pin number passed to the constructor.
     */
    uint8_t getPin() const;

    /*
     * Current attenuation for this pin.  See the attenuation table in the
     * class-level documentation for voltage ranges.
     */
    adc_attenuation_t getAttenuation() const;

    /*
     * Global ADC resolution in bits (default 12).
     *
     * NOTE: resolution is a global property on ESP32 — calling
     * setResolution() on any AdcController instance affects ALL ADC reads.
     */
    uint8_t getResolution() const;

    // ── Setters ───────────────────────────────────────────────────────

    /*
     * Set the attenuation for this specific pin.
     *
     * Per-pin attenuation is supported by the ESP32 hardware — different
     * pins can have different attenuation levels at the same time.
     *
     * atten — one of ADC_0db, ADC_2_5db, ADC_6db, ADC_11db.
     *         See class-level documentation for voltage ranges.
     */
    void setAttenuation(adc_attenuation_t atten);

    /*
     * Set the global ADC resolution.
     *
     * WARNING: This is a GLOBAL setting on ESP32.  Calling this method
     * affects ALL ADC reads on ALL pins.  The resolution applies to the
     * entire ADC peripheral, not just this pin.
     *
     * bits — resolution in bits (9–12 supported in hardware).
     *        Values outside 9–12 are accepted by analogReadResolution()
     *        but the raw results will be shifted.
     */
    void setResolution(uint8_t bits);

private:
    uint8_t           _pin;
    int               _value;
    adc_attenuation_t _attenuation;
    uint8_t           _resolution;
    bool              _beginCalled;
};
