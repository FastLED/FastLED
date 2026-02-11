// @filter: (memory is high)

/// @file Remote.ino
/// @brief Example demonstrating fl::Remote RPC system with callback-based I/O
///
/// This example shows how to:
/// - Register functions that can be called remotely via JSON
/// - Use callback-based I/O with RequestSource/ResponseSink
/// - Execute functions immediately or schedule them for later
/// - Query system state and return JSON values
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
/// @see fl/remote/remote.h for full API documentation

#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

// Request/Response queues for callback-based I/O
fl::vector<fl::Json> requestQueue;
fl::vector<fl::Json> responseQueue;

// Remote with callback-based I/O
fl::Remote remote(
    // RequestSource: pull from queue
    []() -> fl::optional<fl::Json> {
        if (requestQueue.empty()) {
            return fl::nullopt;
        }
        auto req = fl::move(requestQueue[0]);
        requestQueue.erase(requestQueue.begin());
        return req;
    },
    // ResponseSink: push to queue
    [](const fl::Json& response) {
        responseQueue.push_back(response);
    }
);

// Forward declaration
fl::string readSerialJson();

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect (needed for some boards)
    }

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Register RPC functions (commands - no return value)
    remote.bind("setLed", [](int index, int r, int g, int b) {
        if (index >= 0 && index < NUM_LEDS) {
            leds[index] = CRGB(r, g, b);
            FL_DBG("Set LED " << index << " to RGB(" << r << ", " << g << ", " << b << ")");
        }
    });

    remote.bind("fill", [](int r, int g, int b) {
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        FL_DBG("Filled all LEDs with RGB(" << r << ", " << g << ", " << b << ")");
    });

    remote.bind("setBrightness", [](int brightness) {
        FastLED.setBrightness(brightness);
        FL_DBG("Set brightness to " << brightness);
    });

    // Register RPC query functions (with return values)
    // Match Arduino function names for consistency
    remote.bind("millis", []() -> int64_t {
        return static_cast<int64_t>(millis());
    });

    remote.bind("micros", []() -> int64_t {
        return static_cast<int64_t>(micros());
    });

    remote.bind("getStatus", []() -> fl::Json {
        fl::Json result = fl::Json::object();
        result.set("numLeds", NUM_LEDS);
        result.set("brightness", FastLED.getBrightness());
        result.set("millis", static_cast<int64_t>(millis()));
        return result;
    });

    remote.bind("getLed", [](int index) -> fl::Json {
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
    // Check for incoming JSON RPC via serial and queue it
    if (Serial.available()) {
        fl::string jsonRpc = readSerialJson();

        // Parse JSON and queue the request
        fl::Json doc = fl::Json::parse(jsonRpc);
        requestQueue.push_back(doc);
    }

    // Process all queued requests: pull + tick + push
    remote.update(millis());

    // Drain response queue and output to serial
    while (!responseQueue.empty()) {
        const auto& response = responseQueue[0];
        Serial.println(response.to_string().c_str());
        responseQueue.erase(responseQueue.begin());
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
