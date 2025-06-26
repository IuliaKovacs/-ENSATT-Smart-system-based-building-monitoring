#include "humidity_sensor.h"
#include <DHT.h>

#define HUMIDITY_DHT_PIN 2
#define DHTTYPE DHT11

DHT dht(HUMIDITY_DHT_PIN, DHTTYPE);

void initHumidityTemperatureSensor() {
    dht.begin();
    Serial.println("DHT11 sensor initialized!");
}

bool readHumiditySensor(float *result) {
    *result = dht.readHumidity();
    if (isnan(*result)) {
        Serial.println("Error! Failed to read the data from Humidity Sensor!");
        return false;
    }
    return true;
}

bool readTemperatureSensor(float *result) {
    *result = dht.readTemperature();
    if (isnan(*result)) {
        Serial.println("Error! Failed to read the data from Temperature Sensor!");
        return false;
    }
    return true;
}
