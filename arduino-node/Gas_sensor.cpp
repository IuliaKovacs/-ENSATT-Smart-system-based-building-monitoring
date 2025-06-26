#include "Gas_sensor.h"
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include <math.h>

Adafruit_SGP30 sgp;
int aq_counter = 0;

uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f *
    exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature));
  return static_cast<uint32_t>(1000.0f * absoluteHumidity);
}

void initAirQuality() {
//  Serial.println("SGP30 init...");
  if (!sgp.begin()) {
    Serial.println("Sensor not found :(");
    while (1);
  }

//  Serial.print("Found SGP30 serial #");
//  Serial.print(sgp.serialnumber[0], HEX);
//  Serial.print(sgp.serialnumber[1], HEX);
//  Serial.println(sgp.serialnumber[2], HEX);
}

void readAirQuality(float *eCO2_aux, float *TVOC_aux) {
  if (!sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }

  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

//  if (!sgp.IAQmeasureRaw()) {
//    Serial.println("Raw Measurement failed");
//    return;
//  }

//  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
//  Serial.print("Raw Ethanol "); Serial.println(sgp.rawEthanol);

// -- dynamical compensation of the baselin -- //
  aq_counter++;
  if (aq_counter == 30) {
    aq_counter = 0;
    uint16_t TVOC_base, eCO2_base;
    if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);

    *eCO2_aux = eCO2_base;
    *TVOC_aux = TVOC_base;
  }
}
