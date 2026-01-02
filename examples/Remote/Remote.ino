// @filter: (memory is high)

/// @file Remote.ino
/// @brief Example demonstrating fl::Remote RPC system for JSON-based remote function calls
///
/// This example shows how to:
/// - Register functions that can be called remotely via JSON
/// - Execute functions immediately or schedule them for later
/// - Query system state and return JSON values
/// - Track execution timing with metadata
/// - Use FASTLED_REMOTE_PREFIX for host-side output filtering
///
/// Send JSON commands via serial, for example:
///   Commands (no return):
///     {"function":"fill","args":[255,0,0]}
///     {"timestamp":5000,"function":"setBrightness","args":[64]}
///
///   Queries (with return):
///     {"function":"millis","args":[]} -> returns current time
///     {"function":"getStatus","args":[]} -> returns system status
///     {"function":"getLed","args":[0]} -> returns LED color
///
/// Host-side filtering:
///   All structured JSON output is prefixed with "REMOTE: " (configurable via FASTLED_REMOTE_PREFIX).
///   This allows easy filtering on the host side to separate structured data from debug output:
///
///   # Read only structured JSON responses (filter out debug noise):
///   grep "^REMOTE: " /dev/ttyUSB0 | sed 's/^REMOTE: //' | jq .
///
///   # Disable prefix (define empty string):
///   build_flags = -DFASTLED_REMOTE_PREFIX=\"\"
///
/// @see fl/remote.h for full API documentation

#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 3

CRGB leds[NUM_LEDS];
fl::Remote remote;

// Forward declaration
fl::string readSerialJson();

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect (needed for some boards)
    }

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Register RPC functions (commands - no return value)
    remote.registerFunction("setLed", [](const fl::Json& args) {
        int index = args[0] | 0;
        int r = args[1] | 0;
        int g = args[2] | 0;
        int b = args[3] | 0;
        if (index >= 0 && index < NUM_LEDS) {
            leds[index] = CRGB(r, g, b);
            FL_DBG("Set LED " << index << " to RGB(" << r << ", " << g << ", " << b << ")");
        }
    });

    remote.registerFunction("fill", [](const fl::Json& args) {
        int r = args[0] | 0;
        int g = args[1] | 0;
        int b = args[2] | 0;
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        FL_DBG("Filled all LEDs with RGB(" << r << ", " << g << ", " << b << ")");
    });

    remote.registerFunction("setBrightness", [](const fl::Json& args) {
        int brightness = args[0] | 128;
        FastLED.setBrightness(brightness);
        FL_DBG("Set brightness to " << brightness);
    });

    // Register RPC query functions (with return values)
    // Match Arduino function names for consistency
    remote.registerFunctionWithReturn("millis", [](const fl::Json& args) -> fl::Json {
        return fl::Json(static_cast<int64_t>(millis()));
    });

    remote.registerFunctionWithReturn("micros", [](const fl::Json& args) -> fl::Json {
        return fl::Json(static_cast<int64_t>(micros()));
    });

    remote.registerFunctionWithReturn("getStatus", [](const fl::Json& args) -> fl::Json {
        fl::Json result = fl::Json::object();
        result.set("numLeds", NUM_LEDS);
        result.set("brightness", FastLED.getBrightness());
        result.set("millis", static_cast<int64_t>(millis()));
        return result;
    });

    remote.registerFunctionWithReturn("getLed", [](const fl::Json& args) -> fl::Json {
        int index = args[0] | 0;
        fl::Json result = fl::Json::object();
        if (index >= 0 && index < NUM_LEDS) {
            result.set("r", leds[index].r);
            result.set("g", leds[index].g);
            result.set("b", leds[index].b);
        } else {
            result.set("error", "Index out of range");
        }
        return result;
    });

    Serial.println("Remote RPC example ready");
    Serial.println("Send JSON over serial, e.g.:");
    Serial.println("  Commands (no return):");
    Serial.println(R"(    {"function":"fill","args":[255,0,0]})");
    Serial.println(R"(    {"timestamp":5000,"function":"setBrightness","args":[64]})");
    Serial.println("  Queries (with return):");
    Serial.println(R"(    {"function":"millis","args":[]})");
    Serial.println(R"(    {"function":"getStatus","args":[]})");
    Serial.println(R"(    {"function":"getLed","args":[0]})");
}

void loop() {
    // Process scheduled RPC calls
    remote.tick(millis());

    // Check for executed function results (includes timing metadata)
    // Output as prefixed single-line JSON for easy host-side filtering
    auto results = remote.getResults();
    for (const auto& r : results) {
        // Serialize result to JSON with all metadata (timing, return value, etc.)
        fl::Remote::printJson(r.to_json());
    }

    // Check for incoming JSON RPC via serial
    if (Serial.available()) {
        fl::string jsonRpc = readSerialJson();

        fl::Json result;
        auto err = remote.processRpc(jsonRpc, result);

        // Build structured JSON response for all cases
        fl::Json response = fl::Json::object();

        switch (err) {
            case fl::Remote::Error::None:
                response.set("status", "ok");
                if (result.has_value()) {
                    response.set("result", result);
                }
                break;
            case fl::Remote::Error::InvalidJson:
                response.set("status", "error");
                response.set("error", "Invalid JSON");
                break;
            case fl::Remote::Error::MissingFunction:
                response.set("status", "error");
                response.set("error", "Missing function field");
                break;
            case fl::Remote::Error::UnknownFunction:
                response.set("status", "error");
                response.set("error", "Unknown function");
                break;
            case fl::Remote::Error::InvalidTimestamp:
                response.set("status", "warning");
                response.set("warning", "Invalid timestamp, executed immediately");
                if (result.has_value()) {
                    response.set("result", result);
                }
                break;
        }

        // Output prefixed single-line JSON response
        fl::Remote::printJson(response);
    }

    FastLED.show();
    delay(10);
}

fl::string readSerialJson() {
    fl::string result;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        result += c;
    }
    return result;
}
