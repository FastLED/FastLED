// Async, net, OTA, BLE RPC bindings: testAsync, startNetServer, startNetClient, runNetClientTest, runNetLoopback, stopNet, startOta, stopOta, startBle, stopBle, decodeFile, bleStatus.
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/system/heap.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/json.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include "fl/math/wave/wave_perf_bench.h"
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"
#include "AutoResearchWave8Expand.h"
#include "AutoResearchParlioEncode.h"
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"


void AutoResearchRemoteControl::bindAsyncNetMethods(fl::Remote& remote) {
    // Register "testAsync" function - verify that show() returns before TX completes (async DMA)
    // This proves the SPI driver releases back to the main thread while draining.
    remote.bind("testAsync", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Parse optional parameters from args object
        int num_leds = 300;
        fl::string requested_driver;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null()) {
            if (config.contains("numLeds") && config["numLeds"].is_int()) {
                num_leds = static_cast<int>(config["numLeds"].as_int().value());
            }
            if (config.contains("driver") && config["driver"].is_string()) {
                requested_driver = config["driver"].as_string().value();
            }
        }

        // Set exclusive driver if requested (by-name path: requested_driver from RPC)
        if (!requested_driver.empty()) {
            if (!autoResearchSetExclusiveDriverByName(requested_driver.c_str())) {
                response.set("success", false);
                response.set("error", "DriverSetupFailed");
                fl::sstream msg;
                msg << "Failed to set '" << requested_driver.c_str() << "' as exclusive driver";
                response.set("message", msg.str().c_str());
                return response;
            }
        }

        if (num_leds < 10 || num_leds > 1000) {
            response.set("success", false);
            response.set("error", "InvalidNumLeds");
            response.set("message", "numLeds must be 10-1000");
            return response;
        }

        // Set up a channel with the specified number of LEDs
        fl::vector<CRGB> leds(num_leds);
        for (int i = 0; i < num_leds; i++) {
            leds[i] = CRGB(0xFF, 0x00, 0x80);  // Solid color pattern
        }

        fl::ChannelConfig channel_config(
            mState->pin_tx,
            fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
            leds,
            RGB
        );

        auto channel = FastLED.add(channel_config);
        if (!channel) {
            response.set("success", false);
            response.set("error", "ChannelCreationFailed");
            response.set("message", "Failed to create channel");
            return response;
        }

        // === DRAW 1: May include one-time SPI hardware init overhead ===
        uint32_t t0 = micros();
        FastLED.show();
        uint32_t t1 = micros();
        FastLED.wait(5000);  // 5 second timeout
        uint32_t t2 = micros();

        uint32_t show1_us = t1 - t0;
        uint32_t wait1_us = t2 - t1;
        uint32_t total1_us = t2 - t0;

        // === DRAW 2: Should be fast (no init overhead) ===
        uint32_t t3 = micros();
        FastLED.show();
        uint32_t t4 = micros();
        FastLED.wait(5000);  // 5 second timeout
        uint32_t t5 = micros();

        uint32_t show2_us = t4 - t3;
        uint32_t wait2_us = t5 - t4;
        uint32_t total2_us = t5 - t3;

        // Clean up channel
        FastLED.clear(ClearFlags::CHANNELS);

        // Determine if async behavior is working on draw 2 (no init overhead):
        // If async: show_us << total_us (show returns quickly, wait blocks for remainder)
        // If blocking: show_us ≈ total_us (show blocks for entire TX, wait returns instantly)
        // Pass criterion: show_us < 50% of total_us (on draw 2)
        bool passed = (total2_us > 0) && (show2_us < total2_us / 2);

        // Determine driver name
        fl::string driver_name = "unknown";
        for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
            if (mState->drivers_available[i].enabled) {
                driver_name = mState->drivers_available[i].name;
                break;
            }
        }

        response.set("success", true);
        response.set("passed", passed);
        // Draw 1 (with possible init overhead)
        response.set("show1_us", static_cast<int64_t>(show1_us));
        response.set("wait1_us", static_cast<int64_t>(wait1_us));
        response.set("total1_us", static_cast<int64_t>(total1_us));
        // Draw 2 (steady-state, no init)
        response.set("show2_us", static_cast<int64_t>(show2_us));
        response.set("wait2_us", static_cast<int64_t>(wait2_us));
        response.set("total2_us", static_cast<int64_t>(total2_us));
        response.set("num_leds", static_cast<int64_t>(num_leds));
        response.set("driver", driver_name.c_str());

        fl::sstream msg;
        if (passed) {
            msg << "Async OK: draw2 show()=" << show2_us
                << "us, wait()=" << wait2_us
                << "us, total=" << total2_us << "us"
                << " (draw1 show=" << show1_us << "us)";
        } else {
            msg << "Async FAIL: draw2 show()=" << show2_us
                << "us out of " << total2_us
                << "us total (expected <50%)"
                << " (draw1 show=" << show1_us << "us)";
        }
        response.set("message", msg.str().c_str());

        return response;
    });

    // ========================================================================
    // Network Validation RPC Functions
    // ========================================================================

    // Register "startNetServer" - Start WiFi AP + HTTP server for net-server validation
    remote.bind("startNetServer", [this](const fl::json& args) -> fl::json {
        mState->net_server_active = true;
        return startNetServer();
    });

    // Register "startNetClient" - Start WiFi AP only for net-client validation
    remote.bind("startNetClient", [this](const fl::json& args) -> fl::json {
        mState->net_client_active = true;
        return startNetClient();
    });

    // Register "runNetClientTest" - ESP32 fetches from host HTTP server
    // Args: {host_ip: string, port: int}
    remote.bind("runNetClientTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Parse arguments - args is the config object
        if (!args.is_object()) {
            response.set("success", false);
            response.set("error", "Expected object with host_ip and port");
            return response;
        }

        fl::json host_ip_val = args[fl::string("host_ip")];
        fl::json port_val = args[fl::string("port")];

        if (!host_ip_val.is_string() || !port_val.is_int()) {
            response.set("success", false);
            response.set("error", "Expected {host_ip: string, port: int}");
            return response;
        }

        fl::string host_ip = host_ip_val.as_string().value();
        uint16_t port = static_cast<uint16_t>(port_val.as_int().value());

        return runNetClientTest(host_ip.c_str(), port);
    });

    // Register "runNetLoopback" - Self-contained loopback test (no WiFi needed)
    // Starts HTTP server on localhost, client GETs 127.0.0.1 endpoints
    remote.bind("runNetLoopback", [](const fl::json& args) -> fl::json {
        return runNetLoopback();
    });

    // Register "stopNet" - Stop WiFi AP and HTTP server/client
    remote.bind("stopNet", [this](const fl::json& args) -> fl::json {
        mState->net_server_active = false;
        mState->net_client_active = false;
        return stopNet();
    });

    // ========================================================================
    // OTA Validation RPC Functions
    // ========================================================================

    // Register "startOta" - Start WiFi AP + OTA HTTP server for OTA validation
    remote.bind("startOta", [](const fl::json& args) -> fl::json {
        return startOta();
    });

    // Register "stopOta" - Stop OTA server and WiFi AP
    remote.bind("stopOta", [](const fl::json& args) -> fl::json {
        return stopOta();
    });

    // ========================================================================
    // BLE Validation RPC Functions
    // ========================================================================

    // Register "startBle" - Start BLE GATT server + create BLE Remote
    remote.bind("startBle", [this](const fl::json& args) -> fl::json {
        return this->startBleRemote();
    });

    // Register "stopBle" - Stop BLE GATT server + destroy BLE Remote
    remote.bind("stopBle", [this](const fl::json& args) -> fl::json {
        return this->stopBleRemote();
    });

    // Register "decodeFile" - Decode a media file and return first 16 pixels
    // Args: [base64_data_string, extension_string]  (base64 auto-decoded to fl::vector<fl::u8>)
    remote.bind("decodeFile", [](fl::vector<fl::u8> data, fl::string ext) -> fl::json {
        fl::json response = fl::json::object();

        if (ext != ".mp4") {
            response.set("success", false);
            response.set("error", "Only .mp4 supported for device decode");
            return response;
        }

        // Parse MP4 container metadata
        fl::string error;
        fl::H264Info info = fl::H264::parseH264Info(data, &error);
        if (!info.isValid) {
            response.set("success", false);
            response.set("error", error.c_str());
            return response;
        }
        response.set("width", static_cast<int64_t>(info.width));
        response.set("height", static_cast<int64_t>(info.height));

        if (!fl::H264::isSupported()) {
            response.set("success", false);
            response.set("error", "H264 decoder not supported on this platform");
            return response;
        }

        // Create decoder and decode first frame
        fl::string dec_error;
        auto decoder = fl::H264::createDecoder(fl::H264Config{}, &dec_error);
        if (!decoder) {
            response.set("success", false);
            response.set("error", dec_error.c_str());
            return response;
        }

        auto stream = fl::make_shared<fl::memorybuf>(data.size());
        stream->write(data);

        if (!decoder->begin(stream)) {
            fl::string msg;
            decoder->hasError(&msg);
            response.set("success", false);
            response.set("error", msg.empty() ? "Decoder begin() failed" : msg.c_str());
            return response;
        }

        auto result = decoder->decode();
        if (result != fl::DecodeResult::Success) {
            response.set("success", false);
            response.set("error", "Decode returned non-success");
            response.set("decode_result", static_cast<int64_t>(static_cast<int>(result)));
            decoder->end();
            return response;
        }

        fl::Frame frame = decoder->getCurrentFrame();
        decoder->end();

        if (!frame.isValid()) {
            response.set("success", false);
            response.set("error", "Decoded frame is invalid");
            return response;
        }

        response.set("success", true);
        response.set("frame_width", static_cast<int64_t>(frame.getWidth()));
        response.set("frame_height", static_cast<int64_t>(frame.getHeight()));

        // Return first 16 pixels as [[r,g,b], ...]
        auto pixels = frame.rgb();
        fl::json pixel_array = fl::json::array();
        int count = pixels.size() < 16 ? static_cast<int>(pixels.size()) : 16;
        for (int i = 0; i < count; i++) {
            fl::json px = fl::json::array();
            px.push_back(static_cast<int64_t>(pixels[i].r));
            px.push_back(static_cast<int64_t>(pixels[i].g));
            px.push_back(static_cast<int64_t>(pixels[i].b));
            pixel_array.push_back(px);
        }
        response.set("pixels", pixel_array);

        return response;
    });

    // Register "bleStatus" - Query BLE connection/subscription state
    remote.bind("bleStatus", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("ble_active", mState->ble_server_active);
        fl::net::ble::StatusInfo info = fl::net::ble::queryStatus(mBleState);
        response.set("connected", info.connected);
        response.set("connected_count", static_cast<int64_t>(info.connectedCount));
        response.set("tx_char_exists", info.txCharExists);
        response.set("tx_value_len", static_cast<int64_t>(info.txValueLen));
        response.set("ring_head", static_cast<int64_t>(info.ringHead));
        response.set("ring_tail", static_cast<int64_t>(info.ringTail));
        return response;
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
