#include "gas_sensor.h"

Adafruit_SGP30 sgp;

void initAirQuality() {
  if (sgp.begin()) Serial.println("GAS INIT");
  else Serial.println("GAS FAIL");
}

void readCo2Sensor(uint16_t *co2_level, bool average) {
  sgp.IAQmeasure();

  *co2_level += sgp.eCO2;

  if (average) *co2_level /= 2;
}