#include <Wire.h>
#include "light_sensor.h"
#include "Gas_sensor.h"
#include "buzzer_sensor.h"
#include "humidity_sensor.h"
#include "flame_sensor.h"
#include "LED_array.h"
#include "sound_sensor.h"
#include "vibration_sensor.h"
#include "people_counting.h"
#include <avr/wdt.h>

// Increase RX buffer size to fit all responses without overwrite
#define _SS_MAX_RX_BUFF 256
#include <SoftwareSerial.h>

#define AT_RESPONSE_DELAY 100
#define AT_ASYNC_RESPONSE_DELAY 500

void sendClean(const char* command, bool blocking_delay = false);

const uint16_t NODES_COUNT = 2;
const String MESH_NODES[NODES_COUNT] = {"685E1C1A68CF", "685E1C1A5A30"};

typedef float float32_t;

SoftwareSerial bt_serial(10, 9);

typedef struct {
  uint16_t device_id;
  uint16_t timestamp;
} DataId;


#define GAS_WARNING_THRESHOLD 200     // ppm
#define GAS_EMERGENCY_THRESHOLD 200   // ppm

#define SOUND_WARNING_THRESHOLD 10     // dB (adjust as per your sound sensor)

typedef enum {
  SYSTEM_NORMAL,
  SYSTEM_WARNING,
  SYSTEM_EMERGENCY
} SystemState;

SystemState current_state = SYSTEM_NORMAL;

bool emergency_flag = false;


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
  float32_t eCO2_level;
  float32_t TVOC_level;
  uint16_t light_level;
  uint16_t noise_level;
  uint16_t vibration_level;
  uint16_t brightness_level;
  uint16_t counter;
  bool flame_detected;
  bool earthquake_detected;
  AlarmStatus alarm_state;
} DataEntry;

typedef enum {
  BLE_STATE_MACHINE_IDLE,
  BLE_STATE_MACHINE_START_MASTER,
  BLE_STATE_MACHINE_DISCOVERY_STATE,
  
  BLE_STATE_MACHINE_MASTER_CONNECT,
  BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES,
  BLE_STATE_MACHINE_PUSH_REMOTE_MISSING,
  BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING,

  BLE_STATE_MACHINE_START_SLAVE,

  BLE_STATE_MACHINE_CONNECT_1,

  BLE_STATE_MACHINE_DEBUG,
} BleStateMachine;

static DataEntry mesh_node_data_buffer[20];
DataEntry aggregation_data_entry;
bool is_first_read = true;
uint16_t device_id = 0; // Update to BLE module last 2 bytes of MAC

String panic_error = "";

BleStateMachine ble_state_machine = BLE_STATE_MACHINE_IDLE;
uint16_t ble_state_machine_time = 0;

void setup() {
  Serial.begin(9600);
  bt_serial.begin(9600);

  // initBleModule();

  Serial.println("Finished initializing BLE");
  Serial.print("Current Device ID: ");
  Serial.println(device_id);

  
  initLightSensor();
  initBuzzer();
  initAirQuality();
  // initFlameSensor(); // doesn't work
  initHumidityTemperatureSensor();
  initLedArray();
  // initVibrationSensor(); / doesn't work
  initSoundSensor();

  readMatrix();
  initInfraredCamera();
  initUltrasonicSensors();
  
}

bool a = false;
void loop() {


////////////////////////////////////////////////////////////////7
//      Serial.println("Reading values from sensors");
//   //  delay(1000);
//    readSensorsData();
//   //  delay(1000);
//    printAggregatedData();
//   // delay(1000);  

//////////////////////////////////////////////////7
// ultrasound and thermal camera part
    counterSysSTM();
    delay(50); 

  ///////////////////////////////////////////////
  // EMERGENCY PART

//   switch (current_state) {
//     case SYSTEM_NORMAL:
//       if (isWarningCondition()) {
//         Serial.println("Switching to WARNING state.");
//         current_state = SYSTEM_WARNING;
//       }
//       break;

//     case SYSTEM_WARNING:
//       displayWarningLights();

//       if (isEmergencyCondition()) {
//         Serial.println("Switching to EMERGENCY state.");
//         emergency_flag = true;
//         current_state = SYSTEM_EMERGENCY;
//       } else if (!isWarningCondition()) {
//         Serial.println("Warning cleared — returning to NORMAL.");
//         current_state = SYSTEM_NORMAL;
//       }
//       break;

//     case SYSTEM_EMERGENCY:
//       activateEmergencyPlan();

//       if (isSafeToReturn()) {
//         Serial.println("Emergency resolved — returning to NORMAL.");
//         emergency_flag = false;
//         current_state = SYSTEM_NORMAL;
//       }
//       break;
//   }

//   delay(1000); // Limit loop frequency

  ////////////////////////////////////////77777

  // Serial.println("HEyYY");
  //readSensorsData();
  // Serial.println("2");
  // printAggregatedData();
  // Serial.println("3");

  // if (panic_error.length() > 0) {
  //   Serial.print("Detected global error (resetting): ");
  //   Serial.println(panic_error);
  //   Serial.println();
  //   Serial.flush();
  //   delay(1000);
  //   reset();
  // }

  // switch (ble_state_machine) {
  //   case BLE_STATE_MACHINE_IDLE:
  //     Serial.println("Starting master");

  //     sendClean("AT+ROLE1");
  //     ble_state_machine = BLE_STATE_MACHINE_START_MASTER;
  //     ble_state_machine_time = millis();

  //     break;
  //   case BLE_STATE_MACHINE_START_MASTER: {
  //     if (millis() - ble_state_machine_time < 3000) break;
  //     String role_change_response = readBtResponse();
  //     if (role_change_response != "OK+Set:1") {
  //       panic_error = String("Was not able to change BLE module role to master: ") + role_change_response;
  //       return;
  //     }
      
  //     Serial.println("Configured master, starting discovery");
  //     sendClean("AT+DISC?");

  //     ble_state_machine = BLE_STATE_MACHINE_DISCOVERY_STATE;
  //     ble_state_machine_time = millis();
  //     break;
  //   }
  //   case BLE_STATE_MACHINE_DISCOVERY_STATE: {
  //     if (millis() - ble_state_machine_time < 10000) break;
  //     String ble_discovery_response = readBtResponse();      

  //     Serial.print("Received discovery response: ");
  //     Serial.println(ble_discovery_response);

  //     ble_state_machine = BLE_STATE_MACHINE_DEBUG;

  //     break; 
  //   }

  //   case BLE_STATE_MACHINE_MASTER_CONNECT:
  //     ble_state_machine = BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES;
  //     ble_state_machine_time = millis();

  //     break;
  //   case BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES:
  //     ble_state_machine = BLE_STATE_MACHINE_PUSH_REMOTE_MISSING;
  //     ble_state_machine_time = millis();

  //     break;
  //   case BLE_STATE_MACHINE_PUSH_REMOTE_MISSING:
  //     ble_state_machine = BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING;
  //     ble_state_machine_time = millis();

  //     break;
  //   case BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING:

  //     ble_state_machine = BLE_STATE_MACHINE_MASTER_CONNECT;
  //     ble_state_machine_time = millis();

  //     break;

  //   case BLE_STATE_MACHINE_DEBUG:
  //     processSerialBle();
  //     break;
  //   default:
  //     break;
  // }

  


// void processSerialBle() {
//   if (Serial.available()) {
//     String message = Serial.readString();
//     bt_serial.print(message);

//     Serial.print("> ");
//     Serial.println(message);
//   }

//   if (btSerial.available()) {
//     String message = btSerial.readString();
//     Serial.println(message);
//   }

  
//   if (panic_error.length() > 0) {
//     Serial.print("Detected global error (resetting): ");
//     Serial.println(panic_error);
//     Serial.println();
//     Serial.flush();
//     delay(1000);
//     reset();
//   }

//   switch (ble_state_machine) {
//     case BLE_STATE_MACHINE_IDLE:
//       Serial.println("Starting master");

//       sendClean("AT+ROLE1");
//       ble_state_machine = BLE_STATE_MACHINE_START_MASTER;
//       ble_state_machine_time = millis();

//       break;
//     case BLE_STATE_MACHINE_START_MASTER: {
//       if (millis() - ble_state_machine_time < 3000) break;
//       String role_change_response = readBtResponse();
//       if (role_change_response != "OK+Set:1") {
//         panic_error = String("Was not able to change BLE module role to master: ") + role_change_response;
//         return;
//       }
      
//       Serial.println("Configured master, starting discovery");
//       sendClean("AT+DISC?");

//       ble_state_machine = BLE_STATE_MACHINE_DISCOVERY_STATE;
//       ble_state_machine_time = millis();
//       break;
//     }
//     case BLE_STATE_MACHINE_DISCOVERY_STATE: {
//       if (millis() - ble_state_machine_time < 10000) break;
//       String ble_discovery_response = readBtResponse();      

//       Serial.print("Received discovery response: ");
//       Serial.println(ble_discovery_response);

//       ble_state_machine = BLE_STATE_MACHINE_DEBUG;

//       break; 
//     }

//     case BLE_STATE_MACHINE_MASTER_CONNECT:
//       ble_state_machine = BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES;
//       ble_state_machine_time = millis();

//       break;
//     case BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES:
//       ble_state_machine = BLE_STATE_MACHINE_PUSH_REMOTE_MISSING;
//       ble_state_machine_time = millis();

//       break;
//     case BLE_STATE_MACHINE_PUSH_REMOTE_MISSING:
//       ble_state_machine = BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING;
//       ble_state_machine_time = millis();

//       break;
//     case BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING:

//       ble_state_machine = BLE_STATE_MACHINE_MASTER_CONNECT;
//       ble_state_machine_time = millis();

//       break;

//     case BLE_STATE_MACHINE_DEBUG:
//       processSerialBle();
//       break;
//     default:
//       break;
//   }

  
 }

// void processSerialBle() {
//   if (Serial.available()) {
//     String message = Serial.readString();
//     bt_serial.print(message);

//     Serial.print("> ");
//     Serial.println(message);
//   }

//   if (bt_serial.available()) {
//     String message = bt_serial.readString();
//     Serial.println(message);
//   }
// }

// void initBleModule() {
//   sendClean("AT", true);
//   sendClean("AT+RENEW", true);
//   sendClean("AT+IMME1", true);
//   sendClean("AT+CLEAR", true);
//   sendClean("AT+ROLE1", true);
//   sendClean("AT+SHOW0", true);

//   sendClean("AT+ADDR?", true);
//   String address_response = readBtResponse();
//   if (!address_response.startsWith("OK+ADDR:")) {
//     panic_error = String("BLE initialization failed. Invalid response for address request: ") + address_response;
//     return;
//   }
    
//   char* end_pointer;
//   long mac_result = strtol(&address_response[15], &end_pointer, 16);

//   if (end_pointer == address_response[20]) {
//     Serial.println("Error: No conversion performed");
//     return 0;
//   }

//   device_id = mac_result;

//   char board_name[128];
//   sprintf(board_name, "AT+NAMET3-Node-%d", device_id);
//   sendClean(board_name);
  
//   delay(AT_ASYNC_RESPONSE_DELAY);
// }

// void sendClean(const char* command, bool blocking_delay = false) {
//   // Clear unread bytes
//   while (bt_serial.available()) bt_serial.read();

//   // Send command
//   bt_serial.print(command);

//   if (blocking_delay) delay(AT_RESPONSE_DELAY);
// }

// String readBtResponse() {
//   String message = "";
//   while (bt_serial.available()) message += (char)bt_serial.read();

//   return message;
// }

// void reset() {
//   wdt_disable();
//   wdt_enable(WDTO_15MS);
//   while (1) {}
// }


void readSensorsData() {
  readAirQuality(&aggregation_data_entry.eCO2_level, &aggregation_data_entry.TVOC_level);
  readLightLevel(&aggregation_data_entry.light_level);
  readFlameSensor(&aggregation_data_entry.flame_detected);
  readHumiditySensor(&aggregation_data_entry.humidity_level);
  readTemperatureSensor(&aggregation_data_entry.temperature);
  readSoundLevel(&aggregation_data_entry.noise_level);
  readVibrationSensor(&aggregation_data_entry.earthquake_detected);
  delay(1000);
  //@ToDo - measure the time needed for all readings
}


void printAggregatedData(){
  Serial.print("light level: "); Serial.println(aggregation_data_entry.light_level);
  Serial.print("TVOC level: "); Serial.println(aggregation_data_entry.TVOC_level);
  Serial.print("eCO2 level: "); Serial.println(aggregation_data_entry.eCO2_level);
  Serial.print("flame_detected: "); Serial.println(aggregation_data_entry.flame_detected);
  Serial.print("humidity_level: "); Serial.println(aggregation_data_entry.humidity_level);
  Serial.print("temperature: "); Serial.println(aggregation_data_entry.temperature);
  Serial.print("noise_level: "); Serial.println(aggregation_data_entry.noise_level);
  Serial.print("earthquake_detected: "); Serial.println(aggregation_data_entry.earthquake_detected);
}

bool isWarningCondition() {
  return (aggregation_data_entry.eCO2_level > GAS_WARNING_THRESHOLD ||
          aggregation_data_entry.noise_level > SOUND_WARNING_THRESHOLD);
}

bool isEmergencyCondition() {
  return (aggregation_data_entry.flame_detected || emergency_flag);
}

bool isSafeToReturn() {
  return (!aggregation_data_entry.flame_detected &&
          !isWarningCondition());
}

void displayWarningLights() {
  Serial.println("⚠️ Warning: Gas or sound levels high — blinking orange light.");
  orangeSlowlyBlinkingLight();
}

void activateEmergencyPlan() {
  Serial.println("🚨 Emergency: Fire or critical condition — activating red light and alarm.");
  redBlinkingLight();
  playAlarmSound();
}