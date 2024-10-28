#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
// Replace with your network credentials
const char* ssid = "UGA_Visitors_WiFi";
const char* password = "";

// NTP server details
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";
const long gmtOffset_sec = 0; // Adjust according to your timezone
const int daylightOffset_sec = 3600; // Adjust if your region uses daylight saving time

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Check internet connectivity
  checkInternetConnection();

  // Initialize and synchronize time with multiple NTP servers
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  Serial.println("Waiting for time synchronization");
  
  int retries = 0;
  while (!time(nullptr) && retries < 10) { // Try for 10 seconds
    Serial.print(".");
    delay(1000);
    retries++;
  }
  if (retries >= 10) {
    Serial.println("Failed to obtain time after retries");
  } else {
    Serial.println("Time synchronized");
    printLocalTime();
  }
}

void loop() {
  // Perform any additional tasks
  delay(60000); // Check every 60 seconds
  printLocalTime();
}

void checkInternetConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://worldtimeapi.org/api/ip"); // URL to ping
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println("Internet is accessible");
      Serial.println("Response:");
      Serial.println(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      Serial.println("Internet is not accessible");
    }

    http.end(); // Free resources
  } else {
    Serial.println("Wi-Fi not connected");
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}