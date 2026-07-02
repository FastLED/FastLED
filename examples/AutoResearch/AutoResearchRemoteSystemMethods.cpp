// System control RPC bindings: device status, debug toggles, driver
// inventory, RPC ack/health (ping, testNoSerial), and the user-facing
// runSingleTest/runParallelTest dispatchers. "System" = device control,
// not feature exercise (those live in the math/network/async/driver bins).
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
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#endif
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "esp_heap_caps.h"  // ok platform headers - heapCheck RPC
#include "freertos/FreeRTOS.h"  // ok platform headers - heapCheck RPC
#include "freertos/task.h"  // ok platform headers - heapCheck RPC
// IWYU pragma: end_keep
FL_EXTERN_C_END
#endif
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
#include "AutoResearchTimingDrift.h"
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/bus_info.json.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

namespace {

fl::json autoResearchDeviceJson(const fl::string& name) {
    if (name == "RMT") return fl::deviceJson<fl::Bus::RMT>();
    if (name == "SPI" || name == "SPI_UNIFIED") return fl::deviceJson<fl::Bus::SPI>();
    if (name == "UART" || name == "LPUART") return fl::deviceJson<fl::Bus::UART>();
    if (name == "BIT_BANG" || name == "STUB") return fl::deviceJson<fl::Bus::BIT_BANG>();

    if (name == "FLEX_IO" || name == "LCD_RGB") {
        return fl::deviceJson<fl::Bus::FLEX_IO, 1>();
    }
    if (name == "OBJECT_FLED" || name == "PARLIO" || name == "LCD_SPI" ||
        name == "LCD_CLOCKLESS" || name == "I2S" || name == "I2S_SPI") {
        return fl::deviceJson<fl::Bus::FLEX_IO, 0>();
    }

    fl::json info = fl::json::object();
    info.set("bus", static_cast<int64_t>(0));
    info.set("bus_name", name.c_str());
    info.set("vendor_name", name.c_str());
    info.set("device_name", name.c_str());
    info.set("which", static_cast<int64_t>(0));
    info.set("is_noop", false);
    info.set("notes", "registered runtime driver");
    fl::json runtime = fl::json::object();
    runtime.set("available", true);
    runtime.set("initialized", false);
    runtime.set("ready", true);
    runtime.set("busy", false);
    runtime.set("error", "");
    info.set("runtime", runtime);
    return info;
}

}  // namespace


void AutoResearchRemoteControl::bindSystemMethods(fl::Remote& remote) {

    // Register "status" function - device readiness check
    remote.bind("status", [this](const fl::json& args) -> fl::json {
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(mState->pin_tx));
        status.set("pinRx", static_cast<int64_t>(mState->pin_rx));
        return status;
    });

    // NOTE: getSchema is no longer needed - use built-in "rpc.discover" instead
    // The rpc.discover method is automatically available via Remote->Rpc and returns
    // the full OpenRPC schema. Our custom getSchema was causing stack overflow on ESP32-C6.

    // Register "debugTest" function - test RPC argument passing
    remote.bind("debugTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("received", args);
        return response;
    });

    // Register "deliberateHang" - force an infinite loop with interrupts
    // disabled to validate the unified Watchdog implementation (#2731).
    // The watchdog should fire within its configured timeout, reset the
    // device, and let the bootloader become reachable again.
    // Returns success BEFORE hanging so the caller sees the ACK; the hang
    // begins in the next iteration of the loop.
    remote.bind("deliberateHang", [this](const fl::json& args) -> fl::json {
        (void)args;
        mState->deliberate_hang_requested = true;
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "device will hang after RPC returns; watchdog should reset within configured timeout");
        response.set("hangDelayMs", static_cast<int64_t>(200));
        response.set("watchdogExpected", true);
        return response;
    });

    // Register "drivers" function - list available drivers
    remote.bind("drivers", [this](const fl::json& args) -> fl::json {
        fl::json drivers = fl::json::array();
        for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
            fl::json driver = fl::json::object();
            driver.set("name", mState->drivers_available[i].name.c_str());
            driver.set("priority", static_cast<int64_t>(mState->drivers_available[i].priority));
            driver.set("enabled", mState->drivers_available[i].enabled);
            fl::json device = autoResearchDeviceJson(mState->drivers_available[i].name);
            driver.set("bus_name", device["bus_name"]);
            driver.set("vendor_name", device["vendor_name"]);
            driver.set("device_name", device["device_name"]);
            driver.set("which", device["which"]);
            driver.set("is_noop", device["is_noop"]);
            driver.set("runtime", device["runtime"]);
            drivers.push_back(driver);
        }
        return drivers;
    });

    // Returns: {success, passed, totalTests, passedTests, duration_ms, driver,
    //          laneCount, laneSizes, pattern, firstFailure?}
    // ASYNC: Sends ACK immediately, final response sent via sendAsyncResponse()
    // NOTE: runSingleTestImpl() may return early (error cases) without calling
    // sendAsyncResponse(). This wrapper ensures a response is ALWAYS sent so
    // the Python client never times out waiting 120s for a missing response.
    remote.bind("runSingleTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runSingleTestImpl(args);
        // If runSingleTestImpl returned a non-null response, it exited early without
        // calling sendAsyncResponse(). Send it now so the client gets a response.
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runSingleTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // Register "runParallelTest" - test multiple drivers simultaneously
    // Args: {drivers: [{driver: "PARLIO", laneSizes: [100]}, {driver: "LCD_RGB", laneSizes: [100]}],
    //         pattern?: "MSB_LSB_A", iterations?: 1, timing?: "WS2812B-V5"}
    // Returns: {success, passed, duration_ms, show_duration_us, drivers: [...],
    //           rx_validation_attempted, rx_validation_passed}
    remote.bind("runParallelTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runParallelTestImpl(args);
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runParallelTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // ========================================================================
    // Phase 4 Functions: Utility and Control
    // ========================================================================

    // Register "ping" function - health check with timestamp
    remote.bind("ping", [this](const fl::json& args) -> fl::json {
        uint32_t now = millis();

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        return response;
    });

    // TEST: Simple RPC without Serial to verify task context works
    remote.bind("testNoSerial", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "RPC works from task context");
        response.set("serial_safe", false);
        return response;
    });

    // "parlioMetrics" — PARLIO engine ISR/DMA counters (C6 bring-up,
    // FastLED#3576 Phase 7). Reads the engine's debug metrics AFTER a
    // test to see whether the TX ISRs ever fired.
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_PARLIO
    remote.bind("parlioMetrics", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        const auto m = fl::detail::ParlioEngine::getInstance().getDebugMetrics();
        response.set("success", true);
        response.set("isrCount", static_cast<int64_t>(m.mIsrCount));
        response.set("txDoneCount", static_cast<int64_t>(m.mTxDoneCount));
        response.set("chunksQueued", static_cast<int64_t>(m.mChunksQueued));
        response.set("chunksCompleted", static_cast<int64_t>(m.mChunksCompleted));
        response.set("bytesTotal", static_cast<int64_t>(m.mBytesTotal));
        response.set("bytesTransmitted", static_cast<int64_t>(m.mBytesTransmitted));
        response.set("underruns", static_cast<int64_t>(m.mUnderrunCount));
        response.set("errorCode", static_cast<int64_t>(m.mErrorCode));
        response.set("hardwareIdle", m.mHardwareIdle);
        response.set("transmissionActive", m.mTransmissionActive);
        return response;
    });
#endif

    // FastLED#3569 bench diagnostic — read up to 16 u32 words of
    // memory-mapped register space without resetting the chip (esptool
    // pokes force a reset, wiping the very peripheral state under
    // investigation). Params: [{addr: <u32>, count?: 1-16}]. The caller
    // is expected to pass valid peripheral/SRAM addresses; an unmapped
    // address faults exactly as it would from any other code.
    // "heapCheck" — heap integrity + stats (FastLED#3588 corruption
    // bisect). heap_caps_check_integrity_all walks every block; with
    // poisoning configured in the IDF libs it also validates canaries.
#if defined(FL_IS_ESP32)
    remote.bind("heapCheck", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        const bool intact = heap_caps_check_integrity_all(true);
        response.set("success", true);
        response.set("intact", intact);
        response.set("free", static_cast<int64_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
        response.set("minFree", static_cast<int64_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
        response.set("largest", static_cast<int64_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        response.set("loopStackHighWater", static_cast<int64_t>(uxTaskGetStackHighWaterMark(nullptr)));
        return response;
    });
#endif

    remote.bind("peekMem", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        if (!args.is_object() || !args.contains("addr") ||
            !args["addr"].is_int()) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "Expected {addr: u32, count?: 1-16}");
            return response;
        }
        const uint32_t addr =
            static_cast<uint32_t>(args["addr"].as_int().value());
        int count = 1;
        if (args.contains("count") && args["count"].is_int()) {
            count = static_cast<int>(args["count"].as_int().value());
        }
        if (count < 1 || count > 16 || (addr & 3u) != 0) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "count must be 1-16, addr 4-byte aligned");
            return response;
        }
        fl::json values = fl::json::array();
        const volatile uint32_t* p =
            reinterpret_cast<const volatile uint32_t*>(addr);  // ok reinterpret cast - caller-supplied MMIO/SRAM address, bench diagnostic
        for (int i = 0; i < count; ++i) {
            values.push_back(static_cast<int64_t>(p[i]));
        }
        response.set("success", true);
        response.set("addr", static_cast<int64_t>(addr));
        response.set("values", values);
        return response;
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
