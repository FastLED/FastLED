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
#include "AutoResearchEdgeProbe.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/parlio_tx.h"  // ok platform headers - parlioRawTest RPC
// IWYU pragma: end_keep
FL_EXTERN_C_END
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
    }, fl::RpcMode::ASYNC);

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
    // "parlioRawTest" — minimal IDF parlio TX (FastLED#3586): 1-bit
    // unit @ requested clock on a pin, transmit an alternating pattern.
    // Bypasses the FastLED engine completely to split engine bugs from
    // IDF-driver/silicon behavior.
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_PARLIO
    remote.bind("parlioRawTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        int pin = 1;
        int hz = 8000000;
        int width = 1;
        int pattern = 0xAA;
        int qnb = 0;
        int mts = 1024;
        int burst = 0;
        int depth = 4;
        int chunks = 1;
        if (args.is_object()) {
            if (args.contains("pin") && args["pin"].is_int()) pin = (int)args["pin"].as_int().value();
            if (args.contains("hz") && args["hz"].is_int()) hz = (int)args["hz"].as_int().value();
            if (args.contains("width") && args["width"].is_int()) width = (int)args["width"].as_int().value();
            if (args.contains("pattern") && args["pattern"].is_int()) pattern = (int)args["pattern"].as_int().value();
            if (args.contains("qnb") && args["qnb"].is_int()) qnb = (int)args["qnb"].as_int().value();
            if (args.contains("mts") && args["mts"].is_int()) mts = (int)args["mts"].as_int().value();
            if (args.contains("burst") && args["burst"].is_int()) burst = (int)args["burst"].as_int().value();
            if (args.contains("depth") && args["depth"].is_int()) depth = (int)args["depth"].as_int().value();
            if (args.contains("chunks") && args["chunks"].is_int()) chunks = (int)args["chunks"].as_int().value();
        }
        parlio_tx_unit_config_t cfg = {};
        cfg.clk_src = PARLIO_CLK_SRC_DEFAULT;
        cfg.data_width = (size_t)width;
        cfg.clk_in_gpio_num = (gpio_num_t)-1;
        cfg.clk_out_gpio_num = (gpio_num_t)-1;
        cfg.valid_gpio_num = (gpio_num_t)-1;
        cfg.trans_queue_depth = (size_t)depth;
        cfg.max_transfer_size = (size_t)mts;
        cfg.output_clk_freq_hz = (uint32_t)hz;
        cfg.sample_edge = PARLIO_SAMPLE_EDGE_POS;
        cfg.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
        if (burst > 0) cfg.dma_burst_size = (size_t)burst;
        for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = (gpio_num_t)-1;
        cfg.data_gpio_nums[0] = (gpio_num_t)pin;
        parlio_tx_unit_handle_t unit = nullptr;
        esp_err_t err = parlio_new_tx_unit(&cfg, &unit);
        if (err != ESP_OK) {
            response.set("success", false);
            response.set("error", esp_err_to_name(err));
            return response;
        }
        err = parlio_tx_unit_enable(unit);
        static uint8_t buf[512];
        for (int i = 0; i < 512; ++i) buf[i] = (uint8_t)pattern;
        parlio_transmit_config_t tcfg = {};
        tcfg.idle_value = 0;
        tcfg.flags.queue_nonblocking = qnb ? 1 : 0;
        if (burst > 0) {
            // dma_burst_size is a unit-config field; already applied below via cfg2 hack? No —
            // it must be set before creation; handled via the 'burst' arg at cfg time.
        }
        esp_err_t terr = ESP_OK;
        const size_t chunk_bytes = 512 / (chunks > 0 ? chunks : 1);
        // Repeat for ~3 s so the edge-probe task (which re-arms 150 ms
        // after the RPC) always overlaps live transmissions.
        for (int rep = 0; rep < 300 && terr == ESP_OK; ++rep) {
            for (int c = 0; c < chunks && terr == ESP_OK; ++c) {
                terr = parlio_tx_unit_transmit(unit, buf + c * chunk_bytes, chunk_bytes * 8, &tcfg);
            }
            if (terr == ESP_OK) {
                (void)parlio_tx_unit_wait_all_done(unit, 100);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        esp_err_t werr = parlio_tx_unit_wait_all_done(unit, 1000);
        parlio_tx_unit_disable(unit);
        parlio_del_tx_unit(unit);
        response.set("success", true);
        response.set("enableErr", esp_err_to_name(err));
        response.set("txErr", esp_err_to_name(terr));
        response.set("waitErr", esp_err_to_name(werr));
        return response;
    });
#endif

    // "edgeProbe" / "edgeProbeRead" — ISR-based edge recorder
    // (FastLED#3586 bring-up): arms a GPIO any-edge interrupt that
    // timestamps up to 200 edges with the cycle counter. Arm it, run a
    // test, then read back exact pulse widths — a 6-ns logic probe for
    // "transmits but capture sees nothing" investigations.
#if defined(FL_IS_ESP32)
    remote.bind("edgeProbe", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        int pin = 0;
        bool passive = false;
        if (args.is_object() && args.contains("pin") && args["pin"].is_int()) {
            pin = static_cast<int>(args["pin"].as_int().value());
        }
        if (args.is_object() && args.contains("passive") && args["passive"].is_bool()) {
            passive = args["passive"].as_bool().value();
        }
        edgeProbeArmMode(pin, passive);
        response.set("success", true);
        response.set("pin", static_cast<int64_t>(pin));
        response.set("selfTestEdges", static_cast<int64_t>(edgeProbeSelfTestEdges()));
        return response;
    });

    remote.bind("edgeProbeRead", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        fl::u32 count = 0;
        const fl::u32 *stamps = edgeProbeStamps(count);
        response.set("edges", static_cast<int64_t>(count));
        response.set("probeState", static_cast<int64_t>(edgeProbeState()));
        {
            const fl::u32 *rl = edgeProbeRmtLive();
            fl::json live = fl::json::array();
            for (int k = 0; k < 8; ++k) live.push_back(static_cast<int64_t>(rl[k]));
            response.set("rmtLive", live);
        }
        fl::json runs = fl::json::array();
        // Convert consecutive timestamps to durations in ns. Entry i
        // encodes (level << 31) | cycle_stamp.
        for (fl::u32 i = 1; i < count && i < 200; ++i) {
            const fl::u32 prev = stamps[i - 1];
            const fl::u32 cur = stamps[i];
            const fl::u32 d_cycles = (cur & 0x7FFFFFFFu) - (prev & 0x7FFFFFFFu);
            const fl::u32 level = prev >> 31;
            // cycles -> ns via configured CPU MHz
            const fl::u64 ns = (static_cast<fl::u64>(d_cycles) * 1000u) / edgeProbeCpuMhz();
            fl::json e = fl::json::object();
            e.set("level", static_cast<int64_t>(level));
            e.set("ns", static_cast<int64_t>(ns));
            runs.push_back(e);
        }
        response.set("runs", runs);
        return response;
    });
#endif

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
#if configUSE_TRACE_FACILITY
        {
            // Per-task stack headroom (words): a near-zero value means
            // that task is about to overflow into adjacent heap
            // (FastLED#3588 candidate mechanism).
            TaskStatus_t statuses[16];
            const UBaseType_t n = uxTaskGetSystemState(statuses, 16, nullptr);
            fl::json tasks = fl::json::array();
            for (UBaseType_t i = 0; i < n; ++i) {
                fl::json t = fl::json::object();
                t.set("name", statuses[i].pcTaskName);
                t.set("hw", static_cast<int64_t>(statuses[i].usStackHighWaterMark));
                tasks.push_back(t);
            }
            response.set("tasks", tasks);
        }
#endif
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
