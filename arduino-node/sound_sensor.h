#ifndef SOUND_SENSOR_H
#define SOUND_SENSOR_H

#define SOUND_SENSOR_PIN  A0      
#define SOUND_THRESHOLD  100  

#include <Arduino.h>

void initSoundSensor();
bool readSoundLevel(int *soundLevel);

#endif