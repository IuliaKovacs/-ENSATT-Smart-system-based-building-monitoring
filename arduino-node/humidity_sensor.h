#ifndef HUMIDITY_SENSOR_H
#define HUMIDITY_SENSOR_H

#include <Arduino.h>

void initHumidityTemperatureSensor();
float readHumidity();
float readTemperature();

#endif
