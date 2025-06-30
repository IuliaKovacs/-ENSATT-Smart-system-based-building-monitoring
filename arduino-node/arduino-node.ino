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
#define AT_LONG_RESPONSE_DELAY 1300

#define MESH_BUFFER_SIZE 10
#define CONNECTION_TIMEOUT 10000
#define READINGS_INTERVAL 6000
#define MESH_COMMAND_DATA_SIZE sizeof(DataId) * MESH_BUFFER_SIZE + 3

const uint16_t NODES_COUNT = 2;
const String MESH_NODES[NODES_COUNT] = {"685E1C1A68CF", "685E1C1A5A30"};
const uint16_t MAX_READINGS_PER_NODE = MESH_BUFFER_SIZE / NODES_COUNT;
uint16_t CURRENT_NODE_IND = -1;
uint16_t readings_counter = 0;

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
  OK = 'O',
  ERROR = 'E',
  UNKNOWN = -1
} BleMeshCommandResult;

// All commands return following format:
// BleMeshCommandResult;<command-response>\n
typedef enum {
  BLE_MESH_COMMAND_UNKNOWN = -1,

  // Parameters: void
  // Returns: void
  BLE_MESH_COMMAND_OK = 'O',

  // Parameters: void
  // Returns: <number of entries>, <array of DataId from mesh_node_data_buffer>
  BLE_MESH_COMMAND_LIST_DATA_ENTRIES = 'L',

  // Parameters: DataId
  // Returns: DataEntry
  BLE_MESH_COMMAND_GET_DATA_ENTRY = 'G',

  // Parameters: DataEntry
  // Returns: void
  BLE_MESH_COMMAND_PUSH_DATA_ENTRY = 'P',
} BleMeshCommand;

typedef struct {
  DataId id;
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
  BLE_STATE_MACHINE_PUSH_REMOTE_MISSING_RESPONSE,
  BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING,
  BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING_RESPONSE,

  // Slave flow
  BLE_STATE_MACHINE_INIT_SLAVE,
  BLE_STATE_MACHINE_START_SLAVE,
  BLE_STATE_MACHINE_INIT_ADVERTISMENT,
  BLE_STATE_MACHINE_START_ADVERTISMENT,
  BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION,
  BLE_STATE_MACHINE_SLAVE_MESH_COMMAND_HANDLER,

  BLE_STATE_MACHINE_CONNECT_1,

  BLE_STATE_MACHINE_DEBUG,
} BleStateMachine;

void sendClean(const char* command, bool blocking_delay = false);
void updateStateMachine(BleStateMachine new_procedure, bool clear_response = true);

static DataEntry mesh_node_data_buffer[MESH_BUFFER_SIZE];
DataEntry aggregation_data_entry;
bool is_first_read = true;
uint16_t device_id = 0; // Update to BLE module last 2 bytes of MAC

String panic_error = "";

BleStateMachine ble_state_machine = BLE_STATE_MACHINE_IDLE;
uint32_t ble_state_machine_time = 0;
String ble_response = "";
uint16_t ble_discovered_nodes_ids[NODES_COUNT] = {-1};

uint32_t slave_duration = 0;
uint32_t slave_start_time = 0;
uint32_t connection_start_time = 0;
BleStateMachine before_send_at_procedure = BLE_STATE_MACHINE_IDLE;

BleMeshCommand mesh_command = BLE_MESH_COMMAND_UNKNOWN;
// Big enough to hold whole mesh list command response
// shoube be bigger then sizeof(DataEntry)
uint8_t mesh_command_data[MESH_COMMAND_DATA_SIZE] = {-1};
const DataId *mesh_list_response = mesh_command_data[3];
uint16_t mesh_command_data_size = 0;
uint16_t mesh_master_list_response_length = 0;
uint16_t mesh_master_counter = 0;

uint8_t valid_remote_response = 0;
uint8_t remote_downloaded_entry[sizeof(DataEntry)] = {0};
uint32_t last_registered_reading = 0;

void setup() {
  Serial.begin(9600);
  bt_serial.begin(9600);
  randomSeed(analogRead(0));

  initBleModule();

  Serial.println("BLE OK");
  Serial.print("ID: ");
  Serial.println(device_id);
}

void loop() {

  if (panic_error.length() > 0) {
    Serial.print("G ERR: ");
    Serial.println(panic_error);
    Serial.println();
    Serial.flush();
    delay(1000);
    reset();
  }

  switch (ble_state_machine) {
    case BLE_STATE_MACHINE_IDLE: {
      if (getStateMachineMillis() < 1000) break;
      
      sendAtCommand(device_id & 1 ? BLE_STATE_MACHINE_INIT_MASTER : BLE_STATE_MACHINE_INIT_SLAVE);
      // sendAtCommand(BLE_STATE_MACHINE_INIT_MASTER);
      break;
    }
    case BLE_STATE_MACHINE_SEND_AT: {
      if (getStateMachineMillis() < 1000) break;

      Serial.println("AT S");
      sendClean("AT");

      updateStateMachine(BLE_STATE_MACHINE_WAIT_AT_RESPONSE);
      break;
    }
    case BLE_STATE_MACHINE_WAIT_AT_RESPONSE: {
      if (!ble_response.startsWith("OK")) {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          Serial.print("AT E");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }

      Serial.println("AT O");
      updateStateMachine(before_send_at_procedure);
      break;
    }
    case BLE_STATE_MACHINE_INIT_MASTER: {
      Serial.println("MASTER S");
      sendClean("AT+ROLE1");

      updateStateMachine(BLE_STATE_MACHINE_START_MASTER);
      break;
    }
    case BLE_STATE_MACHINE_START_MASTER: {
      if (ble_response != "OK+Set:1") {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          Serial.print("MASTER S E");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 1000) break;

      Serial.println("MASTER O");
      sendClean("AT+DISC?");

      updateStateMachine(BLE_STATE_MACHINE_DISCOVERY_STATE);
      break;
    }
    case BLE_STATE_MACHINE_DISCOVERY_STATE: {
      if (!ble_response.endsWith("OK+DISCE")) {
        if (getStateMachineMillis() > 15000) {
          // panic_error = "Failed discovery";
          Serial.print("DISC T");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 1000) break;

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

      Serial.print("DISC: ");
      Serial.println(discovered_count);

      updateStateMachine(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
      break; 
    }
    case BLE_STATE_MACHINE_MASTER_INIT_CONNECT: {
      if (getStateMachineMillis() < 1000) break;

      uint16_t new_node_ind = 0;
      while (new_node_ind < NODES_COUNT && ble_discovered_nodes_ids[new_node_ind] == -1) new_node_ind++;

      // Check there was left one more device to connect to
      if (new_node_ind >= NODES_COUNT) {
        Serial.println("MASTER F");
        updateStateMachine(BLE_STATE_MACHINE_INIT_SLAVE);
        break;
      }

      const String *device_mac = &MESH_NODES[ble_discovered_nodes_ids[new_node_ind]];
      ble_discovered_nodes_ids[new_node_ind] = -1;

      Serial.print("CON M: ");
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
          Serial.print("CON M T");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 300) break;

      // Detect F or E in case there were present
      delay(5);
      loadBleBytes();

      // In case connection failed, ignore and move on to next node
      if (ble_response != "OK+CONNAOK+CONN") {
        Serial.println("CON M F");
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      Serial.println("CON M O");
      connection_start_time = millis();

      bt_serial.write(BLE_MESH_COMMAND_LIST_DATA_ENTRIES);
      mesh_command_data_size = 0;
      mesh_master_list_response_length = -1;

      updateStateMachine(BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES);
      break;
    }
    case BLE_STATE_MACHINE_QUERY_REMOTE_ENTRIES: {
      if (millis() - connection_start_time > CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      if (mesh_command_data_size < 3) {
        loadMeshData(); 
        break;
      }
      if (mesh_command_data[0] != OK) {
        Serial.println("MESH L E");
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      if (mesh_master_list_response_length == -1) {
        mesh_master_list_response_length = *(uint16_t*)&mesh_command_data[1];
        Serial.println(mesh_master_list_response_length);
      }

      if (mesh_command_data_size < mesh_master_list_response_length * sizeof(DataId) + 3) {
        loadMeshData();
        break;
      }

      Serial.print("MESH L: ");
      Serial.println(mesh_master_list_response_length);

      mesh_master_counter = 0;

      updateStateMachine(BLE_STATE_MACHINE_PUSH_REMOTE_MISSING);
      break;
    }      
    case BLE_STATE_MACHINE_PUSH_REMOTE_MISSING: {
      if (millis() - connection_start_time > CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      if (mesh_master_counter >= MESH_BUFFER_SIZE) {
        mesh_master_counter = 0;
        Serial.print("G S");
        updateStateMachine(BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING);
        break;
      }

      const DataEntry *local_entry = &mesh_node_data_buffer[mesh_master_counter++];
      if (local_entry->id.device_id == 0) break;

      for (uint16_t i = 0; i < mesh_master_list_response_length; i++) {
        // Detected missing entry
        if (mesh_list_response[i].device_id != local_entry->id.device_id &&
              mesh_list_response[i].counter != local_entry->id.counter) {

          Serial.print("MESH P: ");
          Serial.print(local_entry->id.device_id);
          Serial.print(":");
          Serial.println(local_entry->id.counter);

          bt_serial.write(BLE_MESH_COMMAND_PUSH_DATA_ENTRY);
          bt_serial.write((uint8_t*)local_entry, sizeof(DataEntry));

          updateStateMachine(BLE_STATE_MACHINE_PUSH_REMOTE_MISSING_RESPONSE);
          break;
        }
      }
      break;
    }
    case BLE_STATE_MACHINE_PUSH_REMOTE_MISSING_RESPONSE: {
      if (millis() - connection_start_time > CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      if (!bt_serial.available()) break;

      if (bt_serial.read() == OK) Serial.println("MESH P O");
      else Serial.println("MESH P E");

      updateStateMachine(BLE_STATE_MACHINE_PUSH_REMOTE_MISSING);
      break;
    }
    case BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING: {
      if (millis() - connection_start_time > CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      if (mesh_master_counter >= mesh_master_list_response_length) {
        mesh_master_counter = 0;
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      const DataId *remote_id = &mesh_list_response[mesh_master_counter++];

      for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++) {
        const DataEntry *local_entry = &mesh_node_data_buffer[i];
        if (local_entry->id.device_id == 0) continue;

        // Detected missing entry
        if (local_entry->id.device_id != remote_id->device_id &&
              local_entry->id.counter != remote_id->counter) {
          
          while (bt_serial.available()) bt_serial.read();

          bt_serial.write(BLE_MESH_COMMAND_GET_DATA_ENTRY);
          bt_serial.write((uint8_t*)remote_id, sizeof(DataId));

          Serial.println("Requesting download");

          mesh_command_data_size = 0;
          valid_remote_response = -1;
          updateStateMachine(BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING_RESPONSE);
          break;
        }
      }

      break;
    }
    case BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING_RESPONSE: {
      if (millis() - connection_start_time > CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_MASTER_INIT_CONNECT);
        break;
      }

      Serial.println(mesh_command_data_size);
      if (valid_remote_response == -1) {
        if (!bt_serial.available()) break;

        valid_remote_response = bt_serial.read();

        Serial.print("G STATUS: ");
        Serial.println((char)valid_remote_response);

        if (valid_remote_response != OK) {
          Serial.println("MESH G E");
          updateStateMachine(BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING);
          break;
        }
      }

      // if (mesh_command_data_size < sizeof(DataEntry)) {
      //   while (bt_serial.available() && mesh_command_data_size < sizeof(DataEntry))
      //     remote_downloaded_entry[mesh_command_data_size++] = bt_serial.read();
      //   break;
      // }

      const DataEntry *new_entry = (DataEntry*)remote_downloaded_entry;
      updateLocalMeshState(new_entry);

      Serial.print("MESH G: ");
      Serial.print(new_entry->id.device_id);
      Serial.print(":");
      Serial.println(new_entry->id.counter);

      updateStateMachine(BLE_STATE_MACHINE_DOWNLOAD_REMOTE_MISSING);
      break;
    }

    // SLAVE LOGIC

    case BLE_STATE_MACHINE_INIT_SLAVE: {
      Serial.println("SLAVE S");
      sendClean("AT+ROLE0");

      updateStateMachine(BLE_STATE_MACHINE_START_SLAVE);
      break;
    }
    case BLE_STATE_MACHINE_START_SLAVE: {
      if (ble_response != "OK+Set:0") {
        if (getStateMachineMillis() > AT_LONG_RESPONSE_DELAY) {
          Serial.print("SLAVE S E");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }
      if (getStateMachineMillis() < 1000) break;

      slave_duration = 15000 + random(15000);
      Serial.print("SLAVE O: ");
      Serial.println(slave_duration);
      
      slave_start_time = millis();
      sendAtCommand(BLE_STATE_MACHINE_INIT_ADVERTISMENT);
      break;
    }
    case BLE_STATE_MACHINE_INIT_ADVERTISMENT: {
      if (getStateMachineMillis() < 1500) break;

      Serial.println("ADV S");
      sendClean("AT+START");

      updateStateMachine(BLE_STATE_MACHINE_START_ADVERTISMENT);
      break;
    }
    case BLE_STATE_MACHINE_START_ADVERTISMENT: {
      if (ble_response != "OK+START") {
        if (getStateMachineMillis() > 3000) {
          Serial.print("ADV S E");
          updateStateMachine(BLE_STATE_MACHINE_IDLE);
          break;
        }     

        loadBleBytes();
        break;
      }

      Serial.println("ADV O");
      updateStateMachine(BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION);
      break;
    }
    case BLE_STATE_MACHINE_SLAVE_WAIT_CONNECTION: {
      // Check if slave phase has ended
      if (millis() - slave_start_time > slave_duration) {
        sendAtCommand(BLE_STATE_MACHINE_INIT_MASTER);
        break;
      }

      if (ble_response.length() != 7) {
        // Read until obtain connection string
        if (bt_serial.available()) ble_response += (char)bt_serial.read();
        break;
      }

      if (ble_response != "OK+CONN") {
        Serial.print("CON S E");
        sendAtCommand(BLE_STATE_MACHINE_INIT_ADVERTISMENT);
        break;
      }

      Serial.println("CON S S");
      connection_start_time = millis();

      updateStateMachine(BLE_STATE_MACHINE_SLAVE_MESH_COMMAND_HANDLER);
      break;
    }
    case BLE_STATE_MACHINE_SLAVE_MESH_COMMAND_HANDLER: {
      if (millis() - connection_start_time >= CONNECTION_TIMEOUT) {
        sendAtCommand(BLE_STATE_MACHINE_INIT_ADVERTISMENT);
        break;
      }

      if (mesh_command == BLE_MESH_COMMAND_UNKNOWN) {
        if (!bt_serial.available()) break;

        mesh_command = bt_serial.read();
        mesh_command_data_size = 0;
      }

      switch (mesh_command) {
        case BLE_MESH_COMMAND_OK: {
          Serial.println("MESH O");
          bt_serial.write(OK);

          mesh_command = BLE_MESH_COMMAND_UNKNOWN;
          break;
        }
        case BLE_MESH_COMMAND_LIST_DATA_ENTRIES: {
          uint16_t valid_entries = 0;
          for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++)
            if (mesh_node_data_buffer[i].id.device_id != 0) valid_entries++;

          bt_serial.write(OK);
          bt_serial.write((uint8_t*)&valid_entries, 2);
          for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++)
            if (mesh_node_data_buffer[i].id.device_id != 0) bt_serial.write((uint8_t*)&mesh_node_data_buffer[i].id, sizeof(DataId));
          
          Serial.print("MESH L: ");
          Serial.println(valid_entries);

          mesh_command = BLE_MESH_COMMAND_UNKNOWN;
          break;
        }
        case BLE_MESH_COMMAND_GET_DATA_ENTRY: {          
          if (mesh_command_data_size < sizeof(DataId)) {
            loadMeshData();
            break;
          }

          const DataId *request_id = (const DataId*)mesh_command_data;
          bool have_found = false;

          for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++) {
            const DataEntry *local_entry = &mesh_node_data_buffer[i];

            if (request_id->device_id == local_entry->id.device_id &&
                  request_id->counter == local_entry->id.counter) {
              
              have_found = true;
              bt_serial.write(OK);
              bt_serial.write((uint8_t*)local_entry, sizeof(DataEntry));

              Serial.print("MESH G: ");
              Serial.print(local_entry->id.device_id);
              Serial.print(":");
              Serial.println(local_entry->id.counter);

              break;
            }
          }

          if (!have_found) {
            bt_serial.write(ERROR);
            Serial.println("G NOT FOUND");
          }
            
          Serial.println("Finished G");
          mesh_command = BLE_MESH_COMMAND_UNKNOWN;
          break;
        }
        case BLE_MESH_COMMAND_PUSH_DATA_ENTRY: {
          if (mesh_command_data_size < sizeof(DataEntry)) {
            loadMeshData();
            break;
          }

          const DataEntry *remote_entry = (const DataEntry*)mesh_command_data;
          updateLocalMeshState(remote_entry);

          Serial.print("MESH P: ");
          Serial.print(remote_entry->id.device_id);
          Serial.print(":");
          Serial.println(remote_entry->id.counter);

          bt_serial.write(OK);
          mesh_command = BLE_MESH_COMMAND_UNKNOWN;

          break;
        }

        default:

          bt_serial.write(ERROR);
          mesh_command = BLE_MESH_COMMAND_UNKNOWN;

          break;
      }

      break;
    }
    case BLE_STATE_MACHINE_DEBUG: {
      processSerialBle();

      break;
    }

    default:
      break;
  }  

  aggregateSensorsReadings();
  
  if (millis() - last_registered_reading > READINGS_INTERVAL) {
    Serial.println("R NEW READ");

    aggregation_data_entry.id.device_id = device_id;
    aggregation_data_entry.id.counter = readings_counter++;

    updateLocalMeshState(&aggregation_data_entry);

    last_registered_reading = millis();
  }
  
}

void aggregateSensorsReadings() {
  // TODO:
}

void loadMeshData() {
  while (bt_serial.available() && mesh_command_data_size < MESH_COMMAND_DATA_SIZE)
    mesh_command_data[mesh_command_data_size++] = bt_serial.read();
}

void updateLocalMeshState(DataEntry *new_entry) {
  uint16_t device_readings_distribution[NODES_COUNT] = {0};

  for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++) {
    const DataEntry *current_data_entry = &mesh_node_data_buffer[i];

    for (uint16_t node_ind = 0; node_ind < NODES_COUNT; node_ind++) {
      uint16_t node_device_id = getDeviceId(MESH_NODES[node_ind].c_str());

      if (current_data_entry->id.device_id == node_device_id)
        device_readings_distribution[node_ind]++;
    }
  }

  uint16_t new_entry_node_ind = -1;
  for (uint16_t i = 0; i < NODES_COUNT; i++) {
    if (getDeviceId(MESH_NODES[i].c_str()) == new_entry->id.device_id) {
      new_entry_node_ind = i;
      break;
    }
  }

  // Overwrite oldest device type entry
  if (device_readings_distribution[new_entry_node_ind] >= MAX_READINGS_PER_NODE) {
    uint16_t oldest_entry_ind = -1;
    uint16_t oldest_entry_counter = -1;

    for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++) {
      const DataEntry *current_data_entry = &mesh_node_data_buffer[i];

      if (current_data_entry->id.device_id != new_entry->id.device_id) continue;
      if (current_data_entry->id.counter >= oldest_entry_counter) continue;

      oldest_entry_ind = i;
      oldest_entry_counter = current_data_entry->id.counter;  
    }

    if (mesh_node_data_buffer[oldest_entry_ind].counter < new_entry->id.counter)
      mesh_node_data_buffer[oldest_entry_ind] = *new_entry;

    return;
  } 
  
  // Insert in empty slot
  for (uint16_t i = 0; i < MESH_BUFFER_SIZE; i++) {
    const DataEntry *current_data_entry = &mesh_node_data_buffer[i];

    if (current_data_entry->id.device_id == 0) {
      mesh_node_data_buffer[i] = *new_entry;
      break;
    }
  }
}

uint16_t getDeviceId(const char *mac_address) {
  char* end_pointer;
  uint16_t device_id = strtol(&mac_address[8], &end_pointer, 16);

  if (end_pointer == mac_address[12]) return -1;  

  return device_id;
}

uint16_t getStateMachineMillis() {
  return millis() - ble_state_machine_time;
}

void sendAtCommand(BleStateMachine callback_procedure) {
  before_send_at_procedure = callback_procedure;
  updateStateMachine(BLE_STATE_MACHINE_SEND_AT);
}

void updateStateMachine(BleStateMachine new_procedure, bool clear_response = true) {
  if (clear_response) ble_response = "";
  ble_state_machine = new_procedure;
  ble_state_machine_time = millis();
}

void loadBleBytes() {
  while (bt_serial.available()) ble_response += (char)bt_serial.read();
}

void startDebugProcedure() {
  Serial.println("#D#");
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
    return panic_error = "Failed AT";

  sendClean("AT+RENEW", true);
  if (readBtResponse() != "OK+RENEW")
    return panic_error = "Failed RENEW";

  sendClean("AT+IMME1", true);
  if (readBtResponse() != "OK+Set:1")
    return panic_error = "Failed IMME";

  sendClean("AT+CLEAR", true);
  if (readBtResponse() != "OK+CLEAR")
    return panic_error = "Failed CLEAR";

  sendClean("AT+ROLE1", true);
  if (readBtResponse() != "OK+Set:1")
    return panic_error = "Failed ROLE";

  sendClean("AT+SHOW0", true);
  if (readBtResponse() != "OK+Set:0")
    return panic_error = "Failed SHOW";

  sendClean("AT+ADDR?", true);
  String address_response = readBtResponse();
  if (!address_response.startsWith("OK+ADDR:")) {
    panic_error = String("Failed ADDR");
    return;
  }
    
  char* end_pointer;
  long mac_result = strtol(&address_response[15], &end_pointer, 16);

  if (end_pointer == address_response[20]) {
    Serial.println("ADDR E");
    return 0;
  }

  device_id = getDeviceId(&address_response[8]);
  if (device_id == -1)
    return panic_error = "Invalid ADDR";

  // Init CURRENT_NODE_IND
  for (uint16_t i = 0; i < NODES_COUNT; i++)
    if (getDeviceId(MESH_NODES[i].c_str()) == device_id)
      CURRENT_NODE_IND = i;

  char board_name[128];
  sprintf(board_name, "AT+NAMET3-Node-%d", device_id);
  sendClean(board_name, true);
  if (!readBtResponse().startsWith("OK+Set:"))
    return panic_error = "Failed name";
  
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