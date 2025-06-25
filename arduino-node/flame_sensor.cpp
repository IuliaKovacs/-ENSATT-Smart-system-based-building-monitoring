#include "flame_sensor.h"

void initFlameSensor() {
    pinMode(FLAME_SENSOR_PIN, INPUT);
}

bool isFlameDetected() {
    if (digitalRead(FLAME_SENSOR_PIN)){
        return false;
    }
    else{
        Serial.println("!!! -- FLAME DETECTED -- !!!");
        return true;
    }
}
