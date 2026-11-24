/// @file spi_hspi_clock.cpp
/// @brief Host-side tests for the ESP8266 HSPI clock divider computation
///
/// The divider arithmetic is pure integer math, so it is testable without the
/// ESP8266 SDK. The resulting SPI1CLK fields are decoded back with the
/// silicon's own formula (80 MHz / ((CLKDIV_PRE + 1) * (CLKCNT_N + 1))) and
/// checked against the frequency the helper reports.

#include "platforms/esp/8266/spi_hspi_clock.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::esp8266;

namespace {

/// Recompute the frequency from the SPI1CLK bit fields, as the hardware does.
fl::u32 decodeHz(fl::u32 reg) {
    const fl::u32 pre = (reg >> 18) & 0x1FFF;
    const fl::u32 n = (reg >> 12) & 0x3F;
    return kHspiSourceHz / ((pre + 1) * (n + 1));
}

}  // namespace

FL_TEST_CASE("ESP8266 HSPI clock - defaults reported by APA102/SK9822") {
    // APA102 default (6 MHz) must be at or below the request and close to it.
    const HspiClock clk6 = hspiClockFor(6000000);
    FL_CHECK(clk6.actual_hz <= 6000000);
    FL_CHECK(clk6.actual_hz > 5000000);
    FL_CHECK_EQ(clk6.actual_hz, decodeHz(clk6.reg_value));

    const HspiClock clk12 = hspiClockFor(12000000);
    FL_CHECK(clk12.actual_hz <= 12000000);
    FL_CHECK(clk12.actual_hz > 10000000);
    FL_CHECK_EQ(clk12.actual_hz, decodeHz(clk12.reg_value));
}

FL_TEST_CASE("ESP8266 HSPI clock - never exceeds the requested rate") {
    const fl::u32 rates[] = {100000, 500000, 1000000, 3200000, 5000000,
                             6000000, 8000000, 12000000, 24000000, 40000000};
    for (fl::u32 rate : rates) {
        const HspiClock clk = hspiClockFor(rate);
        FL_CHECK(clk.actual_hz <= rate);
        // The best divider is within a factor of two of the request.
        FL_CHECK(clk.actual_hz * 2 > rate);
        FL_CHECK_EQ(clk.actual_hz, decodeHz(clk.reg_value));
        FL_CHECK((clk.reg_value & kHspiEquSysClk) == 0);
    }
}

FL_TEST_CASE("ESP8266 HSPI clock - software divider converts without overclocking") {
    // FL_DATA_RATE_MHZ(12) on the software path is (F_CPU / 1 MHz) / 12
    // truncated to whole CPU cycles per bit, so the real request lies above
    // F_CPU / (cycles + 1). The conversion into the 80 MHz HSPI domain must
    // land at or below the request for both 80 MHz and 160 MHz CPUs.
    const fl::u32 sk9822_80 = hspiDividerForCpuCycles(80000000UL / 12000000UL + 1,
                                                     80000000UL);  // 7
    FL_CHECK_EQ(sk9822_80, 7);
    const HspiClock clk = hspiClockForMinDivider(sk9822_80);
    FL_CHECK(clk.actual_hz <= 12000000UL);
    FL_CHECK(clk.actual_hz > 11000000UL);
    FL_CHECK_EQ(clk.actual_hz, decodeHz(clk.reg_value));
    FL_CHECK_EQ(clk.actual_hz, 80000000UL / 7);

    // At 160 MHz the same request is 13 CPU cycles per bit; the hardware
    // divider is 7, not 14 (which would halve the bus rate).
    const fl::u32 sk9822_160 = hspiDividerForCpuCycles(160000000UL / 12000000UL + 1,
                                                       160000000UL);
    FL_CHECK_EQ(sk9822_160, 7);
    FL_CHECK_EQ(hspiClockForMinDivider(sk9822_160).actual_hz, 80000000UL / 7);

    // Same for the 6 MHz APA102 default, at both CPU speeds: the divider is 14
    // (5.71 MHz), at or below the request.
    const fl::u32 apa102_80 = hspiDividerForCpuCycles(80000000UL / 6000000UL + 1,
                                                      80000000UL);
    FL_CHECK_EQ(apa102_80, 14);
    const HspiClock clk6 = hspiClockForMinDivider(apa102_80);
    FL_CHECK(clk6.actual_hz <= 6000000UL);
    FL_CHECK(clk6.actual_hz > 5000000UL);
    FL_CHECK_EQ(clk6.actual_hz, 80000000UL / 14);

    const fl::u32 apa102_160 = hspiDividerForCpuCycles(160000000UL / 6000000UL + 1,
                                                       160000000UL);
    FL_CHECK_EQ(apa102_160, 14);
    FL_CHECK_EQ(hspiClockForMinDivider(apa102_160).actual_hz, 80000000UL / 14);

    // A CPU running slower than the HSPI source still yields a divider >= 1.
    FL_CHECK(hspiDividerForCpuCycles(1, 16000000UL) >= 1);

    // Degenerate inputs cannot divide by zero and never overclock.
    FL_CHECK(hspiDividerForCpuCycles(0, 80000000UL) >= 1);
    FL_CHECK(hspiClockForMinDivider(0).actual_hz > 0);
}

FL_TEST_CASE("ESP8266 HSPI clock - register fields stay inside their widths") {
    const fl::u32 rates[] = {1, 100, 1000, 100000, 6000000, 79999999};
    for (fl::u32 rate : rates) {
        const HspiClock clk = hspiClockFor(rate);
        const fl::u32 pre = (clk.reg_value >> 18) & 0x1FFF;
        const fl::u32 n = (clk.reg_value >> 12) & 0x3F;
        FL_CHECK(pre < kHspiMaxPreDivider);
        FL_CHECK(n < kHspiMaxNDivider);
        FL_CHECK(clk.actual_hz > 0);
        FL_CHECK_EQ(clk.actual_hz, decodeHz(clk.reg_value));
    }
}

FL_TEST_CASE("ESP8266 HSPI clock - 50 percent duty encoding") {
    // CLKCNT_H stays 0 and CLKCNT_L is (N + 1) / 2, matching the Arduino core.
    const fl::u32 rates[] = {6000000, 12000000, 24000000};
    for (fl::u32 rate : rates) {
        const HspiClock clk = hspiClockFor(rate);
        const fl::u32 n = (clk.reg_value >> 12) & 0x3F;
        const fl::u32 duty_l = clk.reg_value & 0x3F;
        FL_CHECK_EQ(duty_l, (n + 1) / 2);
    }
}

FL_TEST_CASE("ESP8266 HSPI clock - CLKCNT_L is never zero") {
    // CLKCNT_L is the clock's high phase, so a zero leaves SCK parked low for
    // the whole transfer. Measured on an ESP8266: 25 kHz resolved to
    // CLKDIV_PRE = 3201 with CLKCNT_N = CLKCNT_L = 0, and a 64-bit HSPI
    // transfer drove the SCK pad for 0 of 4691 samples -- the frame took the
    // programmed 2.6 ms while nothing reached the wire. Re-splitting the same
    // divider with CLKCNT_N = 1 (CLKCNT_L = 1) produced 128 edges. ESP-IDF's
    // own divider search starts at n = 2 for this reason.
    const fl::u32 rates[] = {1,      100,    1000,     25000,
                             50000,  100000, 1000000,  3200000,
                             6000000, 12000000, 40000000};
    for (fl::u32 rate : rates) {
        const HspiClock clk = hspiClockFor(rate);
        FL_CHECK((clk.reg_value & 0x3F) >= 1);
        FL_CHECK_EQ(clk.actual_hz, decodeHz(clk.reg_value));
    }

    // The minimum-divider entry point is what the driver actually calls, so it
    // gets the same guarantee across the whole usable divider range.
    for (fl::u32 min_div = 1; min_div <= 4096; min_div = min_div * 2) {
        const HspiClock clk = hspiClockForMinDivider(min_div);
        // Divider 1 has no CLKCNT_L encoding; it is the system-clock bypass.
        if (min_div == 1) {
            FL_CHECK_EQ(clk.reg_value, kHspiEquSysClk);
        } else {
            FL_CHECK((clk.reg_value & 0x3F) >= 1);
        }
        FL_CHECK_EQ(clk.actual_hz, decodeHz(clk.reg_value));
        FL_CHECK(clk.actual_hz <= kHspiSourceHz / min_div);
    }

    // The invariant belongs to the arithmetic, not to one caller: a candidate
    // built directly with the forbidden n = 1 is clamped, so its register value
    // and its reported frequency still agree.
    const HspiClock clamped = hspiCandidate(1, 1);
    FL_CHECK((clamped.reg_value & 0x3F) >= 1);
    FL_CHECK_EQ(clamped.actual_hz, decodeHz(clamped.reg_value));
    FL_CHECK_EQ(clamped.actual_hz, kHspiSourceHz / kHspiMinNDivider);
}

FL_TEST_CASE("ESP8266 HSPI clock - divider 1 is the system-clock bypass") {
    // The divider fields cannot express divider 1 (CLKCNT_L would be zero), so
    // the fastest setting is the SPICLK_EQU_SYSCLK bypass -- the same value the
    // Arduino core's setFrequency() uses for any request at or above 80 MHz.
    // The driver turns that into the GPMUX mux bit, exactly as the core does.
    const HspiClock min1 = hspiClockForMinDivider(1);
    FL_CHECK_EQ(min1.reg_value, kHspiEquSysClk);
    FL_CHECK_EQ(min1.actual_hz, kHspiSourceHz);

    // Both entry points agree, and 0 keeps its "slowest possible request"
    // meaning by selecting the bypass rather than dividing by zero.
    FL_CHECK_EQ(hspiClockForMinDivider(0).reg_value, kHspiEquSysClk);
    FL_CHECK_EQ(hspiClockFor(kHspiSourceHz).reg_value, min1.reg_value);
    FL_CHECK_EQ(hspiClockFor(120000000).reg_value, min1.reg_value);

    // One step slower is a real divider with a nonzero high phase.
    const HspiClock min2 = hspiClockForMinDivider(2);
    FL_CHECK((min2.reg_value & kHspiEquSysClk) == 0);
    FL_CHECK((min2.reg_value & 0x3F) >= 1);
    FL_CHECK_EQ(min2.actual_hz, kHspiSourceHz / 2);
}

FL_TEST_CASE("ESP8266 HSPI clock - degenerate inputs") {
    // Zero is clamped instead of dividing by zero.
    const HspiClock zero = hspiClockFor(0);
    FL_CHECK(zero.actual_hz > 0);
    FL_CHECK_EQ(zero.actual_hz, decodeHz(zero.reg_value));

    // At or above the source clock the bus runs straight from the system clock.
    const HspiClock fast = hspiClockFor(80000000);
    FL_CHECK_EQ(fast.reg_value, kHspiEquSysClk);
    const HspiClock faster = hspiClockFor(120000000);
    FL_CHECK_EQ(faster.reg_value, kHspiEquSysClk);
}

}  // FL_TEST_FILE
