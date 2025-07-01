#ifndef AIR_QUALITY_H
#define AIR_QUALITY_H

#include <Arduino.h>
#include "Adafruit_SGP30.h"
#include <math.h>

void initAirQuality();
void readCo2Sensor(uint16_t *co2_level, bool average);

#endif