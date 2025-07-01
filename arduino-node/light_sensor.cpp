#include "light_sensor.h"

void initLightSensor() {
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  Serial.println("LIGHT INIT");
}

void readLightSensor(uint16_t *result, bool average) {
  *result = analogRead(LIGHT_SENSOR_PIN);
  if (average) *result /= 2;
}