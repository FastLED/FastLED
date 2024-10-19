/*
   Chromance wall hexagon source (emotion controlled w/ EmotiBit)
   Partially cribbed from the DotStar example
   I smooshed in the ESP32 BasicOTA sketch, too

   (C) Voidstar Lab 2021
*/

#include "mapping.h"
#include "net.h"
#include "ripple.h"
#include <FastLED.h>



#if defined(USING_DOTSTAR)
int lengths[] = {154, 168, 84,
                 154}; // Strips are different lengths because I am a dumb

Adafruit_DotStar strip0(lengths[0], 15, 2, DOTSTAR_BRG);
Adafruit_DotStar strip1(lengths[1], 0, 4, DOTSTAR_BRG);
Adafruit_DotStar strip2(lengths[2], 16, 17, DOTSTAR_BRG);
Adafruit_DotStar strip3(lengths[3], 5, 18, DOTSTAR_BRG);

Adafruit_DotStar strips[4] = {strip0, strip1, strip2, strip3};

#else
#define NUM_STRIPS 4
#define COLOR_ORDER BGR
const int lengths[] = {154, 168, 84,
                       154}; // Strips are different lengths because I am a dumb
CRGB leds0[lengths[0]];
CRGB leds1[lengths[1]];
CRGB leds2[lengths[2]];
CRGB leds3[lengths[3]];
CRGB *leds[] = {leds0, leds1, leds2, leds3};
#endif

byte ledColors[40][14][3]; // LED buffer - each ripple writes to this, then we
                           // write this to the strips
float decay = 0.97; // Multiply all LED's by this amount each tick to create
                    // fancy fading tails

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
#define starburstPulsesEnabled true      // Draw starbursts
#define simulatedBiometricsEnabled false // Simulate heartbeat and EDA ripples

#define autoPulseTimeout                                                       \
    5000 // If no heartbeat is received in this many ms, begin firing
         // random/simulated pulses
#define randomPulseTime 2000 // Fire a random pulse every (this many) ms
unsigned long lastRandomPulse;
byte lastAutoPulseNode = 255;

byte numberOfAutoPulseTypes =
    randomPulsesEnabled + cubePulsesEnabled + starburstPulsesEnabled;
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

CLEDController *controllers[4] = {};

void setup() {
    Serial.begin(115200);

    Serial.println("*** LET'S GOOOOO ***");

    // Initialize FastLED strips
    controllers[0] = &FastLED.addLeds<WS2812, 1>(leds[0], lengths[0]);
    controllers[1] = &FastLED.addLeds<WS2812, 2>(leds[1], lengths[1]);
    controllers[2] = &FastLED.addLeds<WS2812, 3>(leds[2], lengths[2]);
    controllers[3] = &FastLED.addLeds<WS2812, 4>(leds[3], lengths[3]);

    // If your PSU sucks, use this to limit the current
    //  FastLED.setBrightness(125);

    FastLED.show();
    net_init();
}

uint32_t Adafruit_DotStar_ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {

    uint8_t r, g, b;

    // Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
    // 0 is not the start of pure red, but the midpoint...a few values above
    // zero and a few below 65536 all yield pure red (similarly, 32768 is the
    // midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
    // each for red, green, blue) really only allows for 1530 distinct hues
    // (not 1536, more on that below), but the full unsigned 16-bit type was
    // chosen for hue so that one's code can easily handle a contiguous color
    // wheel by allowing hue to roll over in either direction.
    hue = (hue * 1530L + 32768) / 65536;
    // Because red is centered on the rollover point (the +32768 above,
    // essentially a fixed-point +0.5), the above actually yields 0 to 1530,
    // where 0 and 1530 would yield the same thing. Rather than apply a
    // costly modulo operator, 1530 is handled as a special case below.

    // So you'd think that the color "hexcone" (the thing that ramps from
    // pure red, to pure yellow, to pure green and so forth back to red,
    // yielding six slices), and with each color component having 256
    // possible values (0-255), might have 1536 possible items (6*256),
    // but in reality there's 1530. This is because the last element in
    // each 256-element slice is equal to the first element of the next
    // slice, and keeping those in there this would create small
    // discontinuities in the color wheel. So the last element of each
    // slice is dropped...we regard only elements 0-254, with item 255
    // being picked up as element 0 of the next slice. Like this:
    // Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
    // Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
    // Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
    // and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
    // the constants below are not the multiples of 256 you might expect.

    // Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
    if (hue < 510) { // Red to Green-1
        b = 0;
        if (hue < 255) { //   Red to Yellow-1
            r = 255;
            g = hue;       //     g = 0 to 254
        } else {           //   Yellow to Green-1
            r = 510 - hue; //     r = 255 to 1
            g = 255;
        }
    } else if (hue < 1020) { // Green to Blue-1
        r = 0;
        if (hue < 765) { //   Green to Cyan-1
            g = 255;
            b = hue - 510;  //     b = 0 to 254
        } else {            //   Cyan to Blue-1
            g = 1020 - hue; //     g = 255 to 1
            b = 255;
        }
    } else if (hue < 1530) { // Blue to Red-1
        g = 0;
        if (hue < 1275) {   //   Blue to Magenta-1
            r = hue - 1020; //     r = 0 to 254
            b = 255;
        } else { //   Magenta to Red-1
            r = 255;
            b = 1530 - hue; //     b = 255 to 1
        }
    } else { // Last 0.5 Red (quicker than % operator)
        r = 255;
        g = b = 0;
    }

    // Apply saturation and value to R,G,B, pack into 32-bit result:
    uint32_t v1 = 1 + val;  // 1 to 256; allows >>8 instead of /255
    uint16_t s1 = 1 + sat;  // 1 to 256; same reason
    uint8_t s2 = 255 - sat; // 255 to 0
    return ((((((r * s1) >> 8) + s2) * v1) & 0xff00) << 8) |
           (((((g * s1) >> 8) + s2) * v1) & 0xff00) |
           (((((b * s1) >> 8) + s2) * v1) >> 8);
}

void loop() {
    unsigned long benchmark = millis();
    net_loop();

    // Fade all dots to create trails
    for (int strip = 0; strip < 40; strip++) {
        for (int led = 0; led < 14; led++) {
            for (int i = 0; i < 3; i++) {
                ledColors[strip][led][i] *= decay;
            }
        }
    }

    for (int i = 0; i < numberOfRipples; i++) {
        ripples[i].advance(ledColors);
    }

#if defined(USING_DOTSTAR)
    for (int segment = 0; segment < 40; segment++) {
        for (int fromBottom = 0; fromBottom < 14; fromBottom++) {
            int strip = ledAssignments[segment][0];
            int led = round(fmap(fromBottom, 0, 13, ledAssignments[segment][2],
                                 ledAssignments[segment][1]));
            strips[strip].setPixelColor(led, ledColors[segment][fromBottom][0],
                                        ledColors[segment][fromBottom][1],
                                        ledColors[segment][fromBottom][2]);
        }
    }

    for (int i = 0; i < 4; i++)
        strips[i].show();
#else
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

    FastLED.show();
#endif

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
