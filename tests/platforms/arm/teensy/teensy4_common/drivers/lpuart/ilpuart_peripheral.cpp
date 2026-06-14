/// @file ilpuart_peripheral.cpp
/// @brief Host tests for the LPUART peripheral abstraction (issue #3023.A).
///
/// Exercises the abstract `ILPUARTPeripheral` interface against the host
/// stub mock (`LPUARTPeripheralMock`). The engine TU and the real iMXRT
/// register driver land in follow-up PRs; these tests are scoped to the
/// peripheral seam.

#ifdef FASTLED_STUB_IMPL

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "platforms/shared/mock/arm/teensy4/drivers/lpuart/lpuart_peripheral_mock.h"
#include "fl/stl/shared_ptr.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("LPUARTPeripheralMock accepts every kLPUARTTxPins[] entry by default") {
    LPUARTPeripheralMock mock;
    for (u32 i = 0; i < kLPUARTTxPinCount; ++i) {
        const u8 pin = kLPUARTTxPins[i];
        const LPUARTPinResult result = mock.validatePin(pin);
        FL_INFO("pin=" << static_cast<int>(pin));
        FL_REQUIRE(result.valid);
        FL_REQUIRE(result.error_message == nullptr);
    }
}

FL_TEST_CASE("LPUARTPeripheralMock rejects pins outside the kLPUARTTxPins[] set") {
    LPUARTPeripheralMock mock;
    // Pin 0 and 13 are intentionally not in the table (pin 13 is LED_BUILTIN /
    // SPI_SCK on Teensy 4 — would collide). The exact error text is part of
    // the contract: the engine surfaces it via ChannelManager::reportError.
    const u8 not_in_table[] = {0, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13};
    for (auto pin : not_in_table) {
        const LPUARTPinResult result = mock.validatePin(pin);
        FL_INFO("pin=" << static_cast<int>(pin));
        FL_REQUIRE_FALSE(result.valid);
        FL_REQUIRE(result.error_message != nullptr);
    }
}

FL_TEST_CASE("LPUARTPeripheralMock setInvalidPin overrides default acceptance") {
    LPUARTPeripheralMock mock;
    // Pin 1 is LPUART6_TX — accepted by default.
    FL_REQUIRE(mock.validatePin(1).valid);
    mock.setInvalidPin(1, "test override");
    const LPUARTPinResult result = mock.validatePin(1);
    FL_REQUIRE_FALSE(result.valid);
    FL_REQUIRE(result.error_message != nullptr);
    // The override should be the message we set.
    FL_REQUIRE(fl::string::from_literal(result.error_message)
               == fl::string::from_literal("test override"));
    mock.clearInvalidPins();
    FL_REQUIRE(mock.validatePin(1).valid);
}

FL_TEST_CASE("LPUARTPeripheralMock createInstance captures the full call record") {
    LPUARTPeripheralMock mock;
    auto instance = mock.createInstance(
        /* tx_pin   */ 1,
        /* total    */ 64,
        /* is_rgbw  */ false,
        /* t1_ns    */ 250,
        /* t2_ns    */ 625,
        /* t3_ns    */ 375,
        /* reset_us */ 280);
    FL_REQUIRE(instance != nullptr);
    FL_REQUIRE_EQ(mock.getCreateCount(), 1u);

    const auto* record = mock.getLastCreateRecord();
    FL_REQUIRE(record != nullptr);
    FL_REQUIRE_EQ(record->tx_pin, 1u);
    FL_REQUIRE_EQ(record->total_leds, 64u);
    FL_REQUIRE_FALSE(record->is_rgbw);
    FL_REQUIRE_EQ(record->t1_ns, 250u);
    FL_REQUIRE_EQ(record->t2_ns, 625u);
    FL_REQUIRE_EQ(record->t3_ns, 375u);
    FL_REQUIRE_EQ(record->reset_us, 280u);

    // Buffer sized for 64 LEDs × 3 bytes × 4 UART-bytes-per-LED-byte.
    FL_REQUIRE_EQ(instance->getTxBufferSize(), 64u * 3u * 4u);
    FL_REQUIRE(instance->getTxBuffer() != nullptr);

    // show() should be sync (returns immediately) and bump the counter.
    FL_REQUIRE_EQ(static_cast<LPUARTInstanceMock*>(instance.get())->getShowCount(), 0u);
    instance->show();
    FL_REQUIRE_EQ(static_cast<LPUARTInstanceMock*>(instance.get())->getShowCount(), 1u);
}

FL_TEST_CASE("LPUARTPeripheralMock createInstance honors RGBW buffer sizing") {
    LPUARTPeripheralMock mock;
    auto instance = mock.createInstance(
        /* tx_pin   */ 8,
        /* total    */ 32,
        /* is_rgbw  */ true,
        /* t1_ns    */ 300, /* t2_ns */ 600, /* t3_ns */ 300, /* reset_us */ 80);
    FL_REQUIRE(instance != nullptr);
    // 32 LEDs × 4 bytes (RGBW) × 4 UART-bytes-per-LED-byte.
    FL_REQUIRE_EQ(instance->getTxBufferSize(), 32u * 4u * 4u);
}

FL_TEST_CASE("LPUARTPeripheralMock setCreateFailure forces nullptr from createInstance") {
    LPUARTPeripheralMock mock;
    mock.setCreateFailure(true);
    auto instance = mock.createInstance(1, 64, false, 250, 625, 375, 280);
    FL_REQUIRE(instance == nullptr);
    // The record is still captured even on failure — useful when tests want
    // to verify the engine attempted the create() with the expected args.
    FL_REQUIRE_EQ(mock.getCreateCount(), 1u);
    mock.setCreateFailure(false);
    auto instance2 = mock.createInstance(1, 64, false, 250, 625, 375, 280);
    FL_REQUIRE(instance2 != nullptr);
}

FL_TEST_CASE("ILPUARTPeripheral::create returns a mock on the host build") {
    auto periph = ILPUARTPeripheral::create();
    FL_REQUIRE(periph != nullptr);
    // Should accept a known-good TX pin.
    FL_REQUIRE(periph->validatePin(1).valid);
    FL_REQUIRE_FALSE(periph->validatePin(0).valid);
}

}  // FL_TEST_FILE

#endif  // FASTLED_STUB_IMPL
