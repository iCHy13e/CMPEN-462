#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h" // Include ESP-IDF WiFi header for promiscuous functions

// Struct to hold MAC addresses
struct IgnoredMAC {
  uint8_t mac[6];
};

// Array of ignored MAC addresses
IgnoredMAC ignoredMACs[] = {
  {{0x84, 0x23, 0x88, 0x7B, 0x7E, 0x11}}, // Ruckus Wireless, likely another AP on the network
  {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}  // Broadcast address, don't need to monitor this
};

// Function to check if a MAC address matches any ignored MAC
bool isIgnoredMAC(const uint8_t *mac) {
  for (const auto &ignored : ignoredMACs) {
    if (memcmp(mac, ignored.mac, 6) == 0) {
      return true;
    }
  }
  return false;
}

// IP and MAC address for access point
const uint8_t AP_IP[4] = {172, 16, 201, 1};
const uint8_t AP_BSSID[6] = {0x84, 0x23, 0x88, 0x79, 0x44, 0xB1}; // first 3 bytes match the other AP above

// Timing variables
unsigned long lastPrintTime = 0;
unsigned long printDuration = 3000; // 3 seconds
unsigned long cycleDuration = 10000; // 10 seconds
bool isPrinting = false;

// Function to handle packet info while in promiscuous mode
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = packet->payload;
  
  // Extract packet information
  int rssi = packet->rx_ctrl.rssi;
  uint8_t *srcMAC = &payload[10]; // Source MAC starts at byte 10
  uint8_t *dstMAC = &payload[4];  // Destination MAC starts at byte 4
  
  // For readability only print for 3 seconds every 10 seconds
  if (!isPrinting) {
    return;
  }

  // Ignore packets with the specified MAC address
  if (isIgnoredMAC(srcMAC) || isIgnoredMAC(dstMAC)) {
    return; 
  }

  // Format Management packets
  if (type == WIFI_PKT_MGMT) { 
    Serial.printf("Management Packet - RSSI: %d dBm\n", rssi);
    Serial.printf("Source MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  srcMAC[0], srcMAC[1], srcMAC[2], srcMAC[3], srcMAC[4], srcMAC[5]);
    Serial.printf("Destination MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  dstMAC[0], dstMAC[1], dstMAC[2], dstMAC[3], dstMAC[4], dstMAC[5]);
  } 
  // Format Data packets
  else if (type == WIFI_PKT_DATA) { 
    // Check if the packet contains an IP header (Ethernet II frame with IPv4)
    if (payload[12] == 0x08 && payload[13] == 0x00) { // EtherType == 0x0800 (IPv4)
      // Extract source and destination IP addresses from the IP header
      uint8_t *srcIP = &payload[26]; // Source IP starts at byte 26
      uint8_t *dstIP = &payload[30]; // Destination IP starts at byte 30

      Serial.printf("Data Packet - RSSI: %d dBm\n", rssi);
      Serial.printf("Source IP: %d.%d.%d.%d\n", srcIP[0], srcIP[1], srcIP[2], srcIP[3]);
      Serial.printf("Destination IP: %d.%d.%d.%d\n", dstIP[0], dstIP[1], dstIP[2], dstIP[3]);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  WiFi.mode(WIFI_MODE_STA); // Initialize WiFi in station mode
  esp_wifi_set_promiscuous(true); // Set WiFi to promiscuous mode

  // Configure filter to capture packets
  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_err_t result = esp_wifi_set_promiscuous_filter(&filter);
  if (result != ESP_OK) {
    Serial.printf("Failed to set promiscuous filter. Error code: %d\n", result);
    return;
  } else {
    Serial.println("Promiscuous filter set successfully.");
  }

  result = esp_wifi_set_promiscuous_rx_cb(sniffer);
  if (result == ESP_OK) {
    Serial.println("ESP32 is now in promiscuous mode.");
  } else {
    Serial.printf("Failed to enable promiscuous mode. Error code: %d\n", result);
  }
}

void loop() {
  unsigned long currentTime = millis();

  // Check if we are in the printing window
  if (currentTime - lastPrintTime >= cycleDuration) {
    lastPrintTime = currentTime;
    isPrinting = true; // Enable printing for the next 3 seconds
    Serial.println("Starting packet capture...");
  }

  // Disable printing after the 3-second window
  if (isPrinting && currentTime - lastPrintTime >= printDuration) {
    isPrinting = false;
    Serial.println("Stopping packet capture...");
  }
}