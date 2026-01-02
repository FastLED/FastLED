/// @file Remote.ino
/// @brief Example demonstrating fl::Remote RPC system for JSON-based remote function calls
///
/// This example shows how to:
/// - Register functions that can be called remotely via JSON
/// - Execute functions immediately or schedule them for later
/// - Query system state and return JSON values
/// - Track execution timing with metadata
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
    remote.update(millis());

    // Check for executed function results (includes timing metadata)
    auto results = remote.getResults();
    for (const auto& r : results) {
        if (r.wasScheduled) {
            uint32_t delay = r.executedAt - r.receivedAt;
            Serial.print("Scheduled [");
            Serial.print(r.functionName.c_str());
            Serial.print("] executed after ");
            Serial.print(delay);
            Serial.print("ms");
            if (r.result.has_value()) {
                Serial.print(": ");
                Serial.print(r.result.to_string().c_str());
            }
            Serial.println();
        }
    }

    // Check for incoming JSON RPC via serial
    if (Serial.available()) {
        fl::string jsonRpc = readSerialJson();

        fl::Json result;
        auto err = remote.processRpc(jsonRpc, result);

        switch (err) {
            case fl::Remote::Error::None:
                if (result.has_value()) {
                    // Function returned a value - send it back
                    Serial.println(result.to_string().c_str());
                } else {
                    // Function executed successfully, no return value
                    Serial.println("OK");
                }
                break;
            case fl::Remote::Error::InvalidJson:
                Serial.println("ERROR: Invalid JSON");
                break;
            case fl::Remote::Error::MissingFunction:
                Serial.println("ERROR: Missing function field");
                break;
            case fl::Remote::Error::UnknownFunction:
                Serial.println("ERROR: Unknown function");
                break;
            case fl::Remote::Error::InvalidTimestamp:
                Serial.println("WARNING: Invalid timestamp, executed immediately");
                break;
        }
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
