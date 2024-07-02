#include <WiFi.h>
// WiFi Credentials
const char* ssid = "UGA_Visitors_WiFi";
const char* password = "";
void setup() {
  // Start the serial communication
  Serial.begin(115200);

  // Initialize the WiFi module
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // Get the MAC address
  String macAddress = WiFi.macAddress();

  // Print the MAC address
  Serial.print("MAC Address: ");
  Serial.println(macAddress);
}

void loop() {
  // Nothing to do here
}
