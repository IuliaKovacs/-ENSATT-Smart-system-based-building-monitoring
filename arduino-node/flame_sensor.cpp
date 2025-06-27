#include "flame_sensor.h"

void initFlameSensor() {
    pinMode(FLAME_SENSOR_PIN, INPUT);
}

void readFlameSensor(bool *flameDetected) {
    if (digitalRead(FLAME_SENSOR_PIN) == LOW){
        *flameDetected = false;
    }
    else{
        Serial.println("!!! -- FLAME DETECTED -- !!!");
        *flameDetected = true;
    }
}
