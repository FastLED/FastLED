// @filter: (memory is high) && (not avr)

/// @file HttpRpcBidirectional.ino
/// @brief Example demonstrating bidirectional HTTP streaming RPC (server + client in same process)
///
/// This example shows how to:
/// - Run both HTTP RPC server and client in the same application
/// - Server binds methods (SYNC, ASYNC, ASYNC_STREAM)
/// - Client connects to server via loopback (localhost)
/// - Demonstrate all three RPC modes over HTTP streaming
/// - Handle responses and visualize on LEDs
///
/// This is useful for:
/// - Testing RPC functionality without multiple processes
/// - Building applications with internal RPC communication
/// - Demonstrating full HTTP streaming RPC capabilities
///
/// @see fl/remote/remote.h for full API documentation
/// @see fl/remote/transport/http/PROTOCOL.md for protocol specification

#include <FastLED.h>
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/transport/http/stream_server.h"
#include "fl/remote/transport/http/stream_client.h"
#include "fl/remote/transport/http/stream_server.cpp.hpp"
#include "fl/remote/transport/http/stream_client.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/native_server.cpp.hpp"
#include "fl/remote/transport/http/native_client.cpp.hpp"

#define NUM_LEDS 10
#define DATA_PIN 3
#define SERVER_PORT 8080

CRGB leds[NUM_LEDS];

// Server-side components
fl::HttpStreamServer* serverTransport = nullptr;
fl::Remote* serverRemote = nullptr;

// Client-side components
fl::HttpStreamClient* clientTransport = nullptr;
fl::Remote* clientRemote = nullptr;

// Request tracking
int requestId = 1;
bool waitingForResponse = false;
uint32_t lastRequestTime = 0;
const uint32_t REQUEST_INTERVAL = 3000; // 3 seconds between requests

// Test mode
enum TestMode {
    TEST_SYNC,        // Test SYNC mode (add)
    TEST_ASYNC,       // Test ASYNC mode (longTask)
    TEST_ASYNC_STREAM // Test ASYNC_STREAM mode (streamData)
};
TestMode currentMode = TEST_SYNC;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait for serial or timeout
    }

    Serial.println("\n=== HTTP RPC Bidirectional Example ===\n");
    Serial.println("This example demonstrates server + client in one process:");
    Serial.println("  - Server listens on port 8080");
    Serial.println("  - Client connects to localhost:8080");
    Serial.println("  - All three RPC modes demonstrated");
    Serial.println();

    // Initialize LEDs
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // ========== SERVER SETUP ==========
    Serial.println("Starting HTTP server on port 8080...");

    serverTransport = new fl::HttpStreamServer(SERVER_PORT);
    serverTransport->setHeartbeatInterval(30000);
    serverTransport->setTimeout(60000);

    // Connection callbacks
    serverTransport->setOnConnect([]() {
        Serial.println("[SERVER] Client connected!");
        leds[0] = CRGB::Green;
        FastLED.show();
    });

    serverTransport->setOnDisconnect([]() {
        Serial.println("[SERVER] Client disconnected!");
        leds[0] = CRGB::Red;
        FastLED.show();
    });

    // Create server Remote
    serverRemote = new fl::Remote(
        []() { return serverTransport->readRequest(); },
        [](const fl::Json& response) { serverTransport->writeResponse(response); }
    );

    // ========== BIND SERVER METHODS ==========

    // SYNC mode: add(a, b)
    serverRemote->bind("add", [](const fl::Json& params) -> fl::Json {
        int a = 0, b = 0;
        if (params.is_array() && params.size() >= 2) {
            auto aOpt = params[0].as_int();
            auto bOpt = params[1].as_int();
            if (aOpt && bOpt) {
                a = *aOpt;
                b = *bOpt;
            }
        }
        int result = a + b;
        fl::printf("[SERVER] add(%d, %d) = %d\n", a, b, result);
        return fl::Json(result);
    });

    // ASYNC mode: longTask(duration)
    serverRemote->bindAsync("longTask", [](fl::ResponseSend& send, const fl::Json& params) {
        // Send ACK
        fl::Json ack = fl::Json::object();
        ack.set("ack", true);
        send.send(ack);

        // Extract duration
        int duration = 1000;
        if (params.is_array() && params.size() > 0) {
            auto durationOpt = params[0].as_int();
            if (durationOpt) {
                duration = *durationOpt;
            }
        }

        fl::printf("[SERVER] longTask(%d) - ACK sent\n", duration);

        // Simulate work
        uint32_t startTime = millis();
        while (millis() - startTime < static_cast<uint32_t>(duration)) {
            delay(100);
        }

        // Send result
        fl::Json result = fl::Json::object();
        result.set("value", 42);
        result.set("duration", duration);
        send.send(result);

        fl::printf("[SERVER] longTask(%d) - DONE\n", duration);
    }, fl::RpcMode::ASYNC);

    // ASYNC_STREAM mode: streamData(count)
    serverRemote->bindAsync("streamData", [](fl::ResponseSend& send, const fl::Json& params) {
        // Send ACK
        fl::Json ack = fl::Json::object();
        ack.set("ack", true);
        send.send(ack);

        // Extract count
        int count = 5;
        if (params.is_array() && params.size() > 0) {
            auto countOpt = params[0].as_int();
            if (countOpt) {
                count = *countOpt;
            }
        }

        fl::printf("[SERVER] streamData(%d) - ACK sent\n", count);

        // Send updates
        for (int i = 0; i < count; i++) {
            fl::Json update = fl::Json::object();
            update.set("update", i);
            send.sendUpdate(update);
            fl::printf("[SERVER] streamData - update %d/%d\n", i + 1, count);
            delay(200);
        }

        // Send final
        fl::Json final = fl::Json::object();
        final.set("value", count);
        send.sendFinal(final);

        fl::printf("[SERVER] streamData(%d) - DONE\n", count);
    }, fl::RpcMode::ASYNC_STREAM);

    // Start server
    if (!serverTransport->connect()) {
        Serial.println("ERROR: Failed to start server!");
        return;
    }
    Serial.println("✓ Server started successfully\n");

    // ========== CLIENT SETUP ==========
    delay(500); // Give server time to start

    Serial.println("Connecting client to server...");

    clientTransport = new fl::HttpStreamClient("localhost", SERVER_PORT);
    clientTransport->setHeartbeatInterval(30000);
    clientTransport->setTimeout(60000);

    // Connection callbacks
    clientTransport->setOnConnect([]() {
        Serial.println("[CLIENT] Connected to server!");
        leds[9] = CRGB::Blue;
        FastLED.show();
    });

    clientTransport->setOnDisconnect([]() {
        Serial.println("[CLIENT] Disconnected from server!");
        leds[9] = CRGB::Red;
        FastLED.show();
        waitingForResponse = false;
    });

    // Create client Remote
    clientRemote = new fl::Remote(
        []() { return clientTransport->readRequest(); },
        [](const fl::Json& response) { clientTransport->writeResponse(response); }
    );

    // Connect to server
    if (!clientTransport->connect()) {
        Serial.println("ERROR: Failed to connect to server!");
        return;
    }

    Serial.println("✓ Client connected successfully\n");
    Serial.println("Starting test sequence...\n");
}

void sendSyncRequest() {
    Serial.println(">>> [CLIENT] Testing SYNC mode: add(2, 3)");

    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "add";
    fl::Json params;
    params.push_back(2);
    params.push_back(3);
    request["params"] = params;
    request["id"] = requestId++;

    waitingForResponse = true;
    clientTransport->writeResponse(request);
}

void sendAsyncRequest() {
    Serial.println(">>> [CLIENT] Testing ASYNC mode: longTask(1000)");

    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "longTask";
    fl::Json params;
    params.push_back(1000);
    request["params"] = params;
    request["id"] = requestId++;

    waitingForResponse = true;
    clientTransport->writeResponse(request);
}

void sendAsyncStreamRequest() {
    Serial.println(">>> [CLIENT] Testing ASYNC_STREAM mode: streamData(5)");

    fl::Json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "streamData";
    fl::Json params;
    params.push_back(5);
    request["params"] = params;
    request["id"] = requestId++;

    waitingForResponse = true;
    clientTransport->writeResponse(request);
}

void handleClientResponse(const fl::Json& response) {
    if (response.contains("result")) {
        const fl::Json& result = response["result"];

        // ACK
        if (result.contains("ack")) {
            auto ackOpt = result["ack"].as_bool();
            if (ackOpt && *ackOpt) {
                Serial.println("<<< [CLIENT] Received ACK");
                return;
            }
        }

        // Update
        if (result.contains("update")) {
            auto updateOpt = result["update"].as_int();
            if (updateOpt) {
                int update = *updateOpt;
                fl::printf("<<< [CLIENT] Received update: %d\n", update);
                leds[update % NUM_LEDS] = CRGB::Purple;
                FastLED.show();
            }
            return;
        }

        // Final
        if (result.contains("stop")) {
            auto stopOpt = result["stop"].as_bool();
            if (stopOpt && *stopOpt) {
                Serial.println("<<< [CLIENT] Received FINAL result");
                waitingForResponse = false;
                return;
            }
        }

        // Regular result
        Serial.print("<<< [CLIENT] Received result: ");
        Serial.println(result.to_string().c_str());
        waitingForResponse = false;
    }
}

void loop() {
    uint32_t now = millis();

    // Update server
    serverTransport->update(now);
    serverRemote->update(now);

    // Update client
    clientTransport->update(now);
    clientRemote->update(now);

    // Process client responses
    fl::optional<fl::Json> response = clientTransport->readRequest();
    if (response) {
        handleClientResponse(*response);
    }

    // Send requests periodically
    if (!waitingForResponse && (now - lastRequestTime >= REQUEST_INTERVAL)) {
        lastRequestTime = now;

        switch (currentMode) {
            case TEST_SYNC:
                sendSyncRequest();
                currentMode = TEST_ASYNC;
                break;

            case TEST_ASYNC:
                sendAsyncRequest();
                currentMode = TEST_ASYNC_STREAM;
                break;

            case TEST_ASYNC_STREAM:
                sendAsyncStreamRequest();
                currentMode = TEST_SYNC;
                Serial.println("\n--- Test cycle complete, starting over ---\n");
                break;
        }
    }

    delay(10);
}
