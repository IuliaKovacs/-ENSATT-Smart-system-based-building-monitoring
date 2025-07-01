#ifndef SOUND_SENSOR_H
#define SOUND_SENSOR_H

#define SOUND_SENSOR_PIN A2

#include <Arduino.h>

void initSoundSensor();
void readSoundSensor(uint16_t *result, bool average);

#endif