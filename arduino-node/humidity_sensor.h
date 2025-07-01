#ifndef HUMIDITY_SENSOR_H
#define HUMIDITY_SENSOR_H

#include <Arduino.h>

void initHumidityTemperatureSensor();
bool readHumiditySensor(float *result, bool average);
bool readTemperatureSensor(float *result, bool average);

#endif
