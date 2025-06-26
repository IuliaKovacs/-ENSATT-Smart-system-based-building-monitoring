#ifndef VIBRATION_SENSOR_H
#define VIBRATION_SENSOR_H

#define VIBRATION_SENSOR_PIN  5      


#include <Arduino.h>

void initVibrationSensor();
void readVibrationSensor(bool *earthquakeDetected);

#endif