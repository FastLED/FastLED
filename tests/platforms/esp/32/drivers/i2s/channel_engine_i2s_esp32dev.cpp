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

#endif // FASTLED_STUB_IMPL
