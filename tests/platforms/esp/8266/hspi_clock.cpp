/// @file tests/platforms/esp/8266/hspi_clock.cpp
/// Host tests for the ESP8266 HSPI clock-divider search (#4925).
///
/// The search is pure arithmetic: it picks the SPI1CLK value for a requested
/// bit rate. Pin it here so a refactor cannot retune every HSPI strip's clock
/// without a build failure to catch it, and so the host can check the divider
/// never lands above the requested rate.

#include "test.h"

#include "platforms/esp/8266/hspi_clock.h"

using fl::esp8266_hspi::clockRegister;
using fl::esp8266_hspi::clockRegisterToHz;
using fl::esp8266_hspi::kApbHz;

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("esp8266 hspi clock - never exceeds the requested rate") {
    // The APA102 default is DATA_RATE_MHZ(6) = 80 / 6 = 13 cycles per bit, so
    // SPI_SPEED 13 asks the driver for 80 MHz / 13 = 6.15 MHz.
    const fl::u32 requests[] = {250000, 1000000, 5000000, 6153846, 10000000,
                                20000000, 40000000, 79000000};
    for (fl::u32 hz : requests) {
        const fl::u32 reachable = clockRegisterToHz(clockRegister(hz));
        FL_CHECK(reachable <= hz);
    }
}

FL_TEST_CASE("esp8266 hspi clock - exact rates come out exact") {
    // 80 MHz divided by a whole number, all reachable with pre = 1.
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(8000000)), 8000000u);   // n = 9
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(10000000)), 10000000u); // n = 7
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(20000000)), 20000000u); // n = 3
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(40000000)), 40000000u); // n = 1
    // DATA_RATE_MHZ(6) is 80 / 6 = 13 cycles per bit, i.e. 6.15 MHz, which the
    // driver asks for with ceiling division: 80000000 / 13 rounds up to
    // 6153847, so the search can land on period 13 instead of rounding down to
    // period 14 (5.71 MHz). The floored 6153846 is the one request that cannot
    // answer exactly, because the next divider step above it is 5.71 MHz.
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(6153847)), 6153846u);  // n = 12
    FL_CHECK(clockRegisterToHz(clockRegister(6153846)) < 6153846u);     // never exceeds
}

FL_TEST_CASE("esp8266 hspi clock - the APB clock and the divider ceiling") {
    // 80 MHz is the undivided clock and needs its own register bit.
    FL_CHECK_EQ(clockRegister(kApbHz), 0x80000000u);  // SPI_CLK_EQU_SYSCLK
    FL_CHECK_EQ(clockRegisterToHz(0x80000000u), kApbHz);

    // Under the undivided clock the period is pre * (n + 1) >= 2, so the
    // fastest divided clock is 40 MHz: a request above it, however close to
    // 80 MHz, must not fall back to a slower divider instead.
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(79000000)), 40000000u);
    FL_CHECK_EQ(clockRegisterToHz(clockRegister(67000000)), 40000000u);
}

FL_TEST_CASE("esp8266 hspi clock - the divider search does not overflow") {
    // hz * (n + 1) overflows u32 for any request above 67 MHz as soon as
    // n + 1 > 64 * 80 / hz; a wrapped product picks a wrong prescaler, so
    // the near-APB requests above are the regression guard for the u64 math.
    // Zero is a legal compile-time input (SPI_SPEED 0), not a divide by zero.
    // The slowest divider is pre = 8192, n + 1 = 64: 80 MHz / 524288 = 152 Hz,
    // so requests below that all land on the same slowest setting.
    FL_CHECK(clockRegister(0) != 0);
    FL_CHECK(clockRegisterToHz(clockRegister(0)) <= 153u);
    FL_CHECK_EQ(clockRegister(0), clockRegister(1));
}

FL_TEST_CASE("esp8266 hspi clock - register fields stay in range") {
    const fl::u32 requests[] = {1, 1000, 250000, 6153846, 40000000, 79000000};
    for (fl::u32 hz : requests) {
        const fl::u32 reg = clockRegister(hz);
        const fl::u32 pre = (reg >> 18) & 0x1FFFu;  // SPI_CLKDIV_PRE
        const fl::u32 n = (reg >> 12) & 0x3Fu;      // SPI_CLKCNT_N
        const fl::u32 h = (reg >> 6) & 0x3Fu;       // SPI_CLKCNT_H
        const fl::u32 l = reg & 0x3Fu;              // SPI_CLKCNT_L
        FL_CHECK(pre <= 0x1FFF);
        FL_CHECK(n >= 1 && n <= 63);
        FL_CHECK(l == n);      // low half at the end of the period
        FL_CHECK(h <= n);      // high half inside it
    }
}

}  // FL_TEST_FILE
