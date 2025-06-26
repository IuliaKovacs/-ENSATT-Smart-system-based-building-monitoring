#include "vibration_sensor.h"

void initVibrationSensor(){
    pinMode(VIBRATION_SENSOR_PIN, INPUT);
}


void readVibrationSensor(bool *earthquakeDetected){

    int vibration = digitalRead(VIBRATION_SENSOR_PIN);

    if (vibration == LOW) {   
        Serial.println("!!! - Earthquake Detected! - !!!"); 
        *earthquakeDetected = true;      
    } else {
        *earthquakeDetected = false;
    }
}