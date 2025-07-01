#include "flame_sensor.h"

void initFlameSensor() {
    pinMode(FLAME_SENSOR_PIN, INPUT);

    Serial.println("FLAME INIT");
}

void readFlameSensor(bool *flame_detected) {
    *flame_detected = !digitalRead(FLAME_SENSOR_PIN);
}
