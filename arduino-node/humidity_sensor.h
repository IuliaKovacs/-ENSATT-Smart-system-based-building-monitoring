#ifndef HUMIDITY_SENSOR_H
#define HUMIDITY_SENSOR_H

#include <Arduino.h>

void initHumidityTemperatureSensor();
bool readHumiditySensor(float *result);
bool readTemperatureSensor(float *result);

#endif
