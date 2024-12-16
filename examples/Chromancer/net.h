#pragma once

#include <Arduino.h>
#include <FastLED.h>

#define OTA_SUPPORTED 0
#define WIFI_SUPPORTED 0

#if OTA_SUPPORTED && !WIFI_SUPPORTED
#error "You can't have OTA without WiFi, dummy"
#endif

#if WIFI_SUPPORTED
#include <ArduinoOSC.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
const char *ssid = "YourMom";
const char *password = "is a nice lady";
// WiFi stuff - CHANGE FOR YOUR OWN NETWORK!
const IPAddress ip(4, 20, 6, 9); // IP address that THIS DEVICE should request
const IPAddress gateway(192, 168, 1, 1); // Your router
const IPAddress
    subnet(255, 255, 254,
           0); // Your subnet mask (find it from your router's admin panel)
const int recv_port = 42069; // Port that OSC data should be sent to (pick one,
                             // put same one in EmotiBit's OSC Config XML file)
#endif

#if OTA_SUPPORTED
#include <ArduinoOTA.h>
#endif

void net_init();
void net_loop();

void net_init() {
#if WIFI_SUPPORTED
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(ip, gateway, subnet);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    Serial.print("WiFi connected, IP = ");
    Serial.println(WiFi.localIP());

    // Subscribe to OSC transmissions for important data
    OscWiFi.subscribe(
        recv_port, "/EmotiBit/0/EDA",
        [](const OscMessage &m) { // This weird syntax is a lambda expression
                                  // (anonymous nameless function)
            lastKnownTemperature = m.arg<float>(0);
        });

    OscWiFi.subscribe(recv_port, "/EmotiBit/0/GYRO:X", [](const OscMessage &m) {
        gyroX = m.arg<float>(0) * gyroAlpha + gyroX * (1 - gyroAlpha);
    });

    OscWiFi.subscribe(recv_port, "/EmotiBit/0/GYRO:Y", [](const OscMessage &m) {
        gyroY = m.arg<float>(0) * gyroAlpha + gyroY * (1 - gyroAlpha);
    });

    OscWiFi.subscribe(recv_port, "/EmotiBit/0/GYRO:Z", [](const OscMessage &m) {
        gyroZ = m.arg<float>(0) * gyroAlpha + gyroZ * (1 - gyroAlpha);
    });

    // Heartbeat detection and visualization happens here
    OscWiFi.subscribe(recv_port, "/EmotiBit/0/PPG:IR", [](const OscMessage &m) {
        float reading = m.arg<float>(0);
        Serial.println(reading);

        int hue = 0;

        //  Ignore heartbeat when finger is wiggling around - it's not accurate
        float gyroTotal = abs(gyroX) + abs(gyroY) + abs(gyroZ);

        if (gyroTotal < gyroThreshold && lastIrReading >= reading) {
            // Our hand is sitting still and the reading dropped - let's pulse!
            Serial.print("> ");
            Serial.println(highestIrReading - reading);
            if (highestIrReading - reading >= heartbeatDelta) {
                if (millis() - lastHeartbeat >= heartbeatLockout) {
                    hue = fmap(lastKnownTemperature, lowTemperature,
                               highTemperature, 0xFFFF, 0);
                    for (int i = 0; i < 6; i++) {
                        if (nodeConnections[15][i] > 0) {
                            bool firedRipple = false;
                            // Find a dead ripple to reuse it
                            for (int j = 0; j < 30; j++) {
                                if (!firedRipple && ripples[j].state == dead) {
                                    ripples[j].start(
                                        15, i, strip0.ColorHSV(hue, 255, 255),
                                        float(random(100)) / 100.0 * .2 + .8,
                                        500, 2);

                                    firedRipple = true;
                                }
                            }
                        }
                    }
                }

                lastHeartbeat = millis();
            }
        } else {
            highestIrReading = 0;
        }

        lastIrReading = reading;
        if (reading > highestIrReading)
            highestIrReading = reading;
    });
#endif // WIFI_SUPPORTED

#if OTA_SUPPORTED
    // Wireless OTA updating? On an ARDUINO?! It's more likely than you think!
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount
            // SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

    ArduinoOTA.begin();

    Serial.println("Ready for WiFi OTA updates");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
}

void net_loop() {
#if WIFI_SUPPORTED
    OscWiFi.parse();
#endif

#if OTA_SUPPORTED
    ArduinoOTA.handle();
#endif

}
