#include "sound_sensor.h"

void initSoundSensor() {
    pinMode(SOUND_SENSOR_PIN, INPUT);

    Serial.println("SOUND INIT");
}

void readSoundSensor(uint16_t *result, bool average) {
    *result += analogRead(SOUND_SENSOR_PIN);

    if (average) *result /= 2;
}