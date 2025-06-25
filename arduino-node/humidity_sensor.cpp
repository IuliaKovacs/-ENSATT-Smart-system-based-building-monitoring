#include "humidity_sensor.h"
#include <DHT.h>

#define HUMIDITY_DHT_PIN 2
#define DHTTYPE DHT11

DHT dht(HUMIDITY_DHT_PIN, DHTTYPE);

void initHumidityTemperatureSensor() {
    dht.begin();
    Serial.println("DHT11 sensor initialized!");
}

float readHumidity() {
    float result = dht.readHumidity();
    if (isnan(result)) {
        Serial.println("Error! Failed to read the data from Humidity Sensor!");
        return -1;
    }
    return result;
}

float readTemperature() {
    float result = dht.readTemperature();
    if (isnan(result)) {
        Serial.println("Error! Failed to read the data from Temperature Sensor!");
        return -1;
    }
    return result;
}
