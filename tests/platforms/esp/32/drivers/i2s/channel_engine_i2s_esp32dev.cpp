/// @file channel_engine_i2s_esp32dev.cpp
/// @brief Host unit tests for `ChannelEngineI2sEsp32Dev` (#3471) —
///        exercises the engine's state machine + buffer lifecycle +
///        peripheral interaction end-to-end against
///        `I2sPeripheralEsp32DevMock`.
///
/// Follows the singleton-mock + `MockWrapper` forwarder pattern from
/// the S3 sibling (`tests/.../channel_driver_i2s.cpp`), which is the
/// mandatory shape for peripheral-mock consumption per project
/// conventions (see `.coderabbit.yaml` and
/// `agents/docs/cpp-standards.md`).
///
/// Single-threaded by design — no `std::thread`, no async callbacks.
/// Tests advance simulation manually via
/// `mock.simulateTransmitDone()`.

#ifdef FASTLED_STUB_IMPL

#include "platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.h"
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_mock.h"

#include "fl/channels/config.h"  // for SpiChipsetConfig / SpiEncoder
#include "fl/channels/data.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/memory.h"
#include "test.h"

using namespace fl;

namespace {

/// @brief Reset mock state between tests. The mock is a singleton so
///        state persists across cases without this.
void resetMockState() { I2sPeripheralEsp32DevMock::instance().reset(); }

/// @brief Wrap the singleton mock in a `shared_ptr<II2sPeripheralEsp32Dev>`.
///
/// The engine ctor takes `shared_ptr<II2sPeripheralEsp32Dev>`, which
/// implies ownership. The mock is a singleton so it must not be
/// destroyed when the engine's `shared_ptr` refcount hits zero. This
/// forwarding class solves the mismatch — the shared_ptr owns a
/// lightweight wrapper that forwards every call into the singleton.
/// Follows the S3 sibling's exact pattern
/// (`tests/.../channel_driver_i2s.cpp:43-87`).
fl::shared_ptr<II2sPeripheralEsp32Dev> createMockPeripheral() {
    class MockWrapper : public II2sPeripheralEsp32Dev {
      public:
        bool initialize(const I2sEsp32DevPeripheralConfig &cfg)
            FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().initialize(cfg);
        }
        void deinitialize() FL_NO_EXCEPT override {
            I2sPeripheralEsp32DevMock::instance().deinitialize();
        }
        bool isInitialized() const FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().isInitialized();
        }
        u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().allocateBuffer(
                size_bytes);
        }
        void freeBuffer(u8 *buffer) FL_NO_EXCEPT override {
            I2sPeripheralEsp32DevMock::instance().freeBuffer(buffer);
        }
        bool transmit(const u8 *buffer,
                      size_t size_bytes) FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().transmit(buffer,
                                                                  size_bytes);
        }
        bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().waitTransmitDone(
                timeout_ms);
        }
        bool isBusy() const FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().isBusy();
        }
        bool registerTransmitCallback(I2sEsp32DevTxDoneCallback cb,
                                      void *user_ctx) FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance()
                .registerTransmitCallback(cb, user_ctx);
        }
        bool routeLanePin(u8 lane, i32 gpio_pin) FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().routeLanePin(lane,
                                                                     gpio_pin);
        }
        const I2sEsp32DevPeripheralConfig &
        getConfig() const FL_NO_EXCEPT override {
            return I2sPeripheralEsp32DevMock::instance().getConfig();
        }
    };
    return fl::make_shared<MockWrapper>();
}

/// @brief Build a minimal `ChannelData` with N bytes of test pixel
///        payload. Marked clockless via the (pin, timing) legacy
///        factory so `canHandle` accepts it as clockless.
ChannelDataPtr makeChannelData(int pin, size_t byte_count,
                               u8 fill_byte = 0x00) {
    ChipsetTimingConfig timing;
    timing.t1_ns = 350;
    timing.t2_ns = 300;
    timing.t3_ns = 550;

    fl::vector_psram<u8> encoded;
    encoded.resize(byte_count);
    for (size_t i = 0; i < byte_count; ++i) {
        encoded[i] = fill_byte;
    }

    return ChannelData::create(pin, timing, fl::move(encoded));
}

/// @brief Build a `ChannelData` marked as SPI chipset (APA102-style)
///        so the engine's Phase 2c dispatch treats it as SPI. Used by
///        the Phase 2c dispatch tests below to confirm the SPI branch
///        rejects cleanly rather than silently sending garbage.
ChannelDataPtr makeSpiChannelData(int dataPin, int clockPin,
                                  size_t byte_count,
                                  u8 fill_byte = 0x00) {
    SpiEncoder timing{SpiChipset::APA102, /*clockHz=*/6000000};
    SpiChipsetConfig spi_cfg(dataPin, clockPin, timing);
    ChipsetVariant chipset(spi_cfg);

    fl::vector_psram<u8> encoded;
    encoded.resize(byte_count);
    for (size_t i = 0; i < byte_count; ++i) {
        encoded[i] = fill_byte;
    }
    return ChannelData::create(chipset, fl::move(encoded));
}

} // anonymous namespace

//=============================================================================
// Constructor + peripheral wiring
//=============================================================================

FL_TEST_CASE("I2sEsp32Dev - constructor registers completion callback") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();
    FL_CHECK(mock.registeredCallback() != nullptr);
    FL_CHECK(mock.registeredCallbackUserCtx() == &engine);
}

FL_TEST_CASE("I2sEsp32Dev - engine name is I2S") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    FL_CHECK(engine.getName() == fl::string::from_literal("I2S"));
}

FL_TEST_CASE(
    "I2sEsp32Dev - reports Capabilities(true,true) per parallel-IO rule") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto caps = engine.getCapabilities();
    FL_CHECK(caps.supportsClockless);
    FL_CHECK(caps.supportsSpi);
}

FL_TEST_CASE("I2sEsp32Dev - canHandle rejects null but accepts data") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    FL_CHECK_FALSE(engine.canHandle(ChannelDataPtr()));
    auto data = makeChannelData(/*pin=*/5, /*bytes=*/9);
    FL_CHECK(engine.canHandle(data));
}

//=============================================================================
// State machine
//=============================================================================

FL_TEST_CASE("I2sEsp32Dev - initial state is READY") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("I2sEsp32Dev - show() with no enqueued channels stays READY") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    engine.show();
    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::READY);
    FL_CHECK(I2sPeripheralEsp32DevMock::instance().getTransmitCount() == 0u);
}

FL_TEST_CASE("I2sEsp32Dev - show() with data initializes peripheral once") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 9));
    FL_CHECK_FALSE(mock.isInitialized());
    engine.show();
    FL_CHECK(mock.isInitialized());

    // Second show() — already-initialized peripheral is reused.
    mock.simulateTransmitDone();
    engine.poll();
    engine.enqueue(makeChannelData(1, 9));
    engine.show();
    // Still initialized — no re-init.
    FL_CHECK(mock.isInitialized());
}

FL_TEST_CASE("I2sEsp32Dev - show() moves state to BUSY, transmit fires") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 12, 0xAA));
    engine.show();

    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::BUSY);
    FL_CHECK(mock.getTransmitCount() == 1u);
    FL_CHECK(mock.isBusy());
}

FL_TEST_CASE("I2sEsp32Dev - transmit records the packed byte payload") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    // Two strips, each with a distinct fill byte. Stage-1 packing is
    // linear concatenation — the mock's history captures the exact
    // bytes the peripheral saw.
    engine.enqueue(makeChannelData(0, 3, 0xAA));
    engine.enqueue(makeChannelData(1, 3, 0xBB));
    engine.show();

    const auto &hist = mock.getTransmitHistory();
    FL_REQUIRE(hist.size() == 1u);
    const auto &rec = hist[0];
    FL_CHECK(rec.size_bytes == 6u);
    FL_REQUIRE(rec.buffer_copy.size() == 6u);
    FL_CHECK(rec.buffer_copy[0] == 0xAA);
    FL_CHECK(rec.buffer_copy[1] == 0xAA);
    FL_CHECK(rec.buffer_copy[2] == 0xAA);
    FL_CHECK(rec.buffer_copy[3] == 0xBB);
    FL_CHECK(rec.buffer_copy[4] == 0xBB);
    FL_CHECK(rec.buffer_copy[5] == 0xBB);
}

FL_TEST_CASE("I2sEsp32Dev - simulate completion drives BUSY -> READY on poll") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 9));
    engine.show();
    FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::BUSY);

    mock.simulateTransmitDone();
    FL_CHECK(mock.callbackInvocationCount() == 1u);
    // simulateTransmitDone runs the callback which flips the engine's
    // completion flag. poll() picks it up and drops to READY.
    auto state = engine.poll();
    FL_CHECK(state == IChannelDriver::DriverState::READY);
    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("I2sEsp32Dev - poll() while BUSY without completion stays BUSY") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    engine.enqueue(makeChannelData(0, 9));
    engine.show();
    FL_CHECK(engine.poll() == IChannelDriver::DriverState::BUSY);
    FL_CHECK(engine.poll() == IChannelDriver::DriverState::BUSY);
}

//=============================================================================
// Buffer lifecycle
//=============================================================================

FL_TEST_CASE("I2sEsp32Dev - show() allocates a DMA-capable scratch buffer") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 30));
    engine.show();

    FL_CHECK(mock.getAllocateBufferCount() >= 1u);
    FL_CHECK(engine.currentScratchBufferSize() >= 30u);
    FL_CHECK(mock.getOutstandingBufferCount() >= 1u);
}

FL_TEST_CASE(
    "I2sEsp32Dev - subsequent show() reuses scratch buffer if large enough") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 30));
    engine.show();
    size_t first_alloc = mock.getAllocateBufferCount();

    mock.simulateTransmitDone();
    engine.poll();

    // Second show() with same-size payload — should not reallocate.
    engine.enqueue(makeChannelData(0, 30));
    engine.show();
    FL_CHECK(mock.getAllocateBufferCount() == first_alloc);
}

FL_TEST_CASE("I2sEsp32Dev - larger frame triggers reallocation") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 30));
    engine.show();
    size_t first_alloc = mock.getAllocateBufferCount();

    mock.simulateTransmitDone();
    engine.poll();

    engine.enqueue(makeChannelData(0, 300));
    engine.show();
    FL_CHECK(mock.getAllocateBufferCount() == first_alloc + 1u);
    FL_CHECK(engine.currentScratchBufferSize() >= 300u);
}

FL_TEST_CASE("I2sEsp32Dev - destructor frees the scratch buffer") {
    resetMockState();
    {
        ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
        engine.enqueue(makeChannelData(0, 30));
        engine.show();
        FL_REQUIRE(
            I2sPeripheralEsp32DevMock::instance().getOutstandingBufferCount() ==
            1u);
        I2sPeripheralEsp32DevMock::instance().simulateTransmitDone();
        engine.poll();
    }
    // After the engine destructs, there must be no leaked
    // buffer allocation.
    FL_CHECK(
        I2sPeripheralEsp32DevMock::instance().getOutstandingBufferCount() ==
        0u);
}

//=============================================================================
// Error injection
//=============================================================================

FL_TEST_CASE(
    "I2sEsp32Dev - initialize failure drops to ERROR and clears enqueued") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();
    mock.setInitializeFailure(true);
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());

    auto data = makeChannelData(0, 9);
    engine.enqueue(data);
    engine.show();

    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(data->isInUse());
}

FL_TEST_CASE("I2sEsp32Dev - allocateBuffer failure drops to ERROR") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());

    engine.enqueue(makeChannelData(0, 9));
    mock.setAllocateBufferFailure(true);
    engine.show();

    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::ERROR);
}

FL_TEST_CASE("I2sEsp32Dev - transmit failure drops to ERROR + no busy state") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());

    engine.enqueue(makeChannelData(0, 9));
    mock.setTransmitFailure(true);
    engine.show();

    FL_CHECK(engine.currentState() == IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(mock.isBusy());
}

//=============================================================================
// setInUse channel accounting
//=============================================================================

FL_TEST_CASE("I2sEsp32Dev - marks channels inUse for the transmit window") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    auto data = makeChannelData(0, 9);
    FL_REQUIRE_FALSE(data->isInUse());

    engine.enqueue(data);
    engine.show();
    FL_CHECK(data->isInUse());

    mock.simulateTransmitDone();
    engine.poll();
    FL_CHECK_FALSE(data->isInUse());
}

FL_TEST_CASE("I2sEsp32Dev - enqueue during BUSY doesn't mutate in-flight set") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    auto data1 = makeChannelData(0, 3, 0x11);
    auto data2 = makeChannelData(1, 3, 0x22);
    engine.enqueue(data1);
    engine.show();
    FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::BUSY);

    // Enqueue a second strip while the first is transmitting. It
    // goes into the *next* frame — not the current one.
    engine.enqueue(data2);
    FL_CHECK(mock.getTransmitCount() == 1u);
    FL_CHECK(mock.getTransmitHistory()[0].size_bytes == 3u);
}

FL_TEST_CASE(
    "I2sEsp32Dev - full frame cycle: show / done / poll / show / done") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    for (int frame = 0; frame < 4; ++frame) {
        auto data = makeChannelData(0, 6, static_cast<u8>(frame));
        engine.enqueue(data);
        engine.show();
        FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::BUSY);
        mock.simulateTransmitDone();
        engine.poll();
        FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::READY);
        FL_CHECK_FALSE(data->isInUse());
    }
    FL_CHECK(mock.getTransmitCount() == 4u);
    FL_CHECK(mock.callbackInvocationCount() == 4u);
}

// =============================================================================
// FastLED#3526 Phase 2b prep — streaming buffer-refill callback surface
// =============================================================================

namespace {

struct RefillState {
    int total_calls = 0;
    int stop_after = 3;
    int last_buffer_index = -1;
};

void refillCb(int buffer_index, bool *done, void *user_ctx) FL_NO_EXCEPT {
    auto *state = static_cast<RefillState *>(user_ctx);
    state->total_calls++;
    state->last_buffer_index = buffer_index;
    if (state->total_calls >= state->stop_after) {
        *done = true;
    }
}

} // namespace

FL_TEST_CASE("I2sPeripheralEsp32DevMock - refill callback fires and reports done") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    RefillState state;
    state.stop_after = 4;
    FL_REQUIRE(mock.registerBufferRefillCallback(&refillCb, &state));

    for (int i = 0; i < 4; ++i) {
        mock.simulateBufferRefill(i % 3);
    }
    FL_CHECK(state.total_calls == 4);
    FL_CHECK(mock.bufferRefillInvocationCount() == 4u);
    FL_CHECK(mock.lastRefillBufferIndex() == 0);
    FL_CHECK(mock.lastRefillDone());
}

FL_TEST_CASE("I2sPeripheralEsp32DevMock - refill callback nullptr is no-op") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    // No registration — simulate is a no-op.
    mock.simulateBufferRefill(0);
    FL_CHECK(mock.bufferRefillInvocationCount() == 0u);
    FL_CHECK(mock.lastRefillBufferIndex() == -1);
}

FL_TEST_CASE("I2sPeripheralEsp32DevMock - refill callback clear on nullptr register") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    RefillState state;
    FL_REQUIRE(mock.registerBufferRefillCallback(&refillCb, &state));
    mock.simulateBufferRefill(0);
    FL_CHECK(state.total_calls == 1);

    FL_REQUIRE(mock.registerBufferRefillCallback(nullptr, nullptr));
    mock.simulateBufferRefill(1);
    // State unchanged.
    FL_CHECK(state.total_calls == 1);
}

FL_TEST_CASE("I2sPeripheralEsp32DevMock - refill callback respects failure knob") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    mock.setRegisterCallbackFailure(true);
    RefillState state;
    FL_REQUIRE_FALSE(mock.registerBufferRefillCallback(&refillCb, &state));
    mock.simulateBufferRefill(0);
    FL_CHECK(state.total_calls == 0);
}

// =============================================================================
// FastLED#3526 Phase 2c — mode dispatch scaffolding
// =============================================================================
//
// The engine's `show()` dispatches on the batch's clockless-vs-SPI
// composition. Mixed batches and pure-SPI batches route to a stub
// error path until the SPI transmit machinery lands (bench-blocked).
// Clockless batches take the existing wave8-encoding path.

FL_TEST_CASE("I2sEsp32Dev - pure clockless batch still succeeds") {
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    engine.enqueue(makeChannelData(0, 6, 0xAA));
    engine.enqueue(makeChannelData(1, 6, 0xBB));
    engine.show();

    // Clockless batch is fully accepted — reaches BUSY, not ERROR.
    FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::BUSY);
    FL_CHECK(mock.getTransmitCount() == 1u);
}

FL_TEST_CASE("I2sEsp32Dev - pure SPI batch — Phase 2c delegate path") {
    // FastLED#3526 Phase 2c — SPI batches now delegate to the tested
    // ChannelDriverI2sSpi. On host builds the delegate factory returns
    // nullptr (no ESP peripheral), so the engine drops to ERROR
    // cleanly with the "SPI delegate unavailable" path. On device
    // (esp32dev) the delegate is functional and the batch reaches BUSY.
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    auto spi_data = makeSpiChannelData(/*dataPin=*/23, /*clockPin=*/18, 6, 0xCC);
    engine.enqueue(spi_data);
    engine.show();

    // The clockless peripheral MUST NOT receive the SPI batch —
    // regardless of whether the delegate is available.
    FL_CHECK(mock.getTransmitCount() == 0u);
    // Either the delegate ran (BUSY) or wasn't available (ERROR).
    // Both are correct engine behaviors — the invariant is that the
    // clockless peripheral is untouched.
    auto state = engine.currentState();
    FL_CHECK(state == IChannelDriver::DriverState::BUSY ||
             state == IChannelDriver::DriverState::ERROR);
}

FL_TEST_CASE("I2sEsp32Dev - mixed clockless+SPI batch rejected") {
    // Mixed batches remain rejected — the peripheral can only run one
    // mode at a time on I2S1. The engine forces callers to homogenize
    // their batches.
    resetMockState();
    ChannelEngineI2sEsp32Dev engine(createMockPeripheral());
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    auto clockless_data = makeChannelData(0, 6, 0xAA);
    auto spi_data = makeSpiChannelData(23, 18, 6, 0xCC);
    engine.enqueue(clockless_data);
    engine.enqueue(spi_data);
    engine.show();

    FL_REQUIRE(engine.currentState() == IChannelDriver::DriverState::ERROR);
    FL_CHECK(mock.getTransmitCount() == 0u);
    FL_CHECK_FALSE(clockless_data->isInUse());
    FL_CHECK_FALSE(spi_data->isInUse());
}

FL_TEST_CASE("I2sPeripheralEsp32DevMock - refill + tx-done coexist") {
    resetMockState();
    auto &mock = I2sPeripheralEsp32DevMock::instance();

    static u32 tx_done_calls = 0;
    tx_done_calls = 0;
    auto tx_done = +[](void *) FL_NO_EXCEPT { tx_done_calls++; };
    FL_REQUIRE(mock.registerTransmitCallback(tx_done, nullptr));

    RefillState state;
    FL_REQUIRE(mock.registerBufferRefillCallback(&refillCb, &state));

    I2sEsp32DevPeripheralConfig cfg(1, 2400000, 1);
    FL_REQUIRE(mock.initialize(cfg));
    u8 dummy[4] = {0};
    FL_REQUIRE(mock.transmit(dummy, sizeof(dummy)));

    mock.simulateBufferRefill(0);
    mock.simulateBufferRefill(1);
    FL_CHECK(state.total_calls == 2);
    FL_CHECK(tx_done_calls == 0u);

    mock.simulateTransmitDone();
    FL_CHECK(tx_done_calls == 1u);
    FL_CHECK(mock.bufferRefillInvocationCount() == 2u);
}

// FastLED#3526 Phase 2b step C — lane -> GPIO pin routing.
FL_TEST_CASE("Modern I2S peripheral: routeLanePin records mock mapping") {
    auto& mock = I2sPeripheralEsp32DevMock::instance();
    mock.reset();

    // Uninitialised peripheral rejects the route.
    FL_CHECK_FALSE(mock.routeLanePin(0, 4));

    I2sEsp32DevPeripheralConfig cfg(/*port=*/1, /*clk=*/2400000, /*width=*/4);
    FL_REQUIRE(mock.initialize(cfg));

    // Route a handful of lanes to arbitrary pins.
    FL_REQUIRE(mock.routeLanePin(0, 4));
    FL_REQUIRE(mock.routeLanePin(1, 5));
    FL_REQUIRE(mock.routeLanePin(2, 18));
    FL_REQUIRE(mock.routeLanePin(3, 19));

    FL_CHECK_EQ(mock.laneRouteInvocationCount(), 4u);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(0), 4);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(1), 5);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(2), 18);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(3), 19);
    // Unrouted lanes return -1 across the full 24-lane range.
    FL_CHECK_EQ(mock.lastRoutedPinForLane(4), -1);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(15), -1);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(23), -1);

    // Lanes 16..23 are valid on classic ESP32 I2S (I2S{n}O_DATA_OUT16
    // through DATA_OUT23 exist on the hardware — see soc/gpio_sig_map.h).
    // The current wave8 encoder is 16-wide so lanes 16..23 carry no
    // useful data yet, but the routing surface accepts the full range.
    FL_REQUIRE(mock.routeLanePin(16, 22));
    FL_REQUIRE(mock.routeLanePin(23, 27));
    FL_CHECK_EQ(mock.lastRoutedPinForLane(16), 22);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(23), 27);

    // Rejected: out-of-range lane (24 or higher).
    FL_CHECK_FALSE(mock.routeLanePin(24, 4));
    FL_CHECK_FALSE(mock.routeLanePin(25, 4));

    // 6 successful routes so far (4 initial + 2 upper-range).
    FL_CHECK_EQ(mock.laneRouteInvocationCount(), 6u);

    // Re-routing the same lane replaces the previous pin.
    FL_REQUIRE(mock.routeLanePin(0, 21));
    FL_CHECK_EQ(mock.lastRoutedPinForLane(0), 21);
    FL_CHECK_EQ(mock.laneRouteInvocationCount(), 7u);

    // Reset clears everything.
    mock.reset();
    FL_CHECK_EQ(mock.laneRouteInvocationCount(), 0u);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(0), -1);
    FL_CHECK_EQ(mock.lastRoutedPinForLane(3), -1);
}

#endif // FASTLED_STUB_IMPL
