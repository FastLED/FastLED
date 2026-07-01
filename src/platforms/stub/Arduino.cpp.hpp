// IWYU pragma: private

// ok no namespace fl

#include "platforms/wasm/is_wasm.h"
#if (defined(FASTLED_USE_STUB_ARDUINO) || defined(FL_IS_WASM)) && !defined(FASTLED_NO_ARDUINO_STUBS)
// STUB platform implementation - excluded for WASM builds which provide their own Arduino.cpp
// Also excluded when FASTLED_NO_ARDUINO_STUBS is defined (for compatibility with ArduinoFake, etc.)

#include "platforms/stub/Arduino.h"  // ok include

#include "fl/stl/singleton.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/stdio.h"
#include "fl/math/math.h"
#include "fl/stl/cstdlib.h"
#include "fl/stl/noexcept.h"

// Arduino map() function - in global namespace (NOT in fl::)
// fl::map refers to the map container (red-black tree)
// Delegates to fl::map_range() for consistent behavior and overflow protection
long map(long x, long in_min, long in_max, long out_min, long out_max) FL_NO_EXCEPT {
    return fl::map_range<long, long>(x, in_min, in_max, out_min, out_max);
}

// Random number generation functions
long random(long min, long max) FL_NO_EXCEPT {
    if (min == max) {
        return min;
    }
    if (min > max) {
        // Swap if inverted (Arduino behavior)
        long tmp = min;
        min = max;
        max = tmp;
    }
    // Arduino random() is exclusive of max
    long range = max - min;
    if (range <= 0) {
        return min;
    }
    return min + (rand() % range);
}

long random(long max) FL_NO_EXCEPT {
    return random(0, max);
}

// Analog value storage for test injection
// Key: pin number, Value: analog value (or -1 for unset/random)
static fl::flat_map<int, int> g_analog_values;

int analogRead(int pin) FL_NO_EXCEPT {
    // Check if a test value has been set for this pin
    auto it = g_analog_values.find(pin);
    if (it != g_analog_values.end() && it->second >= 0) {
        return it->second;
    }
    // Default: return random value (0-1023 for 10-bit ADC emulation)
    return random(0, 1024);  // Arduino random is exclusive of max
}

// Test helper functions for analog value injection
void setAnalogValue(int pin, int value) FL_NO_EXCEPT {
    g_analog_values[pin] = value;
}

int getAnalogValue(int pin) FL_NO_EXCEPT {
    auto it = g_analog_values.find(pin);
    if (it != g_analog_values.end()) {
        return it->second;
    }
    return -1;  // Not set
}

void clearAnalogValues() FL_NO_EXCEPT {
    g_analog_values.clear();
}

// Arduino hardware initialization (stub: does nothing)
void init() FL_NO_EXCEPT {
    // On real Arduino platforms, init() sets up timers, interrupts, and other hardware.
    // On stub platform, there's no hardware to initialize, so this is a no-op.
}

// Digital I/O functions
void digitalWrite(int, int) FL_NO_EXCEPT {}
void analogWrite(int, int) FL_NO_EXCEPT {}
void analogReference(int) FL_NO_EXCEPT {}
int digitalRead(int) FL_NO_EXCEPT { return LOW; }
void pinMode(int, int) FL_NO_EXCEPT {}

// SerialEmulation member functions
void SerialEmulation::begin(int) FL_NO_EXCEPT {}

void SerialEmulation::println() FL_NO_EXCEPT {
    fl::printf("\n");
}

int SerialEmulation::available() FL_NO_EXCEPT {
    return 0;
}

int SerialEmulation::read() FL_NO_EXCEPT {
    return 0;
}

fl::string SerialEmulation::readStringUntil(char terminator) FL_NO_EXCEPT {
    (void)terminator;
    // Stub implementation: returns empty string since there's no actual serial input
    // In a real implementation, this would read from stdin until the terminator is found
    // For testing purposes, you could set an environment variable or use stdin redirection
    return fl::string();
}

void SerialEmulation::write(fl::u8) FL_NO_EXCEPT {}

void SerialEmulation::write(const char *s) FL_NO_EXCEPT {
    fl::printf("%s", s);
}

void SerialEmulation::write(const fl::u8 *s, size_t n) FL_NO_EXCEPT {
    fl::write_bytes(s, n);
}

void SerialEmulation::write(const char *s, size_t n) FL_NO_EXCEPT {
    fl::write_bytes(reinterpret_cast<const fl::u8 *>(s), n); // ok reinterpret cast
}

void SerialEmulation::flush() FL_NO_EXCEPT {}

void SerialEmulation::end() FL_NO_EXCEPT {}

fl::u8 SerialEmulation::peek() FL_NO_EXCEPT {
    return 0;
}

// Serial instances
// FL_LINT_ALLOW_GLOBAL(stub Arduino Serial* — emulates the Arduino API globals for host-only test builds; no on-device bloat, cannot wrap in Singleton because user sketches call `Serial.begin()` etc. by name)
SerialEmulation Serial;
// FL_LINT_ALLOW_GLOBAL(stub Arduino Serial1 — see Serial above)
SerialEmulation Serial1;
// FL_LINT_ALLOW_GLOBAL(stub Arduino Serial2 — see Serial above)
SerialEmulation Serial2;
// FL_LINT_ALLOW_GLOBAL(stub Arduino Serial3 — see Serial above)
SerialEmulation Serial3;

#endif
