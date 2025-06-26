#include "sound_sensor.h"

void initSoundSensor() {
    pinMode(SOUND_SENSOR_PIN, INPUT);
}

bool readSoundLevel(int *soundLevel) {
    // function returns true if Sound Level is too high  
    // the sound level is returned through the soundLevel argument
    *soundLevel = analogRead(SOUND_SENSOR_PIN);
    //   Serial.println(soundLevel);  

    if (*soundLevel < SOUND_THRESHOLD ) {
        return false;
    } 
    else if (*soundLevel > SOUND_THRESHOLD ) {
        return true;
    }
}