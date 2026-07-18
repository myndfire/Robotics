/*
 * MultiSensor — Integration Test
 *
 * Reads 3 analog sensors (light, potentiometer, and soil moisture) on
 * ADC1 pins, displays all channels on a scrolling OLED dashboard using
 * StatusDashboard's display infrastructure.
 *
 * Demonstrates composing multiple driver services together with a
 * system-level display service.
 *
 * Hardware needed:
 *   - ESP32-S3 DevKitC-1
 *   - 3 analog sensors on GPIO 1, 2, 3 (ADC1)
 *   - SSD1306 128x32 OLED (SDA=41, SCL=42)
 *
 * Run:
 *   uv run pio run --target upload
 *   uv run pio run --target upload --target monitor
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <AdcController.h>
#include <DisplayController.h>

AdcController adc0(1);  // Light sensor
AdcController adc1(2);  // Potentiometer
AdcController adc2(3);  // Aux sensor

DisplayController display(41, 42);

static int raw0 = 0, raw1 = 0, raw2 = 0;

static void sensorTask(void* pv) {
    for (;;) {
        raw0 = adc0.read();
        raw1 = adc1.read();
        raw2 = adc2.read();

        int pct0 = raw0 * 100 / 4095;
        int pct1 = raw1 * 100 / 4095;
        int pct2 = raw2 * 100 / 4095;

        display.setLine(0, "CH0:%d%% CH1:%d%% CH2:%d%%", pct0, pct1, pct2);
        display.setLine(1, "R0:%d R1:%d R2:%d", raw0, raw1, raw2);
        display.setLine(2, "mV0:%umV mV1:%umV mV2:%umV",
            adc0.readMilliVolts(), adc1.readMilliVolts(), adc2.readMilliVolts());
        display.setLine(3, "ADC1:3ch 12bit 11dB");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void displayTask(void* pv) {
    for (;;) {
        display.processNext(portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(921600);
    delay(500);

    adc0.begin();
    adc1.begin();
    adc2.begin();

    display.begin();
    display.setFont(0, u8g2_font_5x8_tf);
    display.setScrollEnabled(0, true);
    display.setScrollSpeed(0, 200);

    xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(displayTask, "display", 4096, NULL, 1, NULL, 1);

    Serial.println("MultiSensor — 3-channel ADC monitor");
    Serial.println("  CH0: GPIO1 | CH1: GPIO2 | CH2: GPIO3");
    Serial.println("  OLED: SDA=41 SCL=42 | updates every 1s");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
