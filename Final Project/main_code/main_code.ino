#include <WiFi.h>
#include "esp_wifi.h" // Include ESP-IDF WiFi header for promiscuous functions

// Callback function to handle received packets
void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MGMT) { // Only process management packets
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    Serial.print("Packet received: ");
    for (int i = 0; i < packet->rx_ctrl.sig_len; i++) {
      Serial.printf("%02X ", packet->payload[i]);
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize WiFi in station mode
  WiFi.mode(WIFI_MODE_STA);

  // Set WiFi to promiscuous mode
  esp_wifi_set_promiscuous(true);
  wifi_promiscuous_filter_t filter = {};
  esp_wifi_set_promiscuous_filter(&filter); // No filter, capture all packets
  esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);

  Serial.println("ESP32 is now in promiscuous mode.");
}

void loop() {
  // Main loop does nothing, packet processing happens in the callback
}