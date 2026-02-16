// @filter: (memory is high) && (not avr)

/// @file HttpRpcServer.ino
/// @brief Example demonstrating fl::Remote with HTTP streaming transport (server side)
///
/// This example shows how to:
/// - Create an HTTP streaming RPC server on port 8080
/// - Register SYNC, ASYNC, and ASYNC_STREAM methods
/// - Handle incoming HTTP requests with chunked encoding
/// - Send responses with ResponseSend API
/// - Use heartbeat/keepalive for connection management
///
/// Test with curl or HttpRpcClient.ino:
///   SYNC mode (immediate response):
///     curl -X POST http://localhost:8080/rpc \
///       -H "Content-Type: application/json" \
///       -H "Transfer-Encoding: chunked" \
///       -d '{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}'
///
///   ASYNC mode (ACK + later result):
///     curl -X POST http://localhost:8080/rpc \
///       -H "Content-Type: application/json" \
///       -H "Transfer-Encoding: chunked" \
///       -d '{"jsonrpc":"2.0","method":"longTask","params":[1000],"id":2}'
///
///   ASYNC_STREAM mode (ACK + updates + final):
///     curl -X POST http://localhost:8080/rpc \
///       -H "Content-Type: application/json" \
///       -H "Transfer-Encoding: chunked" \
///       -d '{"jsonrpc":"2.0","method":"streamData","params":[10],"id":3}'
///
/// @see fl/remote/remote.h for full API documentation
/// @see fl/remote/transport/http/PROTOCOL.md for protocol specification

#include <FastLED.h>
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/transport/http/stream_server.h"
#include "fl/remote/transport/http/stream_server.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/native_server.cpp.hpp"

#define NUM_LEDS 10
#define DATA_PIN 3
#define SERVER_PORT 8080

CRGB leds[NUM_LEDS];

// Static pointers to allow setup initialization
static fl::shared_ptr<fl::HttpStreamServer>* pTransport = nullptr;
static fl::Remote* pRemote = nullptr;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect (needed for some boards)
    }

    Serial.println("\n==============================================");
    Serial.println("   HTTP RPC SERVER - Streaming Example");
    Serial.println("==============================================\n");

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Allocate transport and remote on heap
    pTransport = new fl::shared_ptr<fl::HttpStreamServer>(fl::make_shared<fl::HttpStreamServer>(SERVER_PORT));
    auto& transport = *pTransport;

    pRemote = new fl::Remote(
        // RequestSource: read from HTTP server
        [&transport]() { return transport->readRequest(); },
        // ResponseSink: write to HTTP server
        [&transport](const fl::Json& r) { transport->writeResponse(r); }
    );
    auto& remote = *pRemote;

    // Configure heartbeat and timeout
    transport->setHeartbeatInterval(30000); // 30 seconds
    transport->setTimeout(60000); // 60 seconds

    // Connection callbacks
    transport->setOnConnect([]() {
        Serial.println("✓ Client connected");
    });

    transport->setOnDisconnect([]() {
        Serial.println("✗ Client disconnected");
    });

    // ========== SYNC MODE: Immediate response ==========

    remote.bind("add", [](int a, int b) -> int {
        Serial.print("add(");
        Serial.print(a);
        Serial.print(", ");
        Serial.print(b);
        Serial.print(") = ");
        Serial.println(a + b);
        return a + b;
    });

    remote.bind("setLed", [](int index, int r, int g, int b) {
        if (index >= 0 && index < NUM_LEDS) {
            leds[index] = CRGB(r, g, b);
            Serial.print("✓ Set LED ");
            Serial.print(index);
            Serial.print(" to RGB(");
            Serial.print(r);
            Serial.print(", ");
            Serial.print(g);
            Serial.print(", ");
            Serial.print(b);
            Serial.println(")");
        } else {
            Serial.print("✗ LED index ");
            Serial.print(index);
            Serial.println(" out of range");
        }
    });

    remote.bind("fill", [](int r, int g, int b) {
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        Serial.print("✓ Filled all LEDs with RGB(");
        Serial.print(r);
        Serial.print(", ");
        Serial.print(g);
        Serial.print(", ");
        Serial.print(b);
        Serial.println(")");
    });

    remote.bind("getStatus", []() -> fl::Json {
        fl::Json result = fl::Json::object();
        result.set("numLeds", NUM_LEDS);
        result.set("brightness", FastLED.getBrightness());
        result.set("millis", static_cast<int64_t>(millis()));
        Serial.println("✓ Status requested");
        return result;
    });

    // ========== ASYNC MODE: ACK immediately, result later ==========

    remote.bindAsync("longTask", [](fl::ResponseSend& send, const fl::Json& params) {
        // Send ACK immediately
        fl::Json ack = fl::Json::object();
        ack.set("ack", true);
        send.send(ack);

        Serial.println("⏳ Long task started (ASYNC mode)");

        // Extract duration from params (array format: [duration])
        int duration = 1000; // default 1 second
        if (params.is_array() && params.size() > 0) {
            auto durationOpt = params[0].as_int();
            if (durationOpt) {
                duration = *durationOpt;
            }
        }

        // Simulate long-running task
        uint32_t startTime = millis();
        while (millis() - startTime < static_cast<uint32_t>(duration)) {
            delay(100);
        }

        // Send final result
        fl::Json result = fl::Json::object();
        result.set("value", 42);
        result.set("duration", duration);
        send.send(result);

        Serial.print("✓ Long task complete (");
        Serial.print(duration);
        Serial.println(" ms)");
    }, fl::RpcMode::ASYNC);

    // ========== ASYNC_STREAM MODE: ACK + multiple updates + final ==========

    remote.bindAsync("streamData", [](fl::ResponseSend& send, const fl::Json& params) {
        // Send ACK immediately
        fl::Json ack = fl::Json::object();
        ack.set("ack", true);
        send.send(ack);

        Serial.println("📊 Stream started (ASYNC_STREAM mode)");

        // Extract count from params (array format: [count])
        int count = 10; // default 10 updates
        if (params.is_array() && params.size() > 0) {
            auto countOpt = params[0].as_int();
            if (countOpt) {
                count = *countOpt;
            }
        }

        // Send multiple updates
        for (int i = 0; i < count; i++) {
            fl::Json update = fl::Json::object();
            update.set("update", i);
            update.set("progress", (i * 100) / count);
            send.sendUpdate(update);

            Serial.print("  Update ");
            Serial.print(i);
            Serial.print(" (");
            Serial.print((i * 100) / count);
            Serial.println("%)");

            delay(100);
        }

        // Send final result with "stop" marker
        fl::Json final = fl::Json::object();
        final.set("done", true);
        final.set("count", count);
        send.sendFinal(final);

        Serial.println("✓ Stream complete");
    }, fl::RpcMode::ASYNC_STREAM);

    // Start the HTTP server
    if (transport->connect()) {
        Serial.print("✓ HTTP server listening on port ");
        Serial.println(SERVER_PORT);
    } else {
        Serial.print("✗ Failed to start HTTP server on port ");
        Serial.println(SERVER_PORT);
    }

    Serial.println("\n=== Available RPC Methods ===");
    Serial.println("SYNC:");
    Serial.println("  add(a, b) -> sum");
    Serial.println("  setLed(index, r, g, b)");
    Serial.println("  fill(r, g, b)");
    Serial.println("  getStatus() -> {numLeds, brightness, millis}");
    Serial.println("ASYNC:");
    Serial.println("  longTask(duration) -> ACK + {value, duration}");
    Serial.println("ASYNC_STREAM:");
    Serial.println("  streamData(count) -> ACK + updates + {done, count}");
    Serial.println();

    Serial.println("Test with curl:");
    Serial.println("  curl -X POST http://localhost:8080/rpc \\");
    Serial.println("    -H \"Content-Type: application/json\" \\");
    Serial.println("    -H \"Transfer-Encoding: chunked\" \\");
    Serial.println("    -d '{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":[2,3],\"id\":1}'");
    Serial.println();
    Serial.println("Ready for incoming HTTP connections...\n");
}

void loop() {
    if (!pTransport || !pRemote) {
        return;
    }

    // Update transport (handle heartbeat, timeouts, reconnection)
    (*pTransport)->update(millis());

    // Update remote (process incoming requests)
    pRemote->update(millis());

    // Update LEDs
    FastLED.show();

    delay(10);
}
