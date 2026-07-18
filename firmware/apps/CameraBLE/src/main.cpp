#include <Arduino.h>
#include <CameraBluetoothServer.h>

CameraBluetoothServer server;

void setup() {
    Serial.begin(921600);
    delay(500);
    server.begin();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
