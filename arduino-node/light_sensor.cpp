#include "light_sensor.h"
#include <Arduino.h>

const int light_sensor_pin = A3;

void initLightSensor() {
  pinMode(light_sensor_pin, INPUT);
}

void readLightLevel(int *light_level) {
  *light_level = analogRead(light_sensor_pin);
}