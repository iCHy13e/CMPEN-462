#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>

// File path
#define KNOWN_DEVICES_FILE "/known_devices.json"
#define LEARNING_DURATION 150000  // 2.5 minutes in ms

// Struct to hold MAC addresses and tracking data
struct DeviceInfo {
  uint8_t mac[6];
  int avgRSSI;
  unsigned long lastSeen;
  int packetCount;
  unsigned long lastAlertTime; 
};

// Global variables
std::vector<DeviceInfo> knownDevices;
std::vector<DeviceInfo> observedDevices;
bool learningMode = true;
unsigned long learningStartTime = 0;
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL = 30000; // 30 seconds

// AP MAC address
const uint8_t AP_BSSID[6] = { 0x84, 0x23, 0x88, 0x7B, 0x90, 0xA0 };

// Ignored MACs (AP itself and broadcast)
const DeviceInfo ignoredMACs[] = {
  { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 0, 0, 0 }  // Broadcast address
};

// WiFi credentials not sure if it's actually necessary for the esp32 to connect to the network but it does just in case
const char *ssid = "Parkway Plaza Dojo";
const char *password = "44WVFXf5";  // i trust that this leak isn't a big deal

// Spoofing detection thresholds
const unsigned long TIME_WINDOW = 30000;  // 30 seconds for change detection
const int RSSI_VARIATION_THRESHOLD = 25;  // Max expected RSSI variation for same device
const int RANDOM_MAC_RSSI = 10;           // threshold for random MAC detection calculation


// Function to check if a MAC address should be ignored
bool isIgnoredMAC(const uint8_t *mac) {
  // Check against ignored MACs list
  for (const auto &ignored : ignoredMACs) {
    if (memcmp(mac, ignored.mac, 6) == 0) {
      return true;
    }
  }

  // Make sure not to ignore the Host AP's MAC address
  if (mac[0] == 0x84 && mac[1] == 0x23 && mac[2] == 0x88 && mac[3] == 0x7B && mac[4] == 0x90 && mac[5] == 0xA0) { 
    return false;
  } 
  //Ignore the MAC addresses of other APs on network
  else if (mac[0] == 0x84 && mac[1] == 0x23 && mac[2] == 0x88) {
    return true;
  }
  return false;
}

// Function to convert MAC to String
String macToString(const uint8_t *mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}


// Function to delete known devices file 
void deleteKnownDevicesFile() {
  if (SPIFFS.exists(KNOWN_DEVICES_FILE)) {
    SPIFFS.remove(KNOWN_DEVICES_FILE);
    SPIFFS.remove("/known_devices.json");
  }
}

// Save known devices to SPIFFS
void saveKnownDevices() {
  File file = SPIFFS.open(KNOWN_DEVICES_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create file");
    return;
  }

  JsonDocument doc;
  JsonArray devices = doc["devices"].to<JsonArray>();

  for (const auto &device : knownDevices) {
    JsonObject dev = devices.add<JsonObject>();
    dev["mac"] = macToString(device.mac);
    dev["avgRSSI"] = device.avgRSSI;
    dev["packetCount"] = device.packetCount;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write file");
  }
  file.close();
}

// Load known devices from SPIFFS
void loadKnownDevices() {
  // Check if the file exists
  if (!SPIFFS.exists(KNOWN_DEVICES_FILE)) {
    Serial.println("No known devices file found - starting in learning mode");
    return;
  }

  // Open the file for reading
  File file = SPIFFS.open(KNOWN_DEVICES_FILE, FILE_READ);
  if (!file) {
    Serial.println("Failed to open known_devices.json");
    return;
  }

  Serial.println("Contents of known_devices.json:");

  // Read the entire file content
  String fileContent = file.readString();
  file.close();

  // Parse and re-serialize the JSON for pretty printing
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, fileContent);
  if (error) {
    Serial.println("Failed to parse JSON content");
    return;
  }

  String formattedJson;
  serializeJsonPretty(doc, formattedJson);
  Serial.println(formattedJson);

  // Load known devices from the parsed JSON
  knownDevices.clear();
  for (JsonObject dev : doc["devices"].as<JsonArray>()) {
    const char *macStr = dev["mac"];
    uint8_t mac[6];
    sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    DeviceInfo device;
    memcpy(device.mac, mac, 6);
    device.avgRSSI = dev["avgRSSI"];
    device.packetCount = dev["packetCount"];
    device.lastSeen = 0;
    knownDevices.push_back(device);
  }

  Serial.printf("Loaded %d known devices\n", knownDevices.size());
}


// Update or add device to observed list
void updateObservedDevices(const uint8_t *mac, int rssi) {
  for (auto &device : observedDevices) {
    if (memcmp(device.mac, mac, 6) == 0) {
      // Update existing device
      device.avgRSSI = (device.avgRSSI * device.packetCount + rssi) / (device.packetCount + 1);
      device.packetCount++;
      device.lastSeen = millis();
      return;
    }
  }

  // Add new device
  DeviceInfo newDevice;
  memcpy(newDevice.mac, mac, 6);
  newDevice.avgRSSI = rssi;
  newDevice.packetCount = 1;
  newDevice.lastSeen = millis();
  observedDevices.push_back(newDevice);
}

// Check for MAC spoofing
void checkForSpoofing(const uint8_t *mac, int rssi) {
  unsigned long currentTime = millis();

  // Check if MAC matches AP's MAC
  if (memcmp(mac, AP_BSSID, 6) == 0) {
    Serial.println("\nALERT: Device using AP's MAC address detected!");
    return;
  }

  // Check against known devices
  for (const auto &known : knownDevices) {
    if (memcmp(mac, known.mac, 6) == 0) {
      if (abs(rssi - known.avgRSSI) > RSSI_VARIATION_THRESHOLD) {
        Serial.printf("\nALERT: Known device %s shows abnormal RSSI change! Current RSSI: %d dBm, Known Avg RSSI: %d dBm", macToString(mac).c_str(), rssi, known.avgRSSI);
        break; // Stop after the first match
      }
      return;
    }
  }

  // Check for MAC randomization
  for (auto &observed : observedDevices) {
    if (abs(rssi - observed.avgRSSI) < RANDOM_MAC_RSSI && memcmp(mac, observed.mac, 6) != 0 && (currentTime - observed.lastSeen) < TIME_WINDOW) {
      if (currentTime - observed.lastAlertTime > TIME_WINDOW) { // Cooldown check
        Serial.printf("\nALERT: Potential MAC randomization detected!");
        Serial.printf("\nCurrent: %s, Previous: %s\n", macToString(mac).c_str(), macToString(observed.mac).c_str());
        observed.lastAlertTime = currentTime; // Update last alert time
        break; // Stop after the first match
      }
    }
  }
}

// Packet sniffer function
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *srcMAC = &packet->payload[10];
  int rssi = packet->rx_ctrl.rssi;

  // Skip ignored MACs
  if (isIgnoredMAC(srcMAC)) return;
  
  updateObservedDevices(srcMAC, rssi);
  if (learningMode) {
    // Print during active window
      const char *pktType = (type == WIFI_PKT_MGMT) ? "MGMT" : "DATA";
      Serial.printf("%s - MAC: %s RSSI: %d dBm\n", pktType, macToString(srcMAC).c_str(), rssi);
  } else {
    checkForSpoofing(srcMAC, rssi);
  }
} 


void setup() {
  Serial.begin(115200);
  delay(5000); // Wait for 5 Seconds for Serial Monitor to open

  Serial.println("Starting up...");

  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nConnection Failed! Restarting...");
    ESP.restart();
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


  // Initialize filesystem format if mount fails
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed, attempting to format...");
    if (SPIFFS.format()) {
      Serial.println("SPIFFS formatted successfully");
      if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed even after formatting!");
        while (1) delay(1000);
      }
    } else {
      Serial.println("SPIFFS formatting failed!");
      while (1) delay(1000);
    }
  }

  // Uncomment lines below to delete known devices file and start in learning mode
  deleteKnownDevicesFile();
  if (SPIFFS.exists(KNOWN_DEVICES_FILE)) {
    Serial.println("File still exists after calling delete function!");
  } else {
    Serial.println("File successfully deleted.");
  }
  
  Serial.println("SPIFFS mounted successfully");

  // Load known devices or start learning
  loadKnownDevices();
  if (knownDevices.empty()) {
    learningMode = true;
    learningStartTime = millis();
    Serial.println("Starting 5-minute learning mode...");
  } else {
    learningMode = false;
    Serial.println("Known devices loaded. Starting monitoring mode.");
  }

  // Setup promiscuous mode
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);

  wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
  };

  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(sniffer);
}

void loop() {
  unsigned long currentTime = millis();

  // Handle WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  // Handle learning mode timeout
  if (learningMode && currentTime - learningStartTime > LEARNING_DURATION) {
    learningMode = false;
    knownDevices = observedDevices;
    saveKnownDevices();
    Serial.printf("\nLearning complete. Saved %d known devices.\n", knownDevices.size());
    Serial.println("Switching to monitoring mode...");
  }

  // Update known devices every 60 seconds
  if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
    lastUpdateTime = currentTime;

    // Update known devices with observed devices
    for (const auto &observed : observedDevices) {
      bool found = false;
      for (auto &known : knownDevices) {
        if (memcmp(observed.mac, known.mac, 6) == 0) {
          // Update existing known device
          known.avgRSSI = (known.avgRSSI * known.packetCount + observed.avgRSSI * observed.packetCount) /
                          (known.packetCount + observed.packetCount);
          known.packetCount += observed.packetCount;
          known.lastSeen = observed.lastSeen;
          found = true;
          break;
        }
      }
      if (!found) {
        // Add new device to known devices
        if (knownDevices.size() >= 200) {
          // Remove the oldest device based on lastSeen
          auto oldest = std::min_element(knownDevices.begin(), knownDevices.end(),[](const DeviceInfo &a, const DeviceInfo &b) { return a.lastSeen < b.lastSeen;});
          if (oldest != knownDevices.end()) {
            knownDevices.erase(oldest);
          }
        }
        knownDevices.push_back(observed);
      }
    }

    // Save updated known devices to SPIFFS
    saveKnownDevices();
    Serial.printf("\nUpdated known devices. Total: %d devices.", knownDevices.size());
  }
}