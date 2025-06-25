#include <SoftwareSerial.h>
#include <Wire.h>
#include "buzzer_sensor.h"
#include "humidity_sensor.h"
#include "flame_sensor.h"
#include "LED_array.h"

typedef float float32_t;


SoftwareSerial btSerial(10, 9);

typedef struct {
  uint16_t device_id;
  uint16_t timestamp;
} DataId;

millis()

typedef struct {
  uint8_t has_temperature : 1;
  uint8_t has_humidity_level : 1;
  uint8_t has_noise_level : 1;
  uint8_t has_vibration_level : 1;
  uint8_t has_brightness_level : 1;
  uint8_t has_counter : 1;
  uint8_t has_flame_status : 1;
  uint8_t has_alarm_state : 1;

  uint16_t reserved : 9;
} FieldsStatuses;

typedef enum {
  ALARM_STATUS_UNDEFINED = 0,
  ALARM_STATUS_ACTIVE    = 1,
  ALARM_STATUS_DISABLED  = 2
} AlarmStatus;

typedef enum {
  OK = 0,
  ERROR = 1,
  UNKNOWN = -1
} BleMeshCommandResult;

// All commands return following format:
// BleMeshCommandResult;<command-response>\n
typedef enum {
  // Parameters: void
  // Returns: void
  BLE_MESH_COMMAND_OK = 0,

  // Parameters: void
  // Returns array of DataId from mesh_node_data_buffer
  BLE_MESH_COMMAND_LIST_DATA_ENTRIES = 1,

  // Parameters: DataId
  // Returns: DataEntry
  BLE_MESH_COMMAND_GET_DATA_ENTRY = 2,

  // Parameters: DataEntry
  // Returns: void
  BLE_MESH_COMMAND_PUSH_DATA_ENTRY = 3,
} BleMeshCommand;

typedef struct {
  DataId         id;
  FieldsStatuses fields_statuses;
  
  float32_t temperature;
  float32_t humidity_level;
  uint16_t noise_level;
  uint16_t vibration_level;
  uint16_t brightness_level;
  uint16_t counter;
  bool flame_detected;
  AlarmStatus alarm_state;
} DataEntry;

DataEntry mesh_node_data_buffer[50] = {0};
DataEntry aggregation_data_entry = {0};
bool is_first_read = true;

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  initHumidityTemperatureSensor();
  initBuzzer();
  initLedArray();
}

void loop() {
  // put your main code here, to run repeatedly:
  // uint8_t last_data_entry_point_id = 13
  // aggregation_data_entry.id.timestamp = millis()
  // mesh_node_data_buffer[++last_data_entry_point_id] = aggregation_data_entry

  // GO TO SLAVE MODE
  // delay(10000)
  // GO TO MASTER MODE
  // delay(3000)

  


  if (Serial.available()) {
    String message = Serial.readString();
    btSerial.print(message);

    Serial.print("> ");
    Serial.println(message);
  }

  if (btSerial.available()) {
    String message = btSerial.readString();
    Serial.println(message);
  }
  
}

void readTemperatureAndHumidity() {
  aggregation_data_entry.temperature = readTemperature();
  aggregation_data_entry.humidity_level = readHumidity();
}

void readNoiseLevel() {
  
}

void readVibrationLevel() {

}

void readBrightnessLevel() {

}

void readCounter() {

}

void readAlarmDisable() {
  
}

void readFlameStatus() {
  aggregation_data_entry.flame_detected = isFlameDetected();
}