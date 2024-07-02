#include "Arduino.h"
#include "WiFi.h"
#include "M5TimerCAM.h"
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
bool takeNewPhoto = true;
bool taskCompleted = false;
bool developmentMode = false; // Set to false for deployment mode

int deepSleepMinutes = 30;

// Function Prototypes
void setup();
void loop();
void initWiFi();
void initCamera();
void initNTP();
void capturePhotoUploadFirebase();
void goToDeepSleep();
String getPhotoFileName();
String getUniqueDeviceID();
void printLocalTime();
void led_breathe(int ms);
void fcsUploadCallback(FCS_UploadStatusInfo info);

void setup() {
    // Initialize serial for debugging purposes
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT); // Optional: Use the built-in LED to indicate sleep state

    // Initialize components
    initWiFi();
    initCamera();
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

    // Capture and upload an image
    capturePhotoUploadFirebase();

    // Go to deep sleep after capturing and uploading the photo
    if (!developmentMode) {
        goToDeepSleep();
    }
}

void loop() {
    if (developmentMode) {
        if (takeNewPhoto) {
            capturePhotoUploadFirebase();
            takeNewPhoto = false;
        }
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

void initCamera() {
    TimerCAM.begin();

    if (!TimerCAM.Camera.begin()) {
        Serial.println("Camera Init Fail");
        return;
    }
    Serial.println("Camera Init Success");

    TimerCAM.Camera.sensor->set_pixformat(TimerCAM.Camera.sensor, PIXFORMAT_JPEG);
    TimerCAM.Camera.sensor->set_framesize(TimerCAM.Camera.sensor, FRAMESIZE_SVGA);
                  // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
                  //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
    TimerCAM.Camera.sensor->set_vflip(TimerCAM.Camera.sensor, 1);
    TimerCAM.Camera.sensor->set_hmirror(TimerCAM.Camera.sensor, 0);
    TimerCAM.Camera.sensor->set_quality(TimerCAM.Camera.sensor, 45);
        // Debugging memory and power


    if (!psramInit()) {
    Serial.println("PSRAM init failed!");
        return;
    } else {
        Serial.println("PSRAM init success!");
    }

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    Serial.printf("Free Internal RAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.printf("Power level: %d mV\n", TimerCAM.Power.getBatteryLevel());
    TimerCAM.Camera.free();
}


void initNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
}

void capturePhotoUploadFirebase() {
    // Dispose of the first pictures because of bad quality
    for (int i = 0; i < 4; i++) {
        if (TimerCAM.Camera.get()) {
            TimerCAM.Camera.free();
            Serial.printf("the %d refreshing capture", i);
        }
    }

    // Take a new photo
    if (TimerCAM.Camera.get()) {
        camera_fb_t* fb = TimerCAM.Camera.fb; // Access the frame buffer
        if (!fb) {
            Serial.println("Camera capture failed");
            delay(1000);
            ESP.restart();
        }

        // Upload the image to Firebase
        String photoPath = getPhotoFileName();
        Serial.printf("Uploading picture to %s...\n", photoPath.c_str());

        if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, fb->buf, fb->len, photoPath.c_str(), "image/jpeg", fcsUploadCallback)) {
            Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
        } else {
            Serial.println(fbdo.errorReason());
        }

        TimerCAM.Camera.free();
    } else {
        Serial.println("Camera capture failed");
        delay(1000);
        ESP.restart();
    }
}

void goToDeepSleep() {
    Serial.printf("Entering deep sleep mode for %d minutes...", deepSleepMinutes);

    // Turn off the LED
    TimerCAM.Power.setLed(0);
    TimerCAM.Camera.free();
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

String getPhotoFileName() {
    // Get the current timestamp
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Format the timestamp
    char timestamp[40];
    strftime(timestamp, sizeof(timestamp), "%Y/%m/%d/%H%M%S", &timeinfo);

    // Get temperature, humidity, and battery voltage
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int batteryVoltage = TimerCAM.Power.getBatteryLevel(); // Battery voltage in mV

    // Create the file name
    char photoFileName[100];
    snprintf(photoFileName, sizeof(photoFileName), "/device_%s/%s_%.2f_%.2f_%dlvl.jpg", 
             getUniqueDeviceID().c_str(), timestamp, temperature, humidity, batteryVoltage);

    return String(photoFileName);
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
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    } else if (info.status == firebase_fcs_upload_status_error) {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}
