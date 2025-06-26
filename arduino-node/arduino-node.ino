#include <Wire.h>
#include "buzzer_sensor.h"
#include "humidity_sensor.h"
#include "flame_sensor.h"
#include "LED_array.h"
#include <avr/wdt.h>

// Increase RX buffer size to fit all responses without overwrite
#define _SS_MAX_RX_BUFF 256
#include <SoftwareSerial.h>

const String MESH_NODES[] = {"685E1C1A68CF", "685E1C1A5A30"};

typedef float float32_t;

SoftwareSerial btSerial(10, 9, 100);

typedef struct {
  uint16_t device_id;
  uint16_t timestamp;
} DataId;

typedef struct {
  uint8_t has_temperature : 1;
  uint8_t has_humidity_level : 1;
  uint8_t has_noise_level : 1;
  uint8_t has_vibration_level : 1;
  uint8_t has_brightness_level : 1;
  uint8_t has_co2_level : 1;
  uint8_t has_counter : 1;
  uint8_t has_flame_status : 1;
  uint8_t has_alarm_state : 1;

  uint16_t reserved : 8;
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
  uint16_t co2_level;
  uint16_t counter;
  bool flame_detected;
  AlarmStatus alarm_state;
} DataEntry;

typedef enum {
  STATE_MACHINE_IDLE;
  STATE_MACHINE_START_MASTER;
  STATE_MACHINE_DISCOVERY_STATE;
  STATE_MACHINE_DISCOVERY_CHECK;

  STATE_MACHINE_CONNECT_1;
} StateMachine;

static DataEntry mesh_node_data_buffer[20];
DataEntry aggregation_data_entry;
bool is_first_read = true;
uint16_t device_id = 0; // Update to BLE module last 2 bytes of MAC
bool has_error = false;

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  initBleModule();

  Serial.println("Finished initializing BLE");
  Serial.print("Current Device ID: ");
  Serial.println(device_id);
}

bool a = false;
void loop() {

  if (has_error) {
    Serial.println("Detected global error, resetting");
    Serial.println();
    Serial.flush();
    delay(100);
    reset();
  }

  switch 



  // if (Serial.available()) {
  //   String message = Serial.readString();
  //   btSerial.print(message);

  //   Serial.print("> ");
  //   Serial.println(message);
  // }

  // if (btSerial.available()) {
  //   String message = btSerial.readString();
  //   Serial.println(message);
  // }
  
}

void initBleModule() {
  sendClean("AT");
  sendClean("AT+RENEW");
  sendClean("AT+CLEAR");
  sendClean("AT+ROLE1");
  sendClean("AT+IMME1");
  sendClean("AT+SHOW0");

  sendClean("AT+ADDR?");
  char address_response[20];
  size_t bytes = btSerial.readBytes(address_response, 20);
  if (bytes != 20) {
    Serial.print("Failed to obtain BLE MAC address, recevied bytes: ");
    Serial.println(bytes);
    has_error = true;
    return;
  }

  char* end_pointer;
  long mac_result = strtol(&address_response[15], &end_pointer, 16);

  if (end_pointer == address_response[20]) {
    Serial.println("Error: No conversion performed");
    return 0;
  }

  device_id = mac_result;

  char board_name[128];
  sprintf(board_name, "AT+NAMET3-Node-%d", device_id);
  sendClean(board_name);
}

void sendClean(const char* command) {
  // Clear unread bytes
  while (btSerial.available()) btSerial.read();

  // Send command
  btSerial.print(command);

  delay(100);
}

void reset() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}