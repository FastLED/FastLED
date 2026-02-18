// @filter: (platform is native)

/// @file HttpRpcClient.ino
/// @brief Example demonstrating HTTP streaming RPC client
///
/// This example shows how to:
/// - Connect to an HTTP RPC server (e.g., HttpRpcServer.ino)
/// - Send RPC requests using SYNC, ASYNC, and ASYNC_STREAM modes
/// - Handle responses and streaming updates
/// - Manage connection state with callbacks
/// - Visualize progress on LEDs
///
/// Prerequisites:
///   1. Start HttpRpcServer.ino first (on localhost:8080)
///   2. Then run this client to connect and send requests
///
/// @see fl/remote/remote.h for full API documentation
/// @see fl/remote/transport/http/PROTOCOL.md for protocol specification

#include <FastLED.h>
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/transport/http/stream_client.h"
#include "fl/remote/transport/http/stream_client.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/native_client.cpp.hpp"

// Client configuration
#define SERVER_HOST "localhost"
#define SERVER_PORT 8080

// LED configuration for visual feedback
#define NUM_LEDS 10
#define DATA_PIN 3
CRGB leds[NUM_LEDS];

// HTTP streaming client
fl::HttpStreamClient* transport = nullptr;
fl::Remote* remote = nullptr;

// Request ID counter
int requestId = 1;

// Timing
uint32_t lastCallTime = 0;
const uint32_t CALL_INTERVAL = 5000; // Call method every 5 seconds

// Current test mode
enum TestMode {
    TEST_SYNC,        // Test SYNC mode (add)
    TEST_ASYNC,       // Test ASYNC mode (longTask)
    TEST_ASYNC_STREAM // Test ASYNC_STREAM mode (streamData)
};
TestMode currentMode = TEST_SYNC;

// Response tracking
bool waitingForResponse = false;
int expectedRequestId = -1;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait for serial or timeout
    }

    Serial.println("\n=== HTTP RPC Client Example ===\n");
    Serial.println("This example connects to an HTTP RPC server");
    Serial.println("and calls remote methods using all three RPC modes:");
    Serial.println("  - SYNC: Immediate response");
    Serial.println("  - ASYNC: ACK + later result");
    Serial.println("  - ASYNC_STREAM: ACK + updates + final");
    Serial.println();
    fl::printf("Connecting to server at %s:%d...\n", SERVER_HOST, SERVER_PORT);

    // Initialize LEDs
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // Create HTTP streaming client transport
    transport = new fl::HttpStreamClient(SERVER_HOST, SERVER_PORT);

    // Configure heartbeat and timeout
    transport->setHeartbeatInterval(30000); // 30 seconds
    transport->setTimeout(60000);           // 60 seconds

    // Set connection state callbacks
    transport->setOnConnect([]() {
        Serial.println("[CALLBACK] Connected to server!");
        // Visual feedback: turn first LED green
        leds[0] = CRGB::Green;
        FastLED.show();
    });

    transport->setOnDisconnect([]() {
        Serial.println("[CALLBACK] Disconnected from server!");
        // Visual feedback: turn first LED red
        leds[0] = CRGB::Red;
        FastLED.show();
        waitingForResponse = false;
    });

    // Create Remote with transport callbacks
    remote = new fl::Remote(
        []() { return transport->readRequest(); },
        [](const fl::Json& response) { transport->writeResponse(response); }
    );

    // Connect to server
    if (!transport->connect()) {
        Serial.println("ERROR: Failed to connect to server!");
        Serial.println("Make sure HttpRpcServer example is running.");
        leds[0] = CRGB::Red;
        FastLED.show();
        return;
    }

    Serial.println("Connected successfully!");
    Serial.println();
}

void sendSyncRequest() {
    Serial.println("=== Testing SYNC mode: add(2, 3) ===");

    // Create JSON-RPC request manually (client doesn't have Remote::call() yet)
    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "add";
    fl::Json params;
    params.push_back(2);
    params.push_back(3);
    request["params"] = params;
    request["id"] = requestId;

    expectedRequestId = requestId++;
    waitingForResponse = true;

    // Send request via transport
    transport->writeResponse(request); // writeResponse sends JSON to server

    fl::printf("Sent request ID %d: add(2, 3)\n", expectedRequestId);
    Serial.println("Waiting for response...");
}

void sendAsyncRequest() {
    Serial.println("=== Testing ASYNC mode: longTask(2000) ===");

    // Create JSON-RPC request
    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "longTask";
    fl::Json params;
    params.push_back(2000); // 2 second task
    request["params"] = params;
    request["id"] = requestId;

    expectedRequestId = requestId++;
    waitingForResponse = true;

    transport->writeResponse(request);

    fl::printf("Sent request ID %d: longTask(2000)\n", expectedRequestId);
    Serial.println("Expecting ACK immediately, then result after 2 seconds...");
}

void sendAsyncStreamRequest() {
    Serial.println("=== Testing ASYNC_STREAM mode: streamData(5) ===");

    // Create JSON-RPC request
    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "streamData";
    fl::Json params;
    params.push_back(5); // Request 5 updates
    request["params"] = params;
    request["id"] = requestId;

    expectedRequestId = requestId++;
    waitingForResponse = true;

    transport->writeResponse(request);

    fl::printf("Sent request ID %d: streamData(5)\n", expectedRequestId);
    Serial.println("Expecting ACK, then 5 updates, then final result...");
}

void handleResponse(const fl::Json& response) {
    // Check if this is our expected response
    if (response.contains("id")) {
        const fl::Json& idJson = response["id"];
        if (idJson.is_number()) {
            auto idOpt = idJson.as_int();
            if (idOpt) {
                int id = *idOpt;
                if (id != expectedRequestId) {
                    fl::printf("Warning: Received response for ID %d, expected %d\n", id, expectedRequestId);
                }
            }
        }
    }

    // Check for error
    if (response.contains("error")) {
        Serial.println("ERROR Response:");
        Serial.println(response.to_string().c_str());
        waitingForResponse = false;
        return;
    }

    // Check for ACK (ASYNC/ASYNC_STREAM modes)
    if (response.contains("result")) {
        const fl::Json& result = response["result"];

        if (result.contains("ack")) {
            auto ackOpt = result["ack"].as_bool();
            if (ackOpt && *ackOpt == true) {
                Serial.println("Received ACK - request accepted");
                return; // Wait for actual result
            }
        }

        // Check for streaming update (ASYNC_STREAM mode)
        if (result.contains("update")) {
            auto updateOpt = result["update"].as_int();
            if (updateOpt) {
                int update = *updateOpt;
                fl::printf("Received update: %d\n", update);

                // Visual feedback: show progress on LEDs
                int ledIndex = (update % NUM_LEDS);
                leds[ledIndex] = CRGB::Blue;
                FastLED.show();
            }
            return; // Wait for more updates or final
        }

        // Check for final result (ASYNC_STREAM mode)
        if (result.contains("stop")) {
            auto stopOpt = result["stop"].as_bool();
            if (stopOpt && *stopOpt == true) {
                if (result.contains("value")) {
                    Serial.print("Received FINAL result: ");
                    Serial.println(result["value"].to_string().c_str());
                } else {
                    Serial.println("Received FINAL marker (no value)");
                }
                waitingForResponse = false;
                return;
            }
        }

        // Regular result (SYNC or ASYNC final)
        Serial.print("Received result: ");
        Serial.println(result.to_string().c_str());
        waitingForResponse = false;
    } else {
        Serial.println("Received response without result:");
        Serial.println(response.to_string().c_str());
    }
}

void loop() {
    uint32_t now = millis();

    // Update transport (handles heartbeat, reconnection, etc.)
    transport->update(now);

    // Update remote (processes incoming responses)
    remote->update(now);

    // Process any responses
    fl::optional<fl::Json> response = transport->readRequest();
    if (response) {
        handleResponse(*response);
    }

    // Send requests periodically (only when not waiting for response)
    if (!waitingForResponse && (now - lastCallTime >= CALL_INTERVAL)) {
        lastCallTime = now;

        // Cycle through test modes
        switch (currentMode) {
            case TEST_SYNC:
                sendSyncRequest();
                currentMode = TEST_ASYNC; // Next mode
                break;

            case TEST_ASYNC:
                sendAsyncRequest();
                currentMode = TEST_ASYNC_STREAM; // Next mode
                break;

            case TEST_ASYNC_STREAM:
                sendAsyncStreamRequest();
                currentMode = TEST_SYNC; // Cycle back to start
                Serial.println("\n--- Cycle complete, starting over ---\n");
                break;
        }
    }

    // Small delay to prevent busy-waiting
    delay(10);
}

// Example curl commands to test manually:
//
// 1. SYNC mode (add):
//    curl -X POST http://localhost:8080/rpc \
//      -H "Content-Type: application/json" \
//      -H "Transfer-Encoding: chunked" \
//      -d '{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}'
//
// 2. ASYNC mode (longTask):
//    curl -X POST http://localhost:8080/rpc \
//      -H "Content-Type: application/json" \
//      -H "Transfer-Encoding: chunked" \
//      -d '{"jsonrpc":"2.0","method":"longTask","params":[2000],"id":2}'
//
// 3. ASYNC_STREAM mode (streamData):
//    curl -X POST http://localhost:8080/rpc \
//      -H "Content-Type: application/json" \
//      -H "Transfer-Encoding: chunked" \
//      -d '{"jsonrpc":"2.0","method":"streamData","params":[5],"id":3}'
