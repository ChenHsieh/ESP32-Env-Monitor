/**
 * @file capture.ino
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief TimerCAM Take Photo Test
 * @version 0.1
 * @date 2023-12-28
 *
 *
 * @Hardwares: TimerCAM
 * @Platform Version: Arduino M5Stack Board Manager v2.0.9
 * @Dependent Library:
 * TimerCam-arduino: https://github.com/m5stack/TimerCam-arduino
 */
#include "M5TimerCAM.h"

void setup() {
    TimerCAM.begin();

    if (!TimerCAM.Camera.begin()) {
        Serial.println("Camera Init Fail");
        return;
    }
    Serial.println("Camera Init Success");

    TimerCAM.Camera.sensor->set_pixformat(TimerCAM.Camera.sensor,
                                          PIXFORMAT_JPEG);
                          // PIXFORMAT_RGB565
                          // PIXFORMAT_GRAYSCALE
                          // PIXFORMAT_RGB888
                          // PIXFORMAT_YUV422
                          // PIXFORMAT_JPEG (as you've already used)
    TimerCAM.Camera.sensor->set_framesize(TimerCAM.Camera.sensor,
                                          FRAMESIZE_UXGA);
                                // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
                                //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
    TimerCAM.Camera.sensor->set_quality(TimerCAM.Camera.sensor, 60);  // Adjust JPEG quality (0-63, the extent of compression, higher number leads to smaller files)


    // TimerCAM.Camera.sensor->set_vflip(TimerCAM.Camera.sensor, 1);
    // TimerCAM.Camera.sensor->set_hmirror(TimerCAM.Camera.sensor, 0);
}

void loop() {
    if (TimerCAM.Camera.get()) {
        Serial.printf("pic size: %d\n", TimerCAM.Camera.fb->len);
        TimerCAM.Camera.free();
    }
}
