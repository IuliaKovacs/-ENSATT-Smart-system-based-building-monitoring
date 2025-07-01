#include "humidity_sensor.h"
#include <DHT.h>

#define HUMIDITY_DHT_PIN 2
#define DHTTYPE DHT11

DHT dht(HUMIDITY_DHT_PIN, DHTTYPE);

void initHumidityTemperatureSensor() {
    dht.begin();
    Serial.println("MOIST INIT");
}

bool readHumiditySensor(float *result, bool average = false) {
    *result += dht.readHumidity();
    if (average) *result /= 2;

    if (isnan(*result)) {
        Serial.println("MOIST E H");
        return false;
    }

    return true;
}

bool readTemperatureSensor(float *result, bool average = false) {
    *result += dht.readTemperature();
    if (average) *result /= 2;

    if (isnan(*result)) {
        Serial.println("MOIST E T");
        return false;
    }

    return true;
}
