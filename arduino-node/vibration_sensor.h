#ifndef VIBRATION_SENSOR_H
#define VIBRATION_SENSOR_H

#include <Arduino.h>

#define VIBRATION_SENSOR_PIN A0

void initVibrationSensor();
void readVibrationSensor(int *result, bool average);

#endif