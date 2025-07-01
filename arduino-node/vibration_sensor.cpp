#include "vibration_sensor.h"

void initVibrationSensor(){
    pinMode(VIBRATION_SENSOR_PIN, INPUT);

    Serial.println("VIBR INIT");
}


void readVibrationSensor(int *result, bool average) {
    *result = analogRead(VIBRATION_SENSOR_PIN);

    if (average) *result /= 2;
}