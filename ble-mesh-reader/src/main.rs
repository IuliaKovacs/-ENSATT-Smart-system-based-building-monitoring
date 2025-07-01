use btleplug::api::{
    Central, CentralEvent, Manager as _, Peripheral as _, ScanFilter, WriteType, bleuuid::BleUuid,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use chrono::{DateTime, Utc};
use futures::stream::StreamExt;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::time::Duration;
use tokio::time::{sleep, timeout};
use uuid::Uuid;

// Your mesh nodes MAC addresses
const MESH_NODES: &[&str] = &["68:5E:1C:1A:68:CF", "68:5E:1C:1A:5A:30"];

// HM-10 default service and characteristic UUIDs
const SERVICE_UUID: Uuid = Uuid::from_u128(0x0000FFE0_0000_1000_8000_00805F9B34FB);
const CHAR_UUID: Uuid = Uuid::from_u128(0x0000FFE1_0000_1000_8000_00805F9B34FB);

// Mesh commands from your Arduino code
const BLE_MESH_COMMAND_LIST_DATA_ENTRIES: u8 = b'L';
const BLE_MESH_COMMAND_GET_DATA_ENTRY: u8 = b'G';
const RESPONSE_OK: u8 = b'O';
const RESPONSE_ERROR: u8 = b'E';

#[derive(Debug, Clone, Copy)]
struct DataId {
    device_id: u16,
    counter: u16,
}

#[derive(Debug, Clone, Copy)]
struct FieldsStatuses {
    has_temperature: bool,
    has_humidity_level: bool,
    has_noise_level: bool,
    has_vibration_level: bool,
    has_brightness_level: bool,
    has_co2_level: bool,
    has_counter: bool,
    has_flame_status: bool,
    has_alarm_state: bool,
}

impl From<u16> for FieldsStatuses {
    fn from(value: u16) -> Self {
        Self {
            has_temperature: (value & 0x0001) != 0,
            has_humidity_level: (value & 0x0002) != 0,
            has_noise_level: (value & 0x0004) != 0,
            has_vibration_level: (value & 0x0008) != 0,
            has_brightness_level: (value & 0x0010) != 0,
            has_co2_level: (value & 0x0020) != 0,
            has_counter: (value & 0x0040) != 0,
            has_flame_status: (value & 0x0080) != 0,
            has_alarm_state: (value & 0x0100) != 0,
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
enum AlarmStatus {
    Undefined = 0,
    Active = 1,
    Disabled = 2,
}

impl From<u8> for AlarmStatus {
    fn from(value: u8) -> Self {
        match value {
            1 => AlarmStatus::Active,
            2 => AlarmStatus::Disabled,
            _ => AlarmStatus::Undefined,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct DataEntry {
    device_id: u16,
    counter: u16,
    fields_available: FieldsStatusesJson,
    temperature: Option<f32>,
    humidity_level: Option<f32>,
    noise_level: Option<u16>,
    vibration_level: Option<u16>,
    brightness_level: Option<u16>,
    co2_level: Option<u16>,
    counter_value: Option<u16>,
    flame_detected: Option<bool>,
    alarm_state: Option<AlarmStatus>,
    timestamp: DateTime<Utc>,
    source_node_mac: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct FieldsStatusesJson {
    has_temperature: bool,
    has_humidity_level: bool,
    has_noise_level: bool,
    has_vibration_level: bool,
    has_brightness_level: bool,
    has_co2_level: bool,
    has_counter: bool,
    has_flame_status: bool,
    has_alarm_state: bool,
}

impl From<FieldsStatuses> for FieldsStatusesJson {
    fn from(fs: FieldsStatuses) -> Self {
        Self {
            has_temperature: fs.has_temperature,
            has_humidity_level: fs.has_humidity_level,
            has_noise_level: fs.has_noise_level,
            has_vibration_level: fs.has_vibration_level,
            has_brightness_level: fs.has_brightness_level,
            has_co2_level: fs.has_co2_level,
            has_counter: fs.has_counter,
            has_flame_status: fs.has_flame_status,
            has_alarm_state: fs.has_alarm_state,
        }
    }
}

#[derive(Debug)]
struct MeshReader {
    adapter: Adapter,
}

impl MeshReader {
    async fn new() -> Result<Self, Box<dyn Error>> {
        let manager = Manager::new().await?;
        let adapters = manager.adapters().await?;
        let adapter = adapters
            .into_iter()
            .next()
            .ok_or("No Bluetooth adapter found")?;

        Ok(Self { adapter })
    }

    async fn find_mesh_node(&self) -> Result<Option<(Peripheral, String)>, Box<dyn Error>> {
        println!("Scanning for mesh nodes...");

        self.adapter
            .start_scan(ScanFilter {
                services: vec![SERVICE_UUID],
            })
            .await?;
        sleep(Duration::from_secs(5)).await;

        self.adapter.stop_scan().await?;

        let peripherals = self.adapter.peripherals().await?;

        for peripheral in peripherals {
            if let Ok(Some(properties)) = peripheral.properties().await {
                let addr_str = properties.address.to_string().to_uppercase();

                // Check if this is one of our mesh nodes
                for &node_addr in MESH_NODES {
                    if addr_str.contains(&node_addr.replace(":", "")) {
                        println!(
                            "Found mesh node: {} ({})",
                            properties.local_name.as_deref().unwrap_or("Unknown"),
                            addr_str
                        );
                        self.adapter.stop_scan().await?;
                        return Ok(Some((peripheral, addr_str)));
                    }
                }
            }
        }

        self.adapter.stop_scan().await?;
        Ok(None)
    }

    async fn connect_to_node(&self, peripheral: &Peripheral) -> Result<(), Box<dyn Error>> {
        println!("Connecting to node...");

        peripheral.connect().await?;

        // Wait for connection to stabilize
        sleep(Duration::from_millis(500)).await;

        // Discover services
        peripheral.discover_services().await?;

        println!("Connected successfully!");
        Ok(())
    }

    async fn list_data_entries(
        &self,
        peripheral: &Peripheral,
    ) -> Result<Vec<DataId>, Box<dyn Error>> {
        println!("Requesting data entries list...");

        let chars = peripheral.characteristics();
        let char = chars
            .iter()
            .find(|c| c.uuid == CHAR_UUID)
            .ok_or("Characteristic not found")?;

        // Subscribe to notifications
        peripheral.subscribe(char).await?;

        // Send LIST command
        peripheral
            .write(
                char,
                &[BLE_MESH_COMMAND_LIST_DATA_ENTRIES],
                WriteType::WithoutResponse,
            )
            .await?;

        // Wait for response with timeout
        let response =
            timeout(Duration::from_secs(5), self.read_list_response(peripheral)).await??;

        if response.is_empty() || response[0] != RESPONSE_OK {
            return Err("Invalid response from node".into());
        }

        // Parse response
        if response.len() < 3 {
            return Err("Response too short".into());
        }

        let entry_count = u16::from_le_bytes([response[1], response[2]]) as usize;
        let mut entries = Vec::new();

        let expected_size = 3 + entry_count * 4; // 3 header bytes + 4 bytes per DataId
        if response.len() < expected_size {
            return Err("Incomplete response".into());
        }

        for i in 0..entry_count {
            let offset = 3 + i * 4;
            let device_id = u16::from_le_bytes([response[offset], response[offset + 1]]);
            let counter = u16::from_le_bytes([response[offset + 2], response[offset + 3]]);

            entries.push(DataId { device_id, counter });
        }

        println!("Found {} data entries", entries.len());
        Ok(entries)
    }

    async fn get_data_entry(
        &self,
        peripheral: &Peripheral,
        data_id: DataId,
        source_mac: &str,
    ) -> Result<DataEntry, Box<dyn Error>> {
        let chars = peripheral.characteristics();
        let char = chars
            .iter()
            .find(|c| c.uuid == CHAR_UUID)
            .ok_or("Characteristic not found")?;

        // Prepare GET command with DataId
        let mut command = vec![BLE_MESH_COMMAND_GET_DATA_ENTRY];
        command.extend_from_slice(&data_id.device_id.to_le_bytes());
        command.extend_from_slice(&data_id.counter.to_le_bytes());

        // Send GET command
        peripheral
            .write(char, &command, WriteType::WithoutResponse)
            .await?;

        // Wait for response
        let response = timeout(
            Duration::from_secs(5),
            self.read_data_entry_response(peripheral),
        )
        .await??;

        if response.is_empty() || response[0] != RESPONSE_OK {
            return Err("Invalid response from node".into());
        }

        // Parse the data entry (starts at byte 1, after the OK response)
        self.parse_data_entry(&response[1..], source_mac)
    }

    fn parse_data_entry(&self, data: &[u8], source_mac: &str) -> Result<DataEntry, Box<dyn Error>> {
        if data.len() < 8 {
            return Err("Data entry too short".into());
        }

        let mut offset = 0;

        // Parse DataId
        let device_id = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;
        let counter = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;

        // Parse FieldsStatuses
        let fields_status_value = u16::from_le_bytes([data[offset], data[offset + 1]]);
        let fields_status = FieldsStatuses::from(fields_status_value);
        offset += 2;

        // Parse fields based on status
        let temperature = if fields_status.has_temperature && offset + 4 <= data.len() {
            let bytes = [
                data[offset],
                data[offset + 1],
                data[offset + 2],
                data[offset + 3],
            ];
            offset += 4;
            Some(f32::from_le_bytes(bytes))
        } else {
            offset += 4; // Skip even if not present
            None
        };

        let humidity_level = if fields_status.has_humidity_level && offset + 4 <= data.len() {
            let bytes = [
                data[offset],
                data[offset + 1],
                data[offset + 2],
                data[offset + 3],
            ];
            offset += 4;
            Some(f32::from_le_bytes(bytes))
        } else {
            offset += 4;
            None
        };

        let noise_level = if fields_status.has_noise_level && offset + 2 <= data.len() {
            let value = u16::from_le_bytes([data[offset], data[offset + 1]]);
            offset += 2;
            Some(value)
        } else {
            offset += 2;
            None
        };

        let vibration_level = if fields_status.has_vibration_level && offset + 2 <= data.len() {
            let value = u16::from_le_bytes([data[offset], data[offset + 1]]);
            offset += 2;
            Some(value)
        } else {
            offset += 2;
            None
        };

        let brightness_level = if fields_status.has_brightness_level && offset + 2 <= data.len() {
            let value = u16::from_le_bytes([data[offset], data[offset + 1]]);
            offset += 2;
            Some(value)
        } else {
            offset += 2;
            None
        };

        let co2_level = if fields_status.has_co2_level && offset + 2 <= data.len() {
            let value = u16::from_le_bytes([data[offset], data[offset + 1]]);
            offset += 2;
            Some(value)
        } else {
            offset += 2;
            None
        };

        let counter_value = if fields_status.has_counter && offset + 2 <= data.len() {
            let value = u16::from_le_bytes([data[offset], data[offset + 1]]);
            offset += 2;
            Some(value)
        } else {
            offset += 2;
            None
        };

        let flame_detected = if fields_status.has_flame_status && offset + 1 <= data.len() {
            let value = data[offset] != 0;
            offset += 1;
            Some(value)
        } else {
            offset += 1;
            None
        };

        let alarm_state = if fields_status.has_alarm_state && offset + 1 <= data.len() {
            Some(AlarmStatus::from(data[offset]))
        } else {
            None
        };

        Ok(DataEntry {
            device_id,
            counter,
            fields_available: fields_status.into(),
            temperature,
            humidity_level,
            noise_level,
            vibration_level,
            brightness_level,
            co2_level,
            counter_value,
            flame_detected,
            alarm_state,
            timestamp: Utc::now(),
            source_node_mac: source_mac.to_string(),
        })
    }

    async fn read_list_response(&self, peripheral: &Peripheral) -> Result<Vec<u8>, Box<dyn Error>> {
        let mut notification_stream = peripheral.notifications().await?;
        let mut buffer = Vec::new();

        // Collect data until we have a complete response
        while let Ok(Some(data)) =
            timeout(Duration::from_millis(500), notification_stream.next()).await
        {
            buffer.extend_from_slice(&data.value);

            // Check if we have a complete response
            if buffer.len() >= 3 {
                // For LIST command, check if we have all data
                if buffer[0] == RESPONSE_OK {
                    let expected_entries = u16::from_le_bytes([buffer[1], buffer[2]]) as usize;
                    let expected_size = 3 + expected_entries * 4;
                    if buffer.len() >= expected_size {
                        break;
                    }
                } else {
                    // Error response
                    break;
                }
            }
        }

        Ok(buffer)
    }

    async fn read_data_entry_response(
        &self,
        peripheral: &Peripheral,
    ) -> Result<Vec<u8>, Box<dyn Error>> {
        let mut notification_stream = peripheral.notifications().await?;
        let mut buffer = Vec::new();
        let expected_size = 1 + std::mem::size_of::<DataEntry>(); // OK byte + data entry

        // Collect data until we have a complete response or timeout
        while let Ok(Some(data)) =
            timeout(Duration::from_millis(500), notification_stream.next()).await
        {
            buffer.extend_from_slice(&data.value);

            // Check if we have enough data
            if buffer.len() >= expected_size || (buffer.len() > 0 && buffer[0] == RESPONSE_ERROR) {
                break;
            }
        }

        Ok(buffer)
    }

    async fn run_periodic_reader(&self) -> Result<(), Box<dyn Error>> {
        loop {
            println!("\n=== Starting new read cycle ===");

            match self.find_mesh_node().await? {
                Some((peripheral, mac_address)) => {
                    match self.connect_to_node(&peripheral).await {
                        Ok(_) => {
                            // First, get the list of entries
                            match self.list_data_entries(&peripheral).await {
                                Ok(entry_ids) => {
                                    println!(
                                        "\nFetching {} data entries from the mesh...",
                                        entry_ids.len()
                                    );

                                    let mut all_entries = Vec::new();

                                    // Fetch each entry
                                    for entry_id in entry_ids {
                                        print!(
                                            "Fetching entry {:04X}:{} ... ",
                                            entry_id.device_id, entry_id.counter
                                        );

                                        match self
                                            .get_data_entry(&peripheral, entry_id, &mac_address)
                                            .await
                                        {
                                            Ok(entry) => {
                                                println!("OK");
                                                all_entries.push(entry);
                                            }
                                            Err(e) => {
                                                println!("Failed: {}", e);
                                            }
                                        }

                                        // Small delay between requests
                                        sleep(Duration::from_millis(100)).await;
                                    }

                                    // Process collected entries
                                    self.process_entries(&all_entries).await?;
                                }
                                Err(e) => println!("Failed to list entries: {}", e),
                            }

                            // Disconnect
                            if let Err(e) = peripheral.disconnect().await {
                                println!("Error disconnecting: {}", e);
                            }
                        }
                        Err(e) => println!("Failed to connect: {}", e),
                    }
                }
                None => println!("No mesh nodes found"),
            }

            println!("Waiting 30 seconds before next cycle...");
            sleep(Duration::from_secs(30)).await;
        }
    }

    async fn process_entries(&self, entries: &[DataEntry]) -> Result<(), Box<dyn Error>> {
        println!("\n=== Collected {} entries ===", entries.len());

        for entry in entries {
            println!(
                "\nDevice ID: 0x{:04X}, Counter: {}",
                entry.device_id, entry.counter
            );
            println!("  Timestamp: {}", entry.timestamp);
            println!("  Source: {}", entry.source_node_mac);

            if let Some(temp) = entry.temperature {
                println!("  Temperature: {:.2}°C", temp);
            }
            if let Some(humidity) = entry.humidity_level {
                println!("  Humidity: {:.2}%", humidity);
            }
            if let Some(noise) = entry.noise_level {
                println!("  Noise Level: {}", noise);
            }
            if let Some(vibration) = entry.vibration_level {
                println!("  Vibration Level: {}", vibration);
            }
            if let Some(brightness) = entry.brightness_level {
                println!("  Brightness: {}", brightness);
            }
            if let Some(co2) = entry.co2_level {
                println!("  CO2 Level: {} ppm", co2);
            }
            if let Some(flame) = entry.flame_detected {
                println!("  Flame Detected: {}", flame);
            }
            if let Some(alarm) = &entry.alarm_state {
                println!("  Alarm State: {:?}", alarm);
            }
        }

        // Here you can add database insertion logic
        // For example:
        // self.insert_to_database(entries).await?;

        // Or save to JSON file for now
        let json = serde_json::to_string_pretty(entries)?;
        let filename = format!(
            "mesh_data_{}.json",
            chrono::Local::now().format("%Y%m%d_%H%M%S")
        );
        tokio::fs::write(&filename, json).await?;
        println!("\nData saved to {}", filename);

        Ok(())
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    println!("BLE Mesh Network Reader");
    println!("======================");

    let reader = MeshReader::new().await?;
    reader.run_periodic_reader().await?;

    Ok(())
}
