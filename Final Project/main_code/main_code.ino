#include <WiFi.h>
#include "esp_wifi.h" // Include ESP-IDF WiFi header for promiscuous functions

// Define your AP's IP address
const uint8_t AP_IP[4] = {172, 16, 201, 1};

// Callback function to handle received packets
void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MGMT || type == WIFI_PKT_CTRL || type == WIFI_PKT_DATA) {
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;

    // Extract RSSI (signal strength)
    int rssi = packet->rx_ctrl.rssi;

    // Print packet type and RSSI
    const char *typeStr = (type == WIFI_PKT_MGMT) ? "Management" :
                          (type == WIFI_PKT_CTRL) ? "Control" :
                          "Data";
    Serial.printf("Packet Type: %s, RSSI: %d dBm\n", typeStr, rssi);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Initialize WiFi in station mode
  WiFi.mode(WIFI_MODE_STA);

  // Set WiFi to promiscuous mode
  esp_wifi_set_promiscuous(true);

  // Configure filter to capture packets to/from AP_IP
  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA; // Filter management and data packets

  esp_err_t result = esp_wifi_set_promiscuous_filter(&filter);
  if (result != ESP_OK) {
    Serial.printf("Failed to set promiscuous filter. Error code: %d\n", result);
    return;
  } else {
    Serial.println("Promiscuous filter set successfully.");
  }

  result = esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
  if (result == ESP_OK) {
    Serial.println("ESP32 is now in promiscuous mode.");
  } else {
    Serial.printf("Failed to enable promiscuous mode. Error code: %d\n", result);
  }
}

void loop() {
  // Nothing to do in the loop; everything is handled in the callback
}