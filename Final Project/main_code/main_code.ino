#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>

// File paths
#define KNOWN_DEVICES_FILE "/known_devices.json"
#define LEARNING_DURATION 300000  // 5 minutes in ms

// Struct to hold MAC addresses and tracking data
struct DeviceInfo {
  uint8_t mac[6];
  int avgRSSI;
  unsigned long lastSeen;
  int packetCount;
};

// Global variables
std::vector<DeviceInfo> knownDevices;
std::vector<DeviceInfo> observedDevices;
bool learningMode = true;
unsigned long learningStartTime = 0;

// AP MAC address
const uint8_t AP_BSSID[6] = { 0x84, 0x23, 0x88, 0x7B, 0x90, 0xA0 };

// Ignored MACs (AP itself and broadcast)
const DeviceInfo ignoredMACs[] = {
  { { 0x84, 0x23, 0x88, 0x7B, 0x7E, 0x11 }, 0, 0, 0 },  // Ruckus Wireless AP
  { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 0, 0, 0 }   // Broadcast address
};

// Spoofing detection thresholds
const int MAC_CHANGE_THRESHOLD = 3;       // Number of MAC changes to trigger alert
const unsigned long TIME_WINDOW = 60000;  // 60 seconds for change detection
const int RSSI_VARIATION_THRESHOLD = 15;  // Max expected RSSI variation for same device

// Timing variables
unsigned long lastPrintTime = 0;
const unsigned long printDuration = 3000;   // 3 seconds
const unsigned long cycleDuration = 10000;  // 10 seconds
bool isPrinting = false;

// WiFi credentials not sure if it's actually necessary for the esp32 to connect to the network but it does just in case
const char *ssid = "Parkway Plaza Dojo";
const char *password = "44WVFXf5";  // i trust that this leak isn't a big deal

// Function to convert MAC to String
String macToString(const uint8_t *mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
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
  if (!SPIFFS.exists(KNOWN_DEVICES_FILE)) {
    Serial.println("No known devices file found - starting in learning mode");
    return;
  }

  File file = SPIFFS.open(KNOWN_DEVICES_FILE, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Failed to parse file");
    file.close();
    return;
  }

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
  file.close();
  Serial.printf("Loaded %d known devices\n", knownDevices.size());
}

// Check if MAC should be ignored
bool isIgnoredMAC(const uint8_t *mac) {
  for (const auto &ignored : ignoredMACs) {
    if (memcmp(mac, ignored.mac, 6) == 0) {
      return true;
    }
  }
  return false;
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
        Serial.printf("\nALERT: Known device %s shows abnormal RSSI change!\n", macToString(mac).c_str());
      }
      return;
    }
  }

  // Check for MAC randomization
  for (const auto &observed : observedDevices) {
    if (abs(rssi - observed.avgRSSI) < RSSI_VARIATION_THRESHOLD && memcmp(mac, observed.mac, 6) != 0 && (currentTime - observed.lastSeen) < TIME_WINDOW) {
      Serial.printf("\nALERT: Potential MAC randomization detected!\n");
      Serial.printf("Current: %s, Previous: %s\n",
                    macToString(mac).c_str(),
                    macToString(observed.mac).c_str());
    }
  }
}

// Packet sniffer callback
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *srcMAC = &packet->payload[10];
  int rssi = packet->rx_ctrl.rssi;

  // Skip ignored MACs
  if (isIgnoredMAC(srcMAC)) return;

  if (learningMode) {
    updateObservedDevices(srcMAC, rssi);
  } else {
    checkForSpoofing(srcMAC, rssi);
  }

  // Print during active window
  if (isPrinting) {
    const char *pktType = (type == WIFI_PKT_MGMT) ? "MGMT" : "DATA";
    Serial.printf("%s - MAC: %s RSSI: %d dBm\n",
                  pktType, macToString(srcMAC).c_str(), rssi);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Initialize filesystem
  if (!SPIFFS.begin(true)) {  // true = format if mount fails
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
  Serial.println("SPIFFS mounted successfully");

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
  }

  // Manage print timing
  if (currentTime - lastPrintTime >= cycleDuration) {
    lastPrintTime = currentTime;
    isPrinting = true;
    if (learningMode) {
      Serial.printf("\nLearning mode - %d devices observed\n", observedDevices.size());
    }
    Serial.println("Starting packet capture...");
  }

  if (isPrinting && currentTime - lastPrintTime >= printDuration) {
    isPrinting = false;
    Serial.println("Stopping packet capture...");
  }
}