#include "Arduino.h"
#include "WiFi.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <Firebase_ESP_Client.h>
#include <DHT.h> 
#include <time.h>
#include "addons/TokenHelper.h"
#include "config.h"

// WiFi Credentials
const char* ssid = "UGA_Visitors_WiFi";
const char* password = "";

// NTP Server Configuration for Eastern Time (EST/EDT)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;       // -5 hours for Eastern Standard Time (EST)
const int daylightOffset_sec = 3600;     // +1 hour for Daylight Saving Time (EDT)

// DHT Sensor Configuration
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

// Flags and Modes
bool taskCompleted = false;
bool developmentMode = false; // Set to false for deployment mode

int deepSleepMinutes = 30;

// Function Prototypes
void setup();
void loop();
void initWiFi();
void initNTP();
void getSensorDataFileName();
void goToDeepSleep();
String getSensorDataFileName();
String getUniqueDeviceID();
void printLocalTime();
void fcsUploadCallback(FCS_UploadStatusInfo info);

void setup() {
    // Initialize serial for debugging purposes
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT); // Optional: Use the built-in LED to indicate sleep state

    // Initialize components
    initWiFi();
    initNTP();
    dht.begin();

    // Turn off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Initialize Firebase
    configF.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    configF.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    Firebase.begin(&configF, &auth);
    Firebase.reconnectWiFi(true);

    // Capture and upload sensor data
    uploadSensorDataToFirebase();

    // Go to deep sleep after capturing and uploading the sensor data
    if (!developmentMode) {
        goToDeepSleep();
    }
}

void loop() {
    if (developmentMode) {
        delay(1);
        if (Firebase.ready() && !taskCompleted) {
            taskCompleted = true;
        }
    }
}

void initWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println(" Connected to WiFi");
}

void initNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
}

void uploadSensorDataToFirebase() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    String sensorDataPath = "/device_" + getUniqueDeviceID() + "/" + String(millis()) + ".json";

    // Create JSON string
    String jsonData;
    jsonData += "{";
    jsonData += "\"temperature\": " + String(temperature) + ",";
    jsonData += "\"humidity\": " + String(humidity);
    jsonData += "}";

    // Convert JSON string to byte array
    int len = jsonData.length();
    uint8_t *data = (uint8_t*)jsonData.c_str();

    // Upload the sensor data to Firebase Storage asynchronously
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, data, len, sensorDataPath.c_str(), "application/json", uploadCallback)) {
        Serial.printf("Data upload initiated to %s\n", sensorDataPath.c_str());
    } else {
        Serial.println(fbdo.errorReason());
    }
}
void uploadCallback(FCS_UploadStatusInfo info) {
    if (info.status == firebase_fcs_upload_status_init) {
        Serial.printf("Uploading file (%d bytes) to %s\n", info.fileSize, info.remoteFileName.c_str());
    } else if (info.status == firebase_fcs_upload_status_upload) {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    } else if (info.status == firebase_fcs_upload_status_complete) {
        Serial.println("Upload completed\n");
    } else if (info.status == firebase_fcs_upload_status_error) {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}

void goToDeepSleep() {
    Serial.printf("Entering deep sleep mode for %d minutes...", deepSleepMinutes);

    // Turn off the LED
    digitalWrite(LED_BUILTIN, LOW);

    // Disable WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Set the device to wake up in 30 minutes (1800 seconds)
    esp_sleep_enable_timer_wakeup(deepSleepMinutes * 60 * 1000000); // Time in microseconds
    esp_deep_sleep_start();
}

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "Time is %A, %B %d %Y %H:%M:%S");
}

String getSensorDataFileName() {
    // Get the current timestamp
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Format the timestamp
    char timestamp[40];
    strftime(timestamp, sizeof(timestamp), "%Y/%m/%d/%H%M%S", &timeinfo);

    // Create the file name
    char sensorDataFileName[100];
    snprintf(sensorDataFileName, sizeof(sensorDataFileName), "/device_%s/%s.json", 
             getUniqueDeviceID().c_str(), timestamp);

    return String(sensorDataFileName);
}

String getUniqueDeviceID() {
    uint64_t chipid = ESP.getEfuseMac();
    char chipIdStr[17];
    snprintf(chipIdStr, sizeof(chipIdStr), "%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
    return String(chipIdStr);
}

void fcsUploadCallback(FCS_UploadStatusInfo info) {
    if (info.status == firebase_fcs_upload_status_init) {
        Serial.printf("Uploading file (%d bytes) to %s\n", info.fileSize, info.remoteFileName.c_str());
    } else if (info.status == firebase_fcs_upload_status_upload) {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    } else if (info.status == firebase_fcs_upload_status_complete) {
        Serial.println("Upload completed\n");
    } else if (info.status == firebase_fcs_upload_status_error) {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}
