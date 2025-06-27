#include <Wire.h>
#include "buzzer_sensor.h"
#include "humidity_sensor.h"
#include "flame_sensor.h"
#include "LED_array.h"
#include <avr/wdt.h>

// Increase RX buffer size to fit all responses without overwrite
#define _SS_MAX_RX_BUFF 256
#include <SoftwareSerial.h>

#define AT_RESPONSE_DELAY 100
#define AT_LONG_RESPONSE_DELAY 1000

void sendClean(const char* command, bool blocking_delay = false);

const uint16_t NODES_COUNT = 2;
const String MESH_NODES[NODES_COUNT] = {"685E1C1A68CF", "685E1C1A5A30"};

typedef float float32_t;

SoftwareSerial bt_serial(10, 9);

typedef struct {
  uint16_t device_id;
  uint16_t counter;
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
  BLE_STATE_MACHINE_IDLE,

  BLE_STATE_MACHINE_SEND_AT,
  BLE_STATE_MACHINE_WAIT_AT_RESPONSE,
  
  // Master flow
  BLE_STATE_MACHINE_INIT_MASTER,
  BLE_STATE_MACHINE_START_MASTER,
  BLE_STATE_MACHINE_DISCOVERY_STATE,
  
  BLE_STATE_MACHINE_MASTER_INIT_CONNECT,
  BLE_STATE_MACHINE_MASTER_CONNECT,
  BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES,
  BLE_STATE_MACHINE_PUSH_REMOTE_MISSING,
  BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING,

  // Slave flow
  BLE_STATE_MACHINE_INIT_SLAVE,
  BLE_STATE_MACHINE_START_SLAVE,
  BLE_STATE_MACHINE_INIT_ADVERTISMENT,
  BLE_STATE_MACHINE_START_ADVERTISMENT,
  BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION,

  BLE_STATE_MACHINE_CONNECT_1,

  BLE_STATE_MACHINE_DEBUG,
} BleStateMachine;

static DataEntry mesh_node_data_buffer[3];
DataEntry aggregation_data_entry;
bool is_first_read = true;
uint16_t device_id = 0; // Update to BLE module last 2 bytes of MAC

String panic_error = "";

BleStateMachine ble_state_machine = BLE_STATE_MACHINE_IDLE;
uint16_t ble_state_machine_time = 0;
String ble_response = "";
uint16_t ble_discovered_nodes_ids[NODES_COUNT] = {-1};

uint16_t slave_duration = 0;
uint16_t slave_start_time = 0;
BleStateMachine before_send_at_procedure = BLE_STATE_MACHINE_IDLE;

void setup() {
  Serial.begin(9600);
  bt_serial.begin(9600);
  randomSeed(analogRead(0));

  initBleModule();

  Serial.println("Finished initializing BLE");
  Serial.print("Current Device ID: ");
  Serial.println(device_id);
}

bool a = false;
void loop() {

  // Serial.print("panic_error: ");
  // Serial.println(panic_error);
  // delay(100);
  if (panic_error.length() > 0) {
    Serial.print("Detected global error (resetting): ");
    Serial.println(panic_error);
    Serial.println();
    Serial.flush();
    delay(1000);
    reset();
  }

  switch (ble_state_machine) {
    case BLE_STATE_MACHINE_IDLE: {
      sendAtCommand(device_id & 0x01 ? BLE_STATE_MACHINE_INIT_MASTER : BLE_STATE_MACHINE_INIT_SLAVE);
      // sendAtCommand(BLE_STATE_MACHINE_INIT_MASTER);
      break;
    }
    case BLE_STATE_MACHINE_SEND_AT: {
      if (getStateMachineMillis() < AT_LONG_RESPONSE_DELAY) break;

      Serial.println("Sending AT command");
      sendClean("AT");

      updateStateMachine(BLE_STATE_MACHINE_WAIT_AT_RESPONSE);
      break;
    }
    case BLE_STATE_MACHINE_WAIT_AT_RESPONSE: {
      if (!ble_response.startsWith("OK")) {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          panic_error = "AT command timeout, did't receive OK";
          return;
        }     

        loadBleBytes();
        break;
      }

      Serial.println("Succesfully received AT response, returning");
      updateStateMachine(before_send_at_procedure);
      break;
    }
    case BLE_STATE_MACHINE_INIT_MASTER: {
      Serial.println("Starting master");
      sendClean("AT+ROLE1");

      updateStateMachine(BLE_STATE_MACHINE_START_MASTER);
      break;
    }
    case BLE_STATE_MACHINE_START_MASTER: {
      if (ble_response != "OK+Set:1") {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          panic_error = "Failed to change BLE module to master";
          return;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 500) break;

      Serial.println("Configured master, starting discovery");
      sendClean("AT+DISC?");

      updateStateMachine(BLE_STATE_MACHINE_DISCOVERY_STATE);
      break;
    }
    case BLE_STATE_MACHINE_DISCOVERY_STATE: {
      if (!ble_response.endsWith("OK+DISCE")) {
        if (getStateMachineMillis() > 15000) {
          panic_error = "Discovery timeout, did not receive OK+DISCE";
          return;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 500) break;

      Serial.print("Received discovery response: ");
      Serial.println(ble_response);

      for (int i = 0; i < NODES_COUNT; i++) ble_discovered_nodes_ids[i] = -1;

      uint16_t discovered_count = 0;
      for (uint16_t i = 0; i < NODES_COUNT; i++) {
        const String *current_node = &MESH_NODES[i];

        if (ble_response.indexOf(*current_node) == -1) continue;

        uint16_t j = 0;
        while (ble_discovered_nodes_ids[j] != -1) j++;

        discovered_count++;
        ble_discovered_nodes_ids[j] = i;
      }

      // Shuffle discovered ids array
      for (uint16_t i = 0; i < NODES_COUNT; i++)
      {
        uint16_t j = random(NODES_COUNT);

        uint16_t temp = ble_discovered_nodes_ids[i];
        ble_discovered_nodes_ids[i] = ble_discovered_nodes_ids[j];
        ble_discovered_nodes_ids[j] = temp;
      }

      Serial.print("Discovered mesh nodes: ");
      Serial.println(discovered_count);

      updateStateMachine(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
      break; 
    }
    case BLE_STATE_MACHINE_MASTER_INIT_CONNECT: {
      uint16_t i = 0;
      while (i < NODES_COUNT && ble_discovered_nodes_ids[i] == -1) i++;

      // Check there was left one more device to connect to
      if (i >= NODES_COUNT) {
        Serial.println("No more discovered nodes, finishing master phase");
        updateStateMachine(BLE_STATE_MACHINE_INIT_SLAVE);
        break;
      }

      const String *device_mac = &MESH_NODES[ble_discovered_nodes_ids[i]];
      ble_discovered_nodes_ids[i] = -1;

      Serial.print("Attempting to connect to the: ");
      Serial.println(*device_mac);

      String connection_command = "AT+CON";
      connection_command += *device_mac;
      sendClean(connection_command.c_str());

      updateStateMachine(BLE_STATE_MACHINE_MASTER_CONNECT);
      break;
    }
    case BLE_STATE_MACHINE_MASTER_CONNECT: {
      if (!ble_response.startsWith("OK+CONNAOK+CONN")) {
        if (getStateMachineMillis() > 15000) {
          panic_error = "Connection timeout, did not receive OK+CONNAOK+CONN";
          return;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 100) break;

      // Detect F or E in case there were present
      delay(5);
      loadBleBytes();

      // In case connection failed, ignore and move on to next node
      if (ble_response != "OK+CONNAOK+CONN") {
        Serial.println("Failed to connect to node, continuing");
        updateStateMachine(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      Serial.println("Succesfully connected to node");
      updateStateMachine(BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES);
      break;
    }
    case BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES: {
      updateStateMachine(BLE_STATE_MACHINE_PUSH_REMOTE_MISSING);
      break;
    }      
    case BLE_STATE_MACHINE_PUSH_REMOTE_MISSING: {
      startDebugProcedure();
      break;

      updateStateMachine(BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING);
      break;
    }
    case BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING: {
      updateStateMachine(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
      break;
    }

    // SLAVE LOGIC

    case BLE_STATE_MACHINE_INIT_SLAVE: {
      Serial.println("Starting slave");
      sendClean("AT+ROLE0");

      updateStateMachine(BLE_STATE_MACHINE_START_SLAVE);
      break;
    }
    case BLE_STATE_MACHINE_START_SLAVE: {
      if (ble_response != "OK+Set:0") {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          panic_error = "Was not able to set slave role";
          return;
        }     

        loadBleBytes();
        break;
      }

      slave_duration = 15000 + random(15000);
      Serial.print("Configured slave, current phase duration: ");
      Serial.println(slave_duration);
      
      sendAtCommand(BLE_STATE_MACHINE_INIT_ADVERTISMENT);
      slave_start_time = millis();
      break;
    }
    case BLE_STATE_MACHINE_INIT_ADVERTISMENT: {
      Serial.println("Starting advertisment");
      sendClean("AT+START");

      updateStateMachine(BLE_STATE_MACHINE_START_ADVERTISMENT);
      break;
    }
    case BLE_STATE_MACHINE_START_ADVERTISMENT: {
      if (ble_response != "OK+START") {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          panic_error = "Failed to start advertising";
          return;
        }     

        loadBleBytes();
        break;
      }

      Serial.println("Sucesfully started advertising, waiting for connections");
      updateStateMachine(BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION);
      break;
    }
    case BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION: {
      // Check if slave phase has ended
      if (millis() - slave_start_time >= slave_duration) {
        startDebugProcedure();
        // sendAtCommand(BLE_STATE_MACHINE_INIT_MASTER)
        break;
      }



      // startDebug();
      // break;

      break;
    }
    case BLE_STATE_MACHINE_DEBUG: {
      processSerialBle();

      break;
    }

    default:
      break;
  }  
}

uint16_t getStateMachineMillis() {
  return millis() - ble_state_machine_time;
}

void sendAtCommand(BleStateMachine callback_procedure) {
  before_send_at_procedure = callback_procedure;
  updateStateMachine(BLE_STATE_MACHINE_SEND_AT);
}

void updateStateMachine(BleStateMachine new_procedure) {
  ble_response = "";
  ble_state_machine = new_procedure;
  ble_state_machine_time = millis();
}

void loadBleBytes() {
  while (bt_serial.available()) ble_response += (char)bt_serial.read();
}

void startDebugProcedure() {
  Serial.println("Starting debug logic...");
  updateStateMachine(BLE_STATE_MACHINE_DEBUG);
}

void processSerialBle() {
  if (Serial.available()) {
    String message = Serial.readString();
    bt_serial.print(message);

    Serial.print("> ");
    Serial.println(message);
  }

  if (bt_serial.available()) {
    String message = bt_serial.readString();
    Serial.println(message);
  }
}

void initBleModule() {
  sendClean("AT", true);
  if (readBtResponse() != "OK")
    return panic_error = "Failed to execute AT";

  sendClean("AT+RENEW", true);
  if (readBtResponse() != "OK+RENEW")
    return panic_error = "Failed to execute AT+RENEW";

  sendClean("AT+IMME1", true);
  if (readBtResponse() != "OK+Set:1")
    return panic_error = "Failed to execute AT+IMME1";

  sendClean("AT+CLEAR", true);
  if (readBtResponse() != "OK+CLEAR")
    return panic_error = "Failed to execute AT+CLEAR";

  sendClean("AT+ROLE1", true);
  if (readBtResponse() != "OK+Set:1")
    return panic_error = "Failed to execute AT+ROLE1";

  sendClean("AT+SHOW0", true);
  if (readBtResponse() != "OK+Set:0")
    return panic_error = "Failed to execute AT+SHOW0";

  sendClean("AT+ADDR?", true);
  String address_response = readBtResponse();
  if (!address_response.startsWith("OK+ADDR:")) {
    panic_error = String("Invalid AT response for address request");
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
  sendClean(board_name, true);
  if (!readBtResponse().startsWith("OK+Set:"))
    return panic_error = "Failed to execute AT+SHOW0";
  
  delay(300);
}

void sendClean(const char* command, bool blocking_delay = false) {
  // Clear unread bytes
  while (bt_serial.available()) bt_serial.read();

  // Send command
  bt_serial.print(command);

  if (blocking_delay) delay(AT_RESPONSE_DELAY);
}

String readBtResponse() {
  String message = "";
  while (bt_serial.available()) message += (char)bt_serial.read();

  return message;
}

void reset() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}