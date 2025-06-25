#ifndef FLAME_SENSOR_H
#define FLAME_SENSOR_H

#include <Arduino.h>

#define FLAME_SENSOR_PIN 3

void initFlameSensor();
bool isFlameDetected();

#endif
