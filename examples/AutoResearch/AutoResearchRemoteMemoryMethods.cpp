// Request-scoped public-API memory benches (#3179 and #4733).
#include "fl/system/sketch_macros.h"

#if FL_PLATFORM_HAS_LARGE_MEMORY && defined(FL_AUTORESEARCH_MEMORY_BENCH)

#include <FastLED.h>
#include "fl/channels/config.h"
#include "fl/remote/remote.h"
#include "fl/stl/json.h"
#include "fl/stl/vector.h"
#include "fl/stl/singleton.h"
#include "fl/stl/static_assert.h"
#include "fl/system/heap.h"
#include "fl/stl/chrono.h"

#if FL_AUTORESEARCH_MEMORY_BENCH == 3
#include "driver/spi_slave.h" // ok platform specific - a second native SPI peripheral is the independent wire receiver; no neutral core SPI-slave API exists.
#include "driver/gpio.h" // ok platform specific - preserve master output while sampling its pads with SPI3.
#include "esp_rom_gpio.h" // ok platform specific - SPI3 input initialization disconnects the existing SPI2 output matrix routes.
#include "soc/gpio_sig_map.h" // ok platform specific - vendor-owned S3 signal IDs restore SPI2 output while preserving SPI3 input.
#include "esp_heap_caps.h" // ok platform specific - measure and allocate the independent receiver's internal DMA storage.
#include "esp_idf_version.h" // ok platform specific - local heap watermark measurement requires the current SDK API.
#include "soc/soc_caps.h" // ok platform specific - verify that this target actually exposes SPI3.
#endif

namespace {

int integerArg(const fl::json& args, const char* name, int fallback) {
    return static_cast<int>(args[name].as_int().value_or(fallback));
}

#if FL_AUTORESEARCH_MEMORY_BENCH != 3
fl::json bindingBench(const fl::json& args) {
    fl::json result = fl::json::object();
    const int pin = integerArg(args, "pin", 4);
    fl::vector<CRGB> leds(32, CRGB::Red);
    fl::ChannelConfig cfg(fl::makeClockless<fl::TIMING_WS2812_800KHZ>(pin), leds, RGB);
    // Hold the selected backend constant: the two builds differ only in
    // whether registration retains the selected driver or every driver.
    cfg.options.mBus = fl::DefaultBus<fl::ClocklessChipset>::value;
    const fl::size heap_before = fl::getFreeHeap().free_sram;
    const fl::size drivers_before = FastLED.getDriverCount();
#if FL_AUTORESEARCH_MEMORY_BENCH == 1
    auto channel = FastLED.add<fl::Bus::AUTO>(cfg);
#else
    auto channel = FastLED.add(cfg);
#endif
    const fl::size heap_registered = fl::getFreeHeap().free_sram;
    const fl::size drivers_registered = FastLED.getDriverCount();
    bool completed = channel != nullptr;
    fl::size heap_first = heap_registered;
    fl::size heap_steady = heap_registered;
    for (int frame = 0; completed && frame < 20; ++frame) {
        FastLED.show();
        completed = FastLED.wait(1000);
        if (frame == 0) heap_first = fl::getFreeHeap().free_sram;
        if (frame == 2) heap_steady = fl::getFreeHeap().free_sram;
        FastLED.watchdog().feed();
    }
    const fl::size heap_end = fl::getFreeHeap().free_sram;
    const fl::string actual_driver = channel ? channel->getEngineName() : fl::string();
    FastLED.remove(channel);
    channel.reset();
    const bool selected_drivers = FL_AUTORESEARCH_MEMORY_BENCH == 1
        ? drivers_registered == 1 : drivers_registered > 0;
    result.set("success", completed && drivers_before == 0 &&
               selected_drivers && heap_steady == heap_end);
    result.set("heapBefore", static_cast<int64_t>(heap_before));
    result.set("heapAfterRegister", static_cast<int64_t>(heap_registered));
    result.set("registrationBytes", static_cast<int64_t>(heap_before) - static_cast<int64_t>(heap_registered));
    result.set("heapAfterFirst", static_cast<int64_t>(heap_first));
    result.set("heapSteady", static_cast<int64_t>(heap_steady));
    result.set("heapEnd", static_cast<int64_t>(heap_end));
    result.set("driversBefore", static_cast<int64_t>(drivers_before));
    result.set("driverCount", static_cast<int64_t>(drivers_registered));
    result.set("selectedBus", static_cast<int>(cfg.options.mBus));
    result.set("selectedBusName", fl::busName(cfg.options.mBus));
    result.set("selectedDriverName", fl::busDriverName(cfg.options.mBus));
    result.set("actualDriverName", actual_driver);
    return result;
}
#endif

#if FL_AUTORESEARCH_MEMORY_BENCH == 3

FL_STATIC_ASSERT(SOC_SPI_PERIPH_NUM > 2, "SPI memory bench requires independent SPI2 and SPI3 peripherals"); // ok platform specific - require two real GP-SPI peripherals for independent TX/RX validation.

// SPI3 samples the same physical MOSI/SCLK pads driven by SPI2, with input
// enabled alongside output. CS is a bench-only GPIO held low over the WHOLE
// frame, including every streamed chunk, then raised to finish the receiver.
// ESP32-S3 SPI2/SPI3 exist in soc_caps.h (SOC_SPI_PERIPH_NUM == 3), and are
// documented in the ESP32-S3 TRM "SPI Controller" chapter and the SDK SPI
// Slave Driver guide. This measures SPI bits, rather than re-reading TX RAM.
struct SpiWireReceiver {
    fl::u8* bytes = nullptr;
    spi_slave_transaction_t transaction = {};
    bool initialized = false;
    bool queued = false;
    volatile bool armed = false;
    const char* stage = "idle";
    esp_err_t error = ESP_OK;

    static void ready(spi_slave_transaction_t* transaction) {
        static_cast<SpiWireReceiver*>(transaction->user)->armed = true;
    }

    bool begin(fl::size byte_count) {
        stage = "allocateReceiver";
        bytes = static_cast<fl::u8*>(heap_caps_malloc(byte_count + 4, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)); // ok platform specific - allocate SPI3 RX storage with the SDK DMA capability; the neutral heap API cannot request DMA memory.
        if (!bytes) return false;
        spi_bus_config_t bus = {};
        bus.mosi_io_num = 1;
        bus.miso_io_num = -1;
        bus.sclk_io_num = 3;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = static_cast<int>(byte_count + 4);
        spi_slave_interface_config_t slave = {};
        slave.spics_io_num = 4;
        slave.queue_size = 1;
        slave.mode = 0;
        slave.post_setup_cb = &ready;
        stage = "initializeReceiver";
        error = spi_slave_initialize(SPI3_HOST, &bus, &slave, SPI_DMA_CH_AUTO); // ok platform specific - create the independent SPI3 wire receiver; there is no neutral SPI-slave API.
        if (error != ESP_OK) return false;
        initialized = true;
        // Slave init enables its GPIO inputs but disables the pad outputs.
        // Restore the already configured master output without changing its
        // GPIO matrix signal, then retain input for independent SPI3 sampling.
        gpio_set_direction(GPIO_NUM_1, GPIO_MODE_INPUT_OUTPUT); // ok platform specific - SPI3 input setup disabled the master MOSI output; retain input and output on the shared pad.
        gpio_set_direction(GPIO_NUM_3, GPIO_MODE_INPUT_OUTPUT); // ok platform specific - SPI3 input setup disabled the master clock output; retain input and output on the shared pad.
        gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT_OUTPUT); // ok platform specific - sample the software-controlled frame-wide CS through the same pad.
        // SPI slave bus initialization routes these pads as inputs, replacing
        // their output selections with SIG_GPIO_OUT_IDX. Output-enable alone
        // does not reconnect the master. S3 gpio_sig_map.h names SPI2's data
        // and clock FSPID_OUT_IDX/FSPICLK_OUT_IDX (103/101); SPI3 has separate
        // input selectors, so both peripherals can observe the same pads.
        esp_rom_gpio_connect_out_signal(1, FSPID_OUT_IDX, false, false); // ok platform specific - restore the measured SPI2 MOSI pad.
        esp_rom_gpio_connect_out_signal(3, FSPICLK_OUT_IDX, false, false); // ok platform specific - restore the measured SPI2 clock pad.
        esp_rom_gpio_connect_out_signal(4, SIG_GPIO_OUT_IDX, false, false); // ok platform specific - software GPIO owns the frame-wide slave CS.
        esp_rom_gpio_connect_in_signal(1, SPI3_D_IN_IDX, false); // ok platform specific - independently sample SPI2 data through SPI3.
        esp_rom_gpio_connect_in_signal(3, SPI3_CLK_IN_IDX, false); // ok platform specific - independently sample SPI2 clock through SPI3.
        esp_rom_gpio_connect_in_signal(4, SPI3_CS0_IN_IDX, false); // ok platform specific - slave frame ends only on software CS rising.
        gpio_set_level(GPIO_NUM_4, 1); // ok platform specific - deassert the independent SPI3 receiver CS without changing the measured master API.
        transaction.length = (byte_count + 4) * 8;
        transaction.rx_buffer = bytes;
        transaction.user = this;
        return true;
    }

    bool arm() {
        stage = "queueReceiver";
        armed = false;
        transaction.trans_len = 0;
        error = spi_slave_queue_trans(SPI3_HOST, &transaction, 0); // ok platform specific - arm an independent DMA wire capture; the neutral channel RX API cannot capture clocked SPI.
        if (error != ESP_OK) return false;
        queued = true;
        const fl::u32 started = fl::micros();
        stage = "waitReceiverArmed";
        while (!armed && fl::micros() - started < 1000000u) {}
        if (!armed) { error = ESP_ERR_TIMEOUT; return false; }
        stage = "capture";
        gpio_set_level(GPIO_NUM_4, 0); // ok platform specific - assert independent SPI3 receiver CS over the entire streamed frame.
        return true;
    }

    bool finish() {
        stage = "drainReceiver";
        gpio_set_level(GPIO_NUM_4, 1); // ok platform specific - deassert the independent SPI3 receiver CS without changing the measured master API.
        spi_slave_transaction_t* completed = nullptr;
        error = spi_slave_get_trans_result(SPI3_HOST, &completed, pdMS_TO_TICKS(1000)); // ok platform specific - drain the independent SPI3 receiver before checking captured bytes.
        if (error != ESP_OK) return false;
        queued = false;
        stage = "captured";
        return completed == &transaction;
    }

    void close() {
        // On timeout the singleton retains the descriptor and DMA buffer until
        // board reset. Return diagnostics instead of freeing SDK-owned memory.
        if (queued) return;
        if (initialized) spi_slave_free(SPI3_HOST); // ok platform specific - release only the independent receiver peripheral after its DMA transaction drains.
        heap_caps_free(bytes); // ok platform specific - return SPI3 RX storage to the same SDK allocator that supplied it.
        bytes = nullptr;
        initialized = false;
    }
};

fl::json spiBench(const fl::json& args) {
    fl::json result = fl::json::object();
    static bool already_run = false;
    if (already_run) {
        result.set("success", false);
        result.set("error", "ResetBoardBeforeNextBench");
        return result;
    }
    const int pixels = integerArg(args, "pixels", 1000);
    const int frames = integerArg(args, "frames", 20);
    const fl::string chipset = args["chipset"].as_string().value_or("apa102");
    if (pixels < 1 || pixels > 2048 || frames < 3 || frames > 100 ||
        (chipset != "apa102" && chipset != "sk9822")) {
        result.set("success", false);
        result.set("error", "InvalidBenchArguments");
        return result;
    }
    already_run = true;
    // The registered legacy controller retains the array through firmware
    // lifetime. Static ownership avoids the common bench stack-pointer bug.
    static fl::vector<CRGB> leds;
    leds.resize(pixels);
    for (int i = 0; i < pixels; ++i) leds[i] = CRGB(255, static_cast<fl::u8>(i * 13 + 2), static_cast<fl::u8>(i * 7 + 3));
    const fl::size heap_before = fl::getFreeHeap().free_sram;
    if (chipset == "apa102") {
        FastLED.addLeds<APA102, 1, 3, RGB, DATA_RATE_MHZ(6)>(leds.data(), pixels, fl::Esp32SpiBus::HOST2); // ok platform specific - the independent receiver requires this physical native SPI host; no neutral host selector exists.
    } else {
        FastLED.addLeds<SK9822, 1, 3, RGB, DATA_RATE_MHZ(6)>(leds.data(), pixels, fl::Esp32SpiBus::HOST2); // ok platform specific - the independent receiver requires this physical native SPI host; no neutral host selector exists.
    }
    FastLED.setBrightness(255);
    FastLED.setDither(0);
    FastLED.setCorrection(CRGB(255, 255, 255));
    FastLED.setTemperature(CRGB(255, 255, 255));
    const fl::size heap_registered = fl::getFreeHeap().free_sram;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) // ok platform specific - scoped heap-watermark APIs are available in this validated IDF range, with no neutral equivalent.
    const fl::size prime_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // ok platform specific - sample internal byte-addressable heap with the same capability mask as the scoped watermark.
    heap_caps_monitor_local_minimum_free_size_start(); // ok platform specific - start a scoped allocation watermark instead of inheriting boot-time lows; no neutral API exists.
#endif
    FastLED.show();
    const bool prime_completed = FastLED.wait(1000);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) // ok platform specific - scoped heap-watermark APIs are available in this validated IDF range, with no neutral equivalent.
    const fl::size prime_low = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // ok platform specific - read the scoped internal-byte heap watermark using the pre-transfer capability mask.
    heap_caps_monitor_local_minimum_free_size_stop(); // ok platform specific - end the SDK-local watermark window before unrelated RPC result allocations.
#endif
    const fl::size heap_prime = fl::getFreeHeap().free_sram;
    if (!prime_completed) {
        result.set("success", false);
        result.set("error", "PrimeFailed");
        return result;
    }
    const fl::size frame_bytes = 4 + 4 * pixels + 4 * (pixels / 32 + 1);
    SpiWireReceiver& receiver = fl::Singleton<SpiWireReceiver>::instance();
    if (!receiver.begin(frame_bytes)) {
        result.set("success", false);
        result.set("error", "Spi3ReceiverInitFailed");
        result.set("stage", receiver.stage);
        result.set("sdkError", static_cast<int>(receiver.error));
        receiver.close();
        return result;
    }
    fl::vector<fl::u32> timings;
    timings.reserve(frames);
    fl::vector<fl::u32> captured_bits;
    captured_bits.reserve(frames);
    bool completed = true;
    fl::size mismatches = 0;
    fl::size heap_steady = 0;
    fl::size heap_end = 0;
    fl::size peak_internal_bytes = 0;
    fl::size peak_dma_bytes = 0;
    fl::size first_bad_offset = frame_bytes;
    int first_expected = -1;
    int first_actual = -1;
    fl::size min_captured_bits = frame_bytes * 8;
    fl::size max_captured_bits = 0;
    for (int frame = 0; frame < frames && completed; ++frame) {
        // Change every payload across frames so a stale staging refill fails.
        for (int i = 0; i < pixels; ++i) leds[i] = CRGB(255, static_cast<fl::u8>(i * 13 + frame), static_cast<fl::u8>(i * 7 + frame * 3));
        if (!receiver.arm()) { completed = false; break; }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) // ok platform specific - scoped heap-watermark APIs are available in this validated IDF range, with no neutral equivalent.
        const fl::size before_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // ok platform specific - sample internal byte-addressable heap with the same capability mask as the scoped watermark.
        const fl::size before_dma = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA); // ok platform specific - sample internal DMA-capable heap separately from general SRAM.
        if (heap_caps_monitor_local_minimum_free_size_start() != ESP_OK) { completed = false; break; } // ok platform specific - start a scoped allocation watermark instead of inheriting boot-time lows; no neutral API exists.
#endif
        const fl::u32 started = fl::micros();
        FastLED.show();
        completed = FastLED.wait(1000);
        const fl::u32 elapsed = fl::micros() - started;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) // ok platform specific - scoped heap-watermark APIs are available in this validated IDF range, with no neutral equivalent.
        const fl::size low_internal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // ok platform specific - read the scoped internal-byte heap watermark using the pre-transfer capability mask.
        const fl::size low_dma = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA); // ok platform specific - read the scoped internal-DMA heap watermark using the pre-transfer capability mask.
        heap_caps_monitor_local_minimum_free_size_stop(); // ok platform specific - end the SDK-local watermark window before unrelated RPC result allocations.
        if (before_internal >= low_internal && before_internal - low_internal > peak_internal_bytes) peak_internal_bytes = before_internal - low_internal;
        if (before_dma >= low_dma && before_dma - low_dma > peak_dma_bytes) peak_dma_bytes = before_dma - low_dma;
#endif
        completed = receiver.finish() && completed;
        if (!completed) break;
        const fl::size received_bits = receiver.transaction.trans_len;
        captured_bits.push_back(static_cast<fl::u32>(received_bits));
        if (received_bits < min_captured_bits) min_captured_bits = received_bits;
        if (received_bits > max_captured_bits) max_captured_bits = received_bits;
        const fl::size received_bytes = received_bits / 8 < frame_bytes
            ? received_bits / 8 : frame_bytes;
        mismatches += received_bits != frame_bytes * 8;
        // Never treat untouched receiver storage as captured data. A missing
        // frame is a length failure with actual=-1, not a random byte mismatch.
        if (received_bytes < frame_bytes && first_bad_offset == frame_bytes) {
            first_bad_offset = received_bytes;
        }
        for (fl::size offset = 0; offset < received_bytes; ++offset) {
            fl::u8 expected = 0xFF;
            if (offset < 4) expected = 0;
            else if (offset < 4 + 4 * pixels) {
                const fl::size pixel = (offset - 4) / 4;
                const fl::size component = (offset - 4) % 4;
                if (component != 0) expected = leds[pixel].raw[component - 1];
            }
            if (receiver.bytes[offset] != expected) {
                ++mismatches;
                if (first_bad_offset == frame_bytes) {
                    first_bad_offset = offset;
                    first_expected = expected;
                    first_actual = receiver.bytes[offset];
                }
            }
        }
        timings.push_back(elapsed);
        heap_end = fl::getFreeHeap().free_sram;
        if (frame == 2) heap_steady = heap_end;
        FastLED.watchdog().feed();
    }
    result.set("success", completed && mismatches == 0 && heap_steady == heap_end);
    result.set("chipset", chipset);
    result.set("pixels", pixels);
    result.set("frames", static_cast<int64_t>(timings.size()));
    result.set("frameBytes", static_cast<int64_t>(frame_bytes));
    result.set("wireMismatches", static_cast<int64_t>(mismatches));
    result.set("stage", receiver.stage);
    result.set("sdkError", static_cast<int>(receiver.error));
    result.set("capturedBits", static_cast<int64_t>(receiver.transaction.trans_len));
    result.set("minCapturedBits", static_cast<int64_t>(min_captured_bits));
    result.set("maxCapturedBits", static_cast<int64_t>(max_captured_bits));
    result.set("firstBadOffset", first_bad_offset == frame_bytes ? -1 : static_cast<int64_t>(first_bad_offset));
    result.set("firstExpected", first_expected);
    result.set("firstActual", first_actual);
    result.set("heapBefore", static_cast<int64_t>(heap_before));
    result.set("heapAfterRegister", static_cast<int64_t>(heap_registered));
    result.set("heapAfterPrime", static_cast<int64_t>(heap_prime));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) // ok platform specific - scoped heap-watermark APIs are available in this validated IDF range, with no neutral equivalent.
    result.set("primeBeforeInternal", static_cast<int64_t>(prime_before));
    result.set("primeLowInternal", static_cast<int64_t>(prime_low));
    result.set("primePeakInternalBytes", prime_before >= prime_low ? static_cast<int64_t>(prime_before - prime_low) : 0);
#endif
    result.set("heapSteady", static_cast<int64_t>(heap_steady));
    result.set("heapEnd", static_cast<int64_t>(heap_end));
    result.set("peakInternalBytes", static_cast<int64_t>(peak_internal_bytes));
    result.set("peakDmaBytes", static_cast<int64_t>(peak_dma_bytes));
    result.set("heapWatermarkMeasured", ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)); // ok platform specific - report whether the SDK-specific scoped-watermark path was compiled.
    fl::json times = fl::json::array();
    for (const fl::u32 us : timings) times.push_back(static_cast<int64_t>(us));
    result.set("frameUs", times);
    fl::json bit_counts = fl::json::array();
    for (const fl::u32 bits : captured_bits) bit_counts.push_back(static_cast<int64_t>(bits));
    result.set("frameBits", bit_counts);
    receiver.close();
    return result;
}
#endif

} // namespace

void bindMemoryBench(fl::Remote& remote) {
    remote.bind("memoryBench", [](const fl::json& args) -> fl::json {
#if FL_AUTORESEARCH_MEMORY_BENCH == 3
        fl::json result = spiBench(args);
#else
        fl::json result = bindingBench(args);
#endif
        result.set("mode", FL_AUTORESEARCH_MEMORY_BENCH);
        return result;
    });
}

#endif
