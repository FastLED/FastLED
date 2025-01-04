/*
   Original Source: https://github.com/ZackFreedman/Chromance
   GaryWoo's Video: https://www.youtube.com/watch?v=-nSCtxa2Kp0
   GaryWoo's LedMap: https://gist.github.com/Garywoo/b6cd1ea90cb5e17cc60b01ae68a2b770
   GaryWoo's presets: https://gist.github.com/Garywoo/82fa67c6e1f9529dc16a01dd97d05d58
   Chromance wall hexagon source (emotion controlled w/ EmotiBit)
   Partially cribbed from the DotStar example
   I smooshed in the ESP32 BasicOTA sketch, too

   (C) Voidstar Lab 2021
*/

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_TEENSYLC)
// Avr is not powerful enough.
// Other platforms have weird issues. Will revisit this later.
void setup() {}
void loop() {}
#else

#include "mapping.h"
#include "net.h"
#include "ripple.h"
#include <FastLED.h>
#include "detail.h"
#include "fl/screenmap.h"
#include "fl/math_macros.h"
#include "fl/json.h"
#include "fl/ui.h"
#include "fl/map.h"

#include "screenmap.json.h"
#include "fl/str.h"


using namespace fl;

enum {
    BlackStrip = 0,
    GreenStrip = 1,
    RedStrip = 2,
    BlueStrip = 3,
};


// Strips are different lengths because I am a dumb

constexpr int lengths[] = {
  154, // Black strip
  168, // Green strip
  84,  // Red strip
  154  // Blue strip
};







// non emscripten uses separate arrays for each strip. Eventually emscripten
// should support this as well but right now we don't
CRGB leds0[lengths[BlackStrip]] = {};
CRGB leds1[lengths[GreenStrip]] = {};
CRGB leds2[lengths[RedStrip]] = {}; // Red 
CRGB leds3[lengths[BlueStrip]] = {};
CRGB *leds[] = {leds0, leds1, leds2, leds3};


byte ledColors[40][14][3]; // LED buffer - each ripple writes to this, then we
                           // write this to the strips
//float decay = 0.97; // Multiply all LED's by this amount each tick to create
                    // fancy fading tails

UISlider sliderDecay("decay", .97f, .8, 1.0, .01);

// These ripples are endlessly reused so we don't need to do any memory
// management
#define numberOfRipples 30
Ripple ripples[numberOfRipples] = {
    Ripple(0),  Ripple(1),  Ripple(2),  Ripple(3),  Ripple(4),  Ripple(5),
    Ripple(6),  Ripple(7),  Ripple(8),  Ripple(9),  Ripple(10), Ripple(11),
    Ripple(12), Ripple(13), Ripple(14), Ripple(15), Ripple(16), Ripple(17),
    Ripple(18), Ripple(19), Ripple(20), Ripple(21), Ripple(22), Ripple(23),
    Ripple(24), Ripple(25), Ripple(26), Ripple(27), Ripple(28), Ripple(29),
};

// Biometric detection and interpretation
// IR (heartbeat) is used to fire outward ripples
float lastIrReading;    // When our heart pumps, reflected IR drops sharply
float highestIrReading; // These vars let us detect this drop
unsigned long
    lastHeartbeat; // Track last heartbeat so we can detect noise/disconnections
#define heartbeatLockout                                                       \
    500 // Heartbeats that happen within this many milliseconds are ignored
#define heartbeatDelta 300 // Drop in reflected IR that constitutes a heartbeat

// Heartbeat color ripples are proportional to skin temperature
#define lowTemperature 33.0  // Resting temperature
#define highTemperature 37.0 // Really fired up
float lastKnownTemperature =
    (lowTemperature + highTemperature) /
    2.0; // Carries skin temperature from temperature callback to IR callback

// EDA code was too unreliable and was cut.
// TODO: Rebuild EDA code

// Gyroscope is used to reject data if you're moving too much
#define gyroAlpha 0.9 // Exponential smoothing constant
#define gyroThreshold                                                          \
    300 // Minimum angular velocity total (X+Y+Z) that disqualifies readings
float gyroX, gyroY, gyroZ;

// If you don't have an EmotiBit or don't feel like wearing it, that's OK
// We'll fire automatic pulses
#define randomPulsesEnabled true // Fire random rainbow pulses from random nodes
#define cubePulsesEnabled true   // Draw cubes at random nodes
UICheckbox starburstPulsesEnabled("Starburst Pulses", true);
UICheckbox simulatedBiometricsEnabled("Simulated Biometrics", true);

#define autoPulseTimeout                                                       \
    5000 // If no heartbeat is received in this many ms, begin firing
         // random/simulated pulses
#define randomPulseTime 2000 // Fire a random pulse every (this many) ms
unsigned long lastRandomPulse;
byte lastAutoPulseNode = 255;

byte numberOfAutoPulseTypes =
    randomPulsesEnabled + cubePulsesEnabled + int(starburstPulsesEnabled);
byte currentAutoPulseType = 255;
#define autoPulseChangeTime 30000
unsigned long lastAutoPulseChange;

#define simulatedHeartbeatBaseTime                                             \
    600 // Fire a simulated heartbeat pulse after at least this many ms
#define simulatedHeartbeatVariance                                             \
    200                           // Add random jitter to simulated heartbeat
#define simulatedEdaBaseTime 1000 // Same, but for inward EDA pulses
#define simulatedEdaVariance 10000
unsigned long nextSimulatedHeartbeat;
unsigned long nextSimulatedEda;

// Helper function to check if a node is on the border
bool isNodeOnBorder(byte node) {
    for (int i = 0; i < numberOfBorderNodes; i++) {
        if (node == borderNodes[i]) {
            return true;
        }
    }
    return false;
}

UITitle title("Chromancer");
UIDescription description("Take 6 seconds to boot up. Chromancer is a wall-mounted hexagonal LED display that originally reacted to biometric data from an EmotiBit sensor. It visualizes your heartbeat, skin temperature, and movement in real-time. Chromancer also has a few built-in effects that can be triggered with the push of a button. Enjoy!");
UICheckbox allWhite("All White", false);

UIButton simulatedHeartbeat("Simulated Heartbeat");
UIButton triggerStarburst("Trigger Starburst"); 
UIButton triggerRainbowCube("Rainbow Cube");
UIButton triggerBorderWave("Border Wave");
UIButton triggerSpiral("Spiral Wave");
bool wasHeartbeatClicked = false;
bool wasStarburstClicked = false;
bool wasRainbowCubeClicked = false;
bool wasBorderWaveClicked = false;
bool wasSpiralClicked = false;

void setup() {
    Serial.begin(115200);

    Serial.println("*** LET'S GOOOOO ***");

    Serial.println("JSON SCREENMAP");
    Serial.println(JSON_SCREEN_MAP);

    FixedMap<Str, ScreenMap, 16> segmentMaps;
    ScreenMap::ParseJson(JSON_SCREEN_MAP, &segmentMaps);

    printf("Parsed %d segment maps\n", int(segmentMaps.size()));
    for (auto kv : segmentMaps) {
        Serial.print(kv.first.c_str());
        Serial.print(" ");
        Serial.println(kv.second.getLength());
    } 


    // ScreenMap screenmaps[4];
    ScreenMap red, black, green, blue;
    bool ok = true;
    ok = segmentMaps.get("red_segment", &red) && ok;
    ok = segmentMaps.get("back_segment", &black) && ok;
    ok = segmentMaps.get("green_segment", &green) && ok;
    ok = segmentMaps.get("blue_segment", &blue) && ok;
    if (!ok) {
        Serial.println("Failed to get all segment maps");
        return;
    }


    CRGB* red_leds = leds[RedStrip];
    CRGB* black_leds = leds[BlackStrip];
    CRGB* green_leds = leds[GreenStrip];
    CRGB* blue_leds = leds[BlueStrip];

    FastLED.addLeds<WS2812, 2>(black_leds, lengths[BlackStrip]).setScreenMap(black);
    FastLED.addLeds<WS2812, 3>(green_leds, lengths[GreenStrip]).setScreenMap(green);
    FastLED.addLeds<WS2812, 1>(red_leds, lengths[RedStrip]).setScreenMap(red);
    FastLED.addLeds<WS2812, 4>(blue_leds, lengths[BlueStrip]).setScreenMap(blue);

    FastLED.show();
    net_init();
}


void loop() {
    unsigned long benchmark = millis();
    net_loop();



    // Fade all dots to create trails
    for (int strip = 0; strip < 40; strip++) {
        for (int led = 0; led < 14; led++) {
            for (int i = 0; i < 3; i++) {
                ledColors[strip][led][i] *= sliderDecay.value();
            }
        }
    }

    for (int i = 0; i < numberOfRipples; i++) {
        ripples[i].advance(ledColors);
    }

    for (int segment = 0; segment < 40; segment++) {
        for (int fromBottom = 0; fromBottom < 14; fromBottom++) {
            int strip = ledAssignments[segment][0];
            int led = round(fmap(fromBottom, 0, 13, ledAssignments[segment][2],
                                 ledAssignments[segment][1]));
            leds[strip][led] = CRGB(ledColors[segment][fromBottom][0],
                                    ledColors[segment][fromBottom][1],
                                    ledColors[segment][fromBottom][2]);
        }
    }

    if (allWhite) {
        // for all strips
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < lengths[i]; j++) {
                leds[i][j] = CRGB::White;
            }
        }
    }

    FastLED.show();


    // Check if buttons were clicked
    wasHeartbeatClicked = bool(simulatedHeartbeat);
    wasStarburstClicked = bool(triggerStarburst);
    wasRainbowCubeClicked = bool(triggerRainbowCube);
    wasBorderWaveClicked = bool(triggerBorderWave);
    wasSpiralClicked = bool(triggerSpiral);
    
    if (wasSpiralClicked) {
        // Trigger spiral wave effect from center
        unsigned int baseColor = random(0xFFFF);
        byte centerNode = 15; // Center node
        
        // Create 6 ripples in a spiral pattern
        for (int i = 0; i < 6; i++) {
            if (nodeConnections[centerNode][i] >= 0) {
                for (int j = 0; j < numberOfRipples; j++) {
                    if (ripples[j].state == dead) {
                        ripples[j].start(
                            centerNode, i,
                            Adafruit_DotStar_ColorHSV(
                                baseColor + (0xFFFF / 6) * i, 255, 255),
                            0.3 + (i * 0.1), // Varying speeds creates spiral effect
                            2000,
                            i % 2 ? alwaysTurnsLeft : alwaysTurnsRight); // Alternating turn directions
                        break;
                    }
                }
            }
        }
        lastHeartbeat = millis();
    }

    if (wasBorderWaveClicked) {
        // Trigger immediate border wave effect
        unsigned int baseColor = random(0xFFFF);
        
        // Start ripples from each border node in sequence
        for (int i = 0; i < numberOfBorderNodes; i++) {
            byte node = borderNodes[i];
            // Find an inward direction
            for (int dir = 0; dir < 6; dir++) {
                if (nodeConnections[node][dir] >= 0 && 
                    !isNodeOnBorder(nodeConnections[node][dir])) {
                    for (int j = 0; j < numberOfRipples; j++) {
                        if (ripples[j].state == dead) {
                            ripples[j].start(
                                node, dir,
                                Adafruit_DotStar_ColorHSV(
                                    baseColor + (0xFFFF / numberOfBorderNodes) * i, 
                                    255, 255),
                                .4, 2000, 0);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        lastHeartbeat = millis();
    }

    if (wasRainbowCubeClicked) {
        // Trigger immediate rainbow cube effect
        int node = cubeNodes[random(numberOfCubeNodes)];
        unsigned int baseColor = random(0xFFFF);
        byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;

        for (int i = 0; i < 6; i++) {
            if (nodeConnections[node][i] >= 0) {
                for (int j = 0; j < numberOfRipples; j++) {
                    if (ripples[j].state == dead) {
                        ripples[j].start(
                            node, i,
                            Adafruit_DotStar_ColorHSV(
                                baseColor + (0xFFFF / 6) * i, 255, 255),
                            .5, 2000, behavior);
                        break;
                    }
                }
            }
        }
        lastHeartbeat = millis();
    }

    if (wasStarburstClicked) {
        // Trigger immediate starburst effect
        unsigned int baseColor = random(0xFFFF);
        byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;
        
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < numberOfRipples; j++) {
                if (ripples[j].state == dead) {
                    ripples[j].start(
                        starburstNode, i,
                        Adafruit_DotStar_ColorHSV(
                            baseColor + (0xFFFF / 6) * i, 255, 255),
                        .65, 1500, behavior);
                    break;
                }
            }
        }
        lastHeartbeat = millis();
    }
    
    if (wasHeartbeatClicked) {
        // Trigger immediate heartbeat effect
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < numberOfRipples; j++) {
                if (ripples[j].state == dead) {
                    ripples[j].start(15, i, 0xEE1111,
                                   float(random(100)) / 100.0 * .1 + .4, 1000, 0);
                    break;
                }
            }
        }
        lastHeartbeat = millis();
    }

    if (millis() - lastHeartbeat >= autoPulseTimeout) {
        // When biometric data is unavailable, visualize at random
        if (numberOfAutoPulseTypes &&
            millis() - lastRandomPulse >= randomPulseTime) {
            unsigned int baseColor = random(0xFFFF);

            if (currentAutoPulseType == 255 ||
                (numberOfAutoPulseTypes > 1 &&
                 millis() - lastAutoPulseChange >= autoPulseChangeTime)) {
                byte possiblePulse = 255;
                while (true) {
                    possiblePulse = random(3);

                    if (possiblePulse == currentAutoPulseType)
                        continue;

                    switch (possiblePulse) {
                    case 0:
                        if (!randomPulsesEnabled)
                            continue;
                        break;

                    case 1:
                        if (!cubePulsesEnabled)
                            continue;
                        break;

                    case 2:
                        if (!starburstPulsesEnabled)
                            continue;
                        break;

                    default:
                        continue;
                    }

                    currentAutoPulseType = possiblePulse;
                    lastAutoPulseChange = millis();
                    break;
                }
            }

            switch (currentAutoPulseType) {
            case 0: {
                int node = 0;
                bool foundStartingNode = false;
                while (!foundStartingNode) {
                    node = random(25);
                    foundStartingNode = true;
                    for (int i = 0; i < numberOfBorderNodes; i++) {
                        // Don't fire a pulse on one of the outer nodes - it
                        // looks boring
                        if (node == borderNodes[i])
                            foundStartingNode = false;
                    }

                    if (node == lastAutoPulseNode)
                        foundStartingNode = false;
                }

                lastAutoPulseNode = node;

                for (int i = 0; i < 6; i++) {
                    if (nodeConnections[node][i] >= 0) {
                        for (int j = 0; j < numberOfRipples; j++) {
                            if (ripples[j].state == dead) {
                                ripples[j].start(
                                    node, i,
                                    //                      strip0.ColorHSV(baseColor
                                    //                      + (0xFFFF / 6) * i,
                                    //                      255, 255),
                                    Adafruit_DotStar_ColorHSV(baseColor, 255,
                                                              255),
                                    float(random(100)) / 100.0 * .2 + .5, 3000,
                                    1);

                                break;
                            }
                        }
                    }
                }
                break;
            }

            case 1: {
                int node = cubeNodes[random(numberOfCubeNodes)];

                while (node == lastAutoPulseNode)
                    node = cubeNodes[random(numberOfCubeNodes)];

                lastAutoPulseNode = node;

                byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;

                for (int i = 0; i < 6; i++) {
                    if (nodeConnections[node][i] >= 0) {
                        for (int j = 0; j < numberOfRipples; j++) {
                            if (ripples[j].state == dead) {
                                ripples[j].start(
                                    node, i,
                                    //                      strip0.ColorHSV(baseColor
                                    //                      + (0xFFFF / 6) * i,
                                    //                      255, 255),
                                    Adafruit_DotStar_ColorHSV(baseColor, 255,
                                                              255),
                                    .5, 2000, behavior);

                                break;
                            }
                        }
                    }
                }
                break;
            }

            case 2: {
                byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;

                lastAutoPulseNode = starburstNode;

                for (int i = 0; i < 6; i++) {
                    for (int j = 0; j < numberOfRipples; j++) {
                        if (ripples[j].state == dead) {
                            ripples[j].start(
                                starburstNode, i,
                                Adafruit_DotStar_ColorHSV(
                                    baseColor + (0xFFFF / 6) * i, 255, 255),
                                .65, 1500, behavior);

                            break;
                        }
                    }
                }
                break;
            }

            default:
                break;
            }
            lastRandomPulse = millis();
        }

        if (simulatedBiometricsEnabled) {
            // Simulated heartbeat
            if (millis() >= nextSimulatedHeartbeat) {
                for (int i = 0; i < 6; i++) {
                    for (int j = 0; j < numberOfRipples; j++) {
                        if (ripples[j].state == dead) {
                            ripples[j].start(
                                15, i, 0xEE1111,
                                float(random(100)) / 100.0 * .1 + .4, 1000, 0);

                            break;
                        }
                    }
                }

                nextSimulatedHeartbeat = millis() + simulatedHeartbeatBaseTime +
                                         random(simulatedHeartbeatVariance);
            }

            // Simulated EDA ripples
            if (millis() >= nextSimulatedEda) {
                for (int i = 0; i < 10; i++) {
                    for (int j = 0; j < numberOfRipples; j++) {
                        if (ripples[j].state == dead) {
                            byte targetNode =
                                borderNodes[random(numberOfBorderNodes)];
                            byte direction = 255;

                            while (direction == 255) {
                                direction = random(6);
                                if (nodeConnections[targetNode][direction] < 0)
                                    direction = 255;
                            }

                            ripples[j].start(
                                targetNode, direction, 0x1111EE,
                                float(random(100)) / 100.0 * .5 + 2, 300, 2);

                            break;
                        }
                    }
                }

                nextSimulatedEda = millis() + simulatedEdaBaseTime +
                                   random(simulatedEdaVariance);
            }
        }
    }

    //  Serial.print("Benchmark: ");
    //  Serial.println(millis() - benchmark);
}

#endif  // __AVR__
