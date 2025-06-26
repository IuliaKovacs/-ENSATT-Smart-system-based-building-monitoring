#ifndef AIR_QUALITY_H
#define AIR_QUALITY_H

#include <Arduino.h>

void initAirQuality();
void readAirQuality(float *eCO2_aux, float *TVOC_aux);
uint32_t getAbsoluteHumidity(float temperature, float humidity);

#endif