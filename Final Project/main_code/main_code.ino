#include <WiFi.h>
#include "esp_wifi.h" // Include ESP-IDF WiFi header for promiscuous functions

// Callback function to handle received packets
void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type) {

  Serial.println("Packet received!");

  if (type == WIFI_PKT_MGMT) { // Only process management packets
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = packet->payload;

    // Extract RSSI (signal strength)
    int rssi = packet->rx_ctrl.rssi;

    // Extract BSSID (MAC address of the access point)
    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);

    // Extract SSID (network name) from the beacon frame
    int ssidLength = payload[37]; // SSID length is at byte 37
    char ssid[33]; // SSID max length is 32 characters + null terminator
    if (ssidLength > 0 && ssidLength <= 32) {
      memcpy(ssid, &payload[38], ssidLength);
      ssid[ssidLength] = '\0'; // Null-terminate the SSID string
    } else {
      strcpy(ssid, "Unknown");
    }

    // Print connection information
    Serial.printf("SSID: %s, BSSID: %s, RSSI: %d dBm\n", ssid, bssid, rssi);
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  // Initialize WiFi in station mode
  WiFi.mode(WIFI_MODE_STA);

  // Set WiFi to promiscuous mode
  esp_wifi_set_promiscuous(true);
  wifi_promiscuous_filter_t filter = {};
  esp_wifi_set_promiscuous_filter(&filter); // No filter, capture all packets
  esp_err_t result = esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);

  if (result == ESP_OK) {
    Serial.println("ESP32 is now in promiscuous mode.");
  } else {
    Serial.printf("Failed to enable promiscuous mode. Error code: %d\n", result);
  }
}

void loop() {
  // Nothing to do in the loop; everything is handled in the callback
}