#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

// Struct to hold MAC addresses and tracking data
struct DeviceInfo {
  uint8_t mac[6];
  int rssi;
  unsigned long lastSeen;
  int packetCount;
};

// Array of ignored MAC addresses
DeviceInfo ignoredMACs[] = {
  {{0x84, 0x23, 0x88, 0x7B, 0x7E, 0x11}, 0, 0, 0}, // Ruckus Wireless AP
  {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0, 0, 0}  // Broadcast address
};

// Known legitimate devices (you should populate this with your network's actual devices)
DeviceInfo knownDevices[] = {
  {{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}, 0, 0, 0} // Example known device
};

// MAC address of the access point
const uint8_t AP_BSSID[6] = {0x84, 0x23, 0x88, 0x7B, 0x90, 0xA0};

// List to track recently seen devices
#define MAX_DEVICES 50
DeviceInfo observedDevices[MAX_DEVICES];
int observedCount = 0;

// Timing variables
unsigned long lastPrintTime = 0;
unsigned long printDuration = 3000; // 3 seconds
unsigned long cycleDuration = 10000; // 10 seconds
bool isPrinting = false;

// Spoofing detection thresholds
const int MAC_CHANGE_THRESHOLD = 3; // Number of MAC changes to trigger alert
const unsigned long TIME_WINDOW = 60000; // 60 seconds for change detection
const int RSSI_VARIATION_THRESHOLD = 15; // Max expected RSSI variation for same device

// Function to check if MAC is in a list
bool isMACInList(const uint8_t *mac, DeviceInfo *list, int listSize) {
  for (int i = 0; i < listSize; i++) {
    if (memcmp(mac, list[i].mac, 6) == 0) {
      return true;
    }
  }
  return false;
}

// Function to check for MAC spoofing
void checkForSpoofing(const uint8_t *mac, int rssi) {
  unsigned long currentTime = millis();
  
  // Check if MAC matches AP's MAC (except for AP itself)
  if (memcmp(mac, AP_BSSID, 6) == 0) {
    Serial.println("\nALERT: Device using AP's MAC address detected (possible spoofing)!");
    return;
  }
  
  // Check if MAC is in known devices list but with significantly different RSSI
  for (int i = 0; i < sizeof(knownDevices)/sizeof(knownDevices[0]); i++) {
    if (memcmp(mac, knownDevices[i].mac, 6) == 0) {
      if (abs(rssi - knownDevices[i].rssi) > RSSI_VARIATION_THRESHOLD) {
        Serial.printf("\nALERT: Device with known MAC %02X:%02X:%02X:%02X:%02X:%02X shows abnormal RSSI change (possible spoofing)!\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      }
      return;
    }
  }
  
  // Track MAC changes (potential randomization)
  for (int i = 0; i < observedCount; i++) {
    // If we've seen this device before with a different MAC recently
    if (abs(rssi - observedDevices[i].rssi) < RSSI_VARIATION_THRESHOLD && 
        memcmp(mac, observedDevices[i].mac, 6) != 0 &&
        (currentTime - observedDevices[i].lastSeen) < TIME_WINDOW) {
      
      observedDevices[i].packetCount++;
      if (observedDevices[i].packetCount >= MAC_CHANGE_THRESHOLD) {
        Serial.printf("\nALERT: Potential MAC randomization detected! RSSI pattern matches but MAC changed multiple times.\n");
        Serial.printf("Current MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf("Previous MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     observedDevices[i].mac[0], observedDevices[i].mac[1], 
                     observedDevices[i].mac[2], observedDevices[i].mac[3], 
                     observedDevices[i].mac[4], observedDevices[i].mac[5]);
      }
    }
  }
  
  // Add new observation
  if (observedCount < MAX_DEVICES) {
    memcpy(observedDevices[observedCount].mac, mac, 6);
    observedDevices[observedCount].rssi = rssi;
    observedDevices[observedCount].lastSeen = currentTime;
    observedDevices[observedCount].packetCount = 1;
    observedCount++;
  } else {
    // Rotate buffer if full
    memmove(observedDevices, observedDevices+1, sizeof(DeviceInfo)*(MAX_DEVICES-1));
    memcpy(observedDevices[MAX_DEVICES-1].mac, mac, 6);
    observedDevices[MAX_DEVICES-1].rssi = rssi;
    observedDevices[MAX_DEVICES-1].lastSeen = currentTime;
    observedDevices[MAX_DEVICES-1].packetCount = 1;
  }
}

// Packet sniffer callback
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = packet->payload;
  int rssi = packet->rx_ctrl.rssi;
  uint8_t *srcMAC = &payload[10];
  
  // Skip ignored MACs
  if (isMACInList(srcMAC, ignoredMACs, sizeof(ignoredMACs)/sizeof(ignoredMACs[0]))) {
    return;
  }
  
  // Check for spoofing
  checkForSpoofing(srcMAC, rssi);
  
  // Only print during active window
  if (!isPrinting) return;
  
  // Display packet info
  if (type == WIFI_PKT_MGMT) {
    Serial.printf("MGMT - MAC: %02X:%02X:%02X:%02X:%02X:%02X RSSI: %d dBm\n",
                 srcMAC[0], srcMAC[1], srcMAC[2], srcMAC[3], srcMAC[4], srcMAC[5], rssi);
  } else if (type == WIFI_PKT_DATA) {
    Serial.printf("DATA - MAC: %02X:%02X:%02X:%02X:%02X:%02X RSSI: %d dBm\n",
                 srcMAC[0], srcMAC[1], srcMAC[2], srcMAC[3], srcMAC[4], srcMAC[5], rssi);
  }
}

const char *ssid = "Parkway Plaza Dojo";
const char *password = "44WVFXf5";

void setup() {
  Serial.begin(115200);
  delay(2000);

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
  
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  
  wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
  };
  
  if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK) {
    Serial.println("Failed to set promiscuous filter");
    return;
  }
  
  if (esp_wifi_set_promiscuous_rx_cb(sniffer) == ESP_OK) {
    Serial.println("Promiscuous mode enabled");
  } else {
    Serial.println("Failed to enable promiscuous mode");
  }
}

void loop() {
  unsigned long currentTime = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(5000);
  }

  // Manage print timing
  if (currentTime - lastPrintTime >= cycleDuration) {
    lastPrintTime = currentTime;
    isPrinting = true;
    Serial.println("Starting packet capture...");
  }

  if (isPrinting && currentTime - lastPrintTime >= printDuration) {
    isPrinting = false;
    Serial.println("Stopping packet capture...");
  }
}