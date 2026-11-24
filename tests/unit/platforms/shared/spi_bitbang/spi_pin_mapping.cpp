/// @file spi_pin_mapping.cpp
/// @brief Calls every production parallel soft-SPI pin-mapping method with valid
/// boundaries, out-of-bank pins, duplicates and data/clock overlap, and the raw
/// mask setters of SpiIsr8/16/32. A rejected call must return false and leave the
/// previous mapping working.

#include "test.h"

#include "fl/channels/spi/parallel_device.h"
#include "fl/stl/int.h"
#include "fl/stl/string.h"
#include "platforms/shared/spi_bitbang/host_sim.h"
#include "platforms/shared/spi_bitbang/spi_block_1.h"
#include "platforms/shared/spi_bitbang/spi_block_16.h"
#include "platforms/shared/spi_bitbang/spi_block_2.h"
#include "platforms/shared/spi_bitbang/spi_block_32.h"
#include "platforms/shared/spi_bitbang/spi_block_4.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"
#include "platforms/shared/spi_bitbang/spi_isr_1.h"
#include "platforms/shared/spi_bitbang/spi_isr_2.h"
#include "platforms/shared/spi_bitbang/spi_isr_4.h"
#include "platforms/shared/spi_bitbang/spi_isr_16.h"
#include "platforms/shared/spi_bitbang/spi_isr_32.h"
#include "platforms/shared/spi_bitbang/spi_isr_8.h"
#include "platforms/shared/spi_bitbang/spi_isr_engine.h"
#include "platforms/shared/spi_bitbang/spi_pin_mapping.h"

namespace fl {

FL_TEST_FILE(FL_FILEPATH) {

namespace {

// One 0xFF byte: set every data pin, clear the clock, then raise it.
void expectMapping(u32 dataMask, u32 clockMask, u32 alsoCleared = 0) {
    FL_GPIO_Event event;
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 0);
    FL_CHECK_EQ(event.gpio_mask, dataMask);
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 1);
    FL_CHECK_EQ(event.gpio_mask, clockMask | alsoCleared);
    FL_REQUIRE(fl_gpio_sim_read_event(&event));
    FL_CHECK_EQ(event.event_type, 0);
    FL_CHECK_EQ(event.gpio_mask, clockMask);
    FL_CHECK_FALSE(fl_gpio_sim_read_event(&event));
}

const u8 kFull[] = {0xFF};

} // namespace

FL_TEST_CASE("the validator: bank edge, duplicates, overlap, empty clock mask") {
    const u8 ok[] = {0, 30};
    FL_CHECK(spiPinMappingValid(ok, 2, 31));
    const u8 high[] = {32};
    FL_CHECK_FALSE(spiPinMappingValid(high, 1, 0));
    const u8 max[] = {255};
    FL_CHECK_FALSE(spiPinMappingValid(max, 1, 0));
    FL_CHECK_FALSE(spiPinMappingValid(ok, 2, 32));
    const u8 dup[] = {3, 3};
    FL_CHECK_FALSE(spiPinMappingValid(dup, 2, 4));
    FL_CHECK_FALSE(spiPinMappingValid(ok, 2, 30));
    FL_CHECK(spiPinMappingValidWithMask(ok, 2, 1U << 31));
    FL_CHECK_FALSE(spiPinMappingValidWithMask(ok, 2, 0));
    FL_CHECK_FALSE(spiPinMappingValidWithMask(ok, 2, 1U << 30));
    // A clock mask names exactly one pin.
    FL_CHECK_FALSE(spiPinMappingValidWithMask(ok, 2, (1U << 31) | (1U << 29)));
    // Every lane, including the always-cleared lanes 8+, owns a distinct pin
    // that is not the clock: aliased lanes would make contradictory masks.
    const u8 wide[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    FL_CHECK(spiPinMappingValid(wide, 31));
    FL_CHECK_FALSE(spiPinMappingValid(wide, 10));
    FL_CHECK_FALSE(spiPinMappingValid(wide, 7));
    const u8 wideRepeat[] = {0, 1, 2, 3, 4, 5, 6, 7, 20, 20, 9};
    FL_CHECK_FALSE(spiPinMappingValid(wideRepeat, 21));
    const u8 wideOnDriven[] = {0, 1, 2, 3, 4, 5, 6, 7, 3};
    FL_CHECK_FALSE(spiPinMappingValid(wideOnDriven, 9));
    const u8 wideHigh[] = {0, 1, 2, 3, 4, 5, 6, 7, 32};
    FL_CHECK_FALSE(spiPinMappingValid(wideHigh, 9));
    // 32 distinct data pins fill the bank, leaving no pin for the clock.
    u8 full[32];
    for (u8 i = 0; i < 32; ++i) {
        full[i] = i;
    }
    for (u8 clock = 0; clock < 32; ++clock) {
        FL_CHECK_FALSE(spiPinMappingValid(full, clock));
    }
}

FL_TEST_CASE("ParallelDevice::begin rejects pins its table cannot shift") {
    auto beginWith = [](u8 clock, fl::vector<u8> pins) {
        spi::ParallelDevice::Config config;
        config.clock_pin = clock;
        config.gpio_pins = fl::move(pins);
        spi::ParallelDevice device(config);
        auto err = device.begin();
        return err ? err->message : fl::string();
    };
    const fl::string bad = "Invalid pin mapping";
    FL_CHECK(beginWith(18, {33}).find(bad) == 0);
    FL_CHECK(beginWith(18, {0, 255}).find(bad) == 0);
    FL_CHECK(beginWith(18, {0, 18}).find(bad) == 0);
    FL_CHECK(beginWith(0xFF, {0, 1}).find(bad) == 0); // Default (unset) clock.
    // A valid mapping gets past validation to the not-yet-implemented backend.
    FL_CHECK(beginWith(18, {0, 1, 2, 3, 4, 5, 6, 7, 9, 9}).find(bad) == 0);
    FL_CHECK(beginWith(18, {0, 1}).find(bad) == fl::string::npos);
}

template <typename Block>
void expectUnmappedSilent() {
    Block spi;
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    FL_GPIO_Event event;
    FL_CHECK_FALSE(fl_gpio_sim_read_event(&event));
}

FL_TEST_CASE("an unmapped blocking driver writes nothing") {
    expectUnmappedSilent<SpiBlock1>();
    expectUnmappedSilent<SpiBlock2>();
    expectUnmappedSilent<SpiBlock4>();
    expectUnmappedSilent<SpiBlock8>();
    expectUnmappedSilent<SpiBlock16>();
    expectUnmappedSilent<SpiBlock32>();
    SpiBlock4 spi;
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 32, 4));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    FL_GPIO_Event event;
    FL_CHECK_FALSE(fl_gpio_sim_read_event(&event));
}

FL_TEST_CASE("SpiBlock1: rejected mappings keep the last good one") {
    SpiBlock1 spi;
    FL_CHECK(spi.setPinMapping(31, 30)); // Top of the bank.
    FL_REQUIRE(spi.setPinMapping(0, 1));
    FL_CHECK_FALSE(spi.setPinMapping(32, 1));
    FL_CHECK_FALSE(spi.setPinMapping(255, 1));
    FL_CHECK_FALSE(spi.setPinMapping(0, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    expectMapping(1U, 2U);
}

FL_TEST_CASE("SpiBlock2: rejected mappings keep the last good one") {
    SpiBlock2 spi;
    FL_CHECK(spi.setPinMapping(30, 31, 29)); // Top of the bank.
    FL_REQUIRE(spi.setPinMapping(0, 1, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 32, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 255, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 2));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    expectMapping(3U, 4U);
}

FL_TEST_CASE("SpiBlock4: rejected mappings keep the last good one") {
    SpiBlock4 spi;
    FL_CHECK(spi.setPinMapping(28, 29, 30, 31, 27)); // Top of the bank.
    FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 32, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 255, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 1, 2, 4));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    expectMapping(15U, 16U);
}

FL_TEST_CASE("SpiBlock8: rejected mappings keep the last good one") {
    SpiBlock8 spi;
    FL_CHECK(spi.setPinMapping(24, 25, 26, 27, 28, 29, 30, 31, 23)); // Top of the bank.
    FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 32, 8));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 255, 8));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 1, 2, 3, 4, 5, 6, 8));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    expectMapping(255U, 256U);
}

FL_TEST_CASE("SpiBlock16: rejected mappings keep the last good one") {
    SpiBlock16 spi;
    FL_CHECK(spi.setPinMapping(16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 15)); // Top of the bank.
    FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 32, 16));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 255, 16));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 3, 16)); // Lane 15 on driven lane 3.
    // The always-cleared lanes 8+ may not repeat or sit on the clock either.
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 20, 20, 20, 20, 20, 20, 20, 20, 16));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 16));
    FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    // A byte has 8 bits: the 256-entry table drives pins 0-7 and clears the rest.
    expectMapping(255U, 65536U, 0xFF00U);
}

FL_TEST_CASE("SpiBlock32: 32 data pins plus a clock cannot fit one bank") {
    SpiBlock32 spi;
    // Every pin of the 32-bit bank is a data lane, so any clock aliases a lane.
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 31));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 8));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 30, 31));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 255, 31));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 0, 31));
    // Rejected, so it stays unmapped and drives nothing.
    fl_gpio_sim_init();
    spi.loadBuffer(kFull, 1);
    spi.transmit();
    FL_GPIO_Event event;
    FL_CHECK_FALSE(fl_gpio_sim_read_event(&event));
}

FL_TEST_CASE("SpiIsr1: both mapping forms reject without changing state") {
    SpiIsr1 spi;
    FL_REQUIRE(spi.setPinMapping(0, 1));
    FL_CHECK_FALSE(spi.setPinMapping(32, 1));
    FL_CHECK_FALSE(spi.setPinMapping(255, 1));
    FL_CHECK_FALSE(spi.setPinMapping(0, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(32, 2U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 0U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 6U)); // Two clock pins.
    FL_CHECK_FALSE(spi.setPinMappingWithMask(255, 2U));
    FL_CHECK_TRUE(spi.setPinMappingWithMask(0, 2U)); // Same as before.
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 2U);
    FL_CHECK_EQ(spi.getLUTArray()[0xFF].set_mask, 1U);
    FL_CHECK(spi.setPinMappingWithMask(0, 2147483648U));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 2147483648U);
}

FL_TEST_CASE("SpiIsr2: both mapping forms reject without changing state") {
    SpiIsr2 spi;
    FL_REQUIRE(spi.setPinMapping(0, 1, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 32, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 255, 2));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 2));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 32, 4U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 0U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 1U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 0xC0000000U)); // Two clock pins.
    FL_CHECK_FALSE(spi.setPinMappingWithMask(1, 1, 4U));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 4U);
    FL_CHECK_EQ(spi.getLUTArray()[0xFF].set_mask, 3U);
    FL_CHECK(spi.setPinMappingWithMask(0, 1, 2147483648U));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 2147483648U);
}

FL_TEST_CASE("SpiIsr4: both mapping forms reject without changing state") {
    SpiIsr4 spi;
    FL_REQUIRE(spi.setPinMapping(0, 1, 2, 3, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 32, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 255, 4));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 32));
    FL_CHECK_FALSE(spi.setPinMapping(0, 1, 2, 3, 0));
    FL_CHECK_FALSE(spi.setPinMapping(0, 0, 1, 2, 4));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 2, 32, 16U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 2, 3, 0U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 2, 3, 1U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 2, 3, 0xFFFFFFF0U)); // Many clock pins.
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 3, 3, 16U));
    FL_CHECK_FALSE(spi.setPinMappingWithMask(0, 1, 2, 255, 16U));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 16U);
    FL_CHECK_EQ(spi.getLUTArray()[0xFF].set_mask, 15U);
    FL_CHECK(spi.setPinMappingWithMask(0, 1, 2, 3, 2147483648U));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 2147483648U);
}

// Runs one ISR burst of `n` bytes from `data` on the host GPIO simulation.
template <typename Isr>
void runIsrBurst(Isr& spi, const u8* data, u16 n) {
    Isr::resetState();
    spi.loadBuffer(data, n);
    fl_gpio_sim_init();
    spi.arm();
    for (int i = 0; i < 2 * n + 2 && !(spi.statusFlags() & Isr::STATUS_DONE); ++i) {
        fl_parallel_spi_isr();
    }
    FL_CHECK(spi.statusFlags() & Isr::STATUS_DONE);
    spi.ackDone();
}

template <typename Isr>
void checkRawMaskContract() {
    Isr spi;
    // Start from an empty table so the shared engine state of earlier tests
    // cannot collide with this test's clock.
    u32 zeros[256] = {};
    FL_REQUIRE(spi.loadLUT(zeros, zeros));

    // The clock mask names exactly one pin.
    FL_REQUIRE(spi.setClockMask(1U << 8));
    FL_CHECK_FALSE(spi.setClockMask(0));
    FL_CHECK_FALSE(spi.setClockMask(3U));
    FL_CHECK_FALSE(spi.setClockMask(0xFFFFFFFFU));
    FL_CHECK_EQ(fl_spi_state()->clock_pin_mask, 1U << 8);

    // Byte v drives pins 0-7 to its bits.
    u32 setMasks[256];
    u32 clearMasks[256];
    for (u32 v = 0; v < 256; ++v) {
        setMasks[v] = v;
        clearMasks[v] = ~v & 0xFFU;
    }
    FL_REQUIRE(spi.loadLUT(setMasks, clearMasks));

    // A table entry may not touch the clock, and may not set and clear one pin.
    FL_CHECK_FALSE(spi.setLUTEntry(0x10, 0x10U | (1U << 8), 0xEFU));
    FL_CHECK_FALSE(spi.setLUTEntry(0x10, 0x10U, 0xEFU | (1U << 8)));
    FL_CHECK_FALSE(spi.setLUTEntry(0x10, 0x10U, 0xFFU));
    FL_CHECK(spi.setLUTEntry(0x10, 0x10U, 0xEFU)); // Same as loaded.

    // loadLUT is all-or-nothing: one bad entry rejects the whole table.
    u32 badSet[256];
    for (u32 v = 0; v < 256; ++v) {
        badSet[v] = v << 1;
    }
    badSet[0x80] = 1U << 8;
    FL_CHECK_FALSE(spi.loadLUT(badSet, clearMasks));
    FL_CHECK_FALSE(spi.loadLUT(nullptr, clearMasks));
    FL_CHECK_FALSE(spi.loadLUT(setMasks, nullptr));
    FL_CHECK_EQ(spi.getLUTArray()[0x01].set_mask, 1U);
    FL_CHECK_EQ(spi.getLUTArray()[0x80].set_mask, 0x80U);

    // The clock may not move onto a pin the loaded table drives.
    FL_CHECK_FALSE(spi.setClockMask(1U << 3));
    FL_CHECK(spi.setClockMask(1U << 31));
    FL_CHECK(spi.setClockMask(1U << 8));

    // The previous configuration still transmits exactly.
    runIsrBurst(spi, kFull, 1);
    expectMapping(0xFFU, 1U << 8);
}

FL_TEST_CASE("SpiIsr8: raw clock and table masks are checked") {
    checkRawMaskContract<SpiIsr8>();
}

FL_TEST_CASE("SpiIsr16: raw clock and table masks are checked") {
    checkRawMaskContract<SpiIsr16>();
}

FL_TEST_CASE("SpiIsr32: raw clock and table masks are checked") {
    checkRawMaskContract<SpiIsr32>();
}

} // FL_TEST_FILE

} // namespace fl
