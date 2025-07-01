#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>

#define LIGHT_SENSOR_PIN A1

void initLightSensor();
void readLightSensor(uint16_t *result, bool average);

#endif