# Debugging Tips

**Difficulty Level**: ⭐⭐ Beginner to Intermediate
**Time to Complete**: 40-50 minutes
**Prerequisites**:
- [Getting Started: First Example](../getting-started/first-example.md) - Basic FastLED setup and code structure
- [Getting Started: Hardware](../getting-started/hardware.md) - Understanding of wiring and components
- [Common Issues](common-issues.md) - Quick fixes that solve 80% of problems before systematic debugging needed

**You'll Learn**:
- How to use Serial monitoring to diagnose LED system status, track frame rates, and identify performance bottlenecks
- Running comprehensive test patterns to verify hardware, check color channels, and identify failing LEDs
- Monitoring memory usage to prevent out-of-memory crashes, especially on constrained AVR boards (Uno/Mega)
- Debugging color issues with systematic color testing, checking COLOR_ORDER settings, and printing RGB values
- Using systematic diagnostic strategies: isolation testing (test components separately), binary search for bad LEDs, minimal test sketches, and frame rate/timing analysis

Techniques and code examples for diagnosing and debugging FastLED projects.

## Serial Monitoring

Use Serial output to monitor your LED system's status and performance.

### Basic Serial Setup

```cpp
void setup() {
    Serial.begin(115200);
    while (!Serial) { }  // Wait for serial (Leonardo/Micro only)

    Serial.println("FastLED Starting...");

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    Serial.println("LEDs initialized");
}

void loop() {
    // Print diagnostic info
    EVERY_N_SECONDS(5) {
        Serial.print("FPS: ");
        Serial.println(FastLED.getFPS());
    }
}
```

**Tips:**
- Use 115200 baud for faster output
- Remove `while (!Serial)` for non-USB boards (it will hang)
- Leonardo/Micro need the wait; Uno/Mega don't

## Test Pattern to Verify Setup

Use this comprehensive test pattern to verify your hardware and wiring:

```cpp
void testPattern() {
    // Test RED
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(1000);

    // Test GREEN
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(1000);

    // Test BLUE
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(1000);

    // Test individual LEDs
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        Serial.print("LED ");
        Serial.println(i);
        delay(100);
    }
}
```

**What this tests:**
- Color channels working correctly
- All LEDs functional
- Data signal reaching all LEDs
- Identifies which LED fails (if any)

**Usage:** Call `testPattern()` from `setup()` or by pressing a button

## Memory Usage Check

Monitor RAM usage to prevent out-of-memory issues, especially on AVR boards.

```cpp
void setup() {
    Serial.begin(115200);

    // Calculate memory usage
    int ledMemory = NUM_LEDS * sizeof(CRGB);
    Serial.print("LED array uses: ");
    Serial.print(ledMemory);
    Serial.println(" bytes");

    // Check available RAM (AVR only)
    #ifdef __AVR__
    extern int __heap_start, *__brkval;
    int freeMemory;
    if ((int)__brkval == 0) {
        freeMemory = ((int)&freeMemory) - ((int)&__heap_start);
    } else {
        freeMemory = ((int)&freeMemory) - ((int)__brkval);
    }
    Serial.print("Free RAM: ");
    Serial.print(freeMemory);
    Serial.println(" bytes");
    #endif
}
```

**Memory guidelines:**
- Each LED = 3 bytes (RGB)
- Arduino Uno: 2KB total RAM (max ~500-600 LEDs after overhead)
- Arduino Mega: 8KB total RAM (max ~2000 LEDs after overhead)
- ESP32/ESP8266: Much more RAM available

## Debugging Color Issues

Verify color order and values are correct:

```cpp
void debugColors() {
    // Test if colors are correct
    leds[0] = CRGB(255, 0, 0);    // Should be RED
    leds[1] = CRGB(0, 255, 0);    // Should be GREEN
    leds[2] = CRGB(0, 0, 255);    // Should be BLUE
    FastLED.show();

    // If colors are wrong, try different COLOR_ORDER:
    // RGB, RBG, GRB, GBR, BRG, BGR
}
```

**Advanced color debugging:**
```cpp
void printColorValues() {
    for (int i = 0; i < min(10, NUM_LEDS); i++) {
        Serial.print("LED ");
        Serial.print(i);
        Serial.print(": R=");
        Serial.print(leds[i].r);
        Serial.print(" G=");
        Serial.print(leds[i].g);
        Serial.print(" B=");
        Serial.println(leds[i].b);
    }
}
```

## Frame Rate Monitoring

Track performance and identify slowdowns:

```cpp
void loop() {
    updatePattern();
    FastLED.show();

    EVERY_N_SECONDS(1) {
        uint16_t fps = FastLED.getFPS();
        Serial.print("FPS: ");
        Serial.println(fps);

        if (fps < 30) {
            Serial.println("WARNING: Low frame rate!");
        }
    }
}
```

**Interpreting results:**
- 60+ FPS: Excellent performance
- 30-60 FPS: Good for most applications
- <30 FPS: May appear choppy, optimize code
- <10 FPS: Likely performance issue

**Common causes of low FPS:**
- Too many LEDs for processor speed
- Complex calculations in loop
- Serial.print() statements in hot path (remove after debugging)
- Blocking delays

## Check for Data Corruption

Verify LED data array hasn't been corrupted:

```cpp
void validateLEDData() {
    bool hasError = false;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Check for impossible values
        if (leds[i].r > 255 || leds[i].g > 255 || leds[i].b > 255) {
            Serial.print("ERROR at LED ");
            Serial.println(i);
            hasError = true;
        }
    }

    if (!hasError) {
        Serial.println("LED data OK");
    }
}
```

**When to use:**
- Strange colors appearing
- Random LED behavior
- After complex pointer operations
- Suspect buffer overrun

## Comprehensive Hardware Test

Complete diagnostic test for hardware verification:

```cpp
// Comprehensive hardware test
void hardwareTest() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== FastLED Hardware Test ===");

    // Test 1: Single LED
    Serial.println("Test 1: First LED RED");
    FastLED.clear();
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(2000);

    // Test 2: All RED
    Serial.println("Test 2: All LEDs RED");
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(2000);

    // Test 3: Color test
    Serial.println("Test 3: Color sequence");
    CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    for (int c = 0; c < 4; c++) {
        fill_solid(leds, NUM_LEDS, colors[c]);
        FastLED.show();
        delay(1000);
    }

    // Test 4: Individual LEDs
    Serial.println("Test 4: Individual LED scan");
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        delay(50);
        leds[i] = CRGB::Black;
    }

    Serial.println("=== Test Complete ===");
}
```

**To use:** Call `hardwareTest()` from `setup()` or create a test sketch

**What it tests:**
1. First LED functionality (most common failure point)
2. All LEDs can display same color
3. Color channels working (R, G, B, and combined)
4. Each individual LED functional and addressable

## Debugging Timing Issues

Check for timing problems with animations:

```cpp
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;

void loop() {
    unsigned long now = millis();
    unsigned long frameTime = now - lastFrameTime;
    lastFrameTime = now;

    EVERY_N_SECONDS(5) {
        Serial.print("Average frame time: ");
        Serial.print(frameTime);
        Serial.println("ms");

        Serial.print("Frames in 5 sec: ");
        Serial.println(frameCount);
        frameCount = 0;
    }

    updateLEDs();
    FastLED.show();
    frameCount++;
}
```

## Debugging Strategies

### Isolation Testing
Test components individually:
1. Test power supply alone (voltage, current capacity)
2. Test microcontroller alone (Serial output, simple LED blink)
3. Test LED strip with known-good controller
4. Combine components one at a time

### Binary Search for Bad LED
If LEDs work up to a certain point:
```cpp
void findBadLED() {
    int testPoint = NUM_LEDS / 2;

    fill_solid(leds, testPoint, CRGB::Red);
    fill_solid(&leds[testPoint], NUM_LEDS - testPoint, CRGB::Green);
    FastLED.show();

    // Adjust testPoint up or down based on which lights up
    // Repeat until you find the exact failing LED
}
```

### Minimal Test Sketch
Create simplest possible sketch to isolate issues:
```cpp
#include <FastLED.h>

#define LED_PIN 5
#define NUM_LEDS 10  // Start with just 10

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
}

void loop() {
    // Empty - just show red
}
```

## See Also

- [Common Issues](common-issues.md) - Quick fixes for frequent problems
- [Hardware Problems](hardware.md) - Hardware-specific troubleshooting
- [Main Cookbook](../COOKBOOK.md) - Complete FastLED guide
