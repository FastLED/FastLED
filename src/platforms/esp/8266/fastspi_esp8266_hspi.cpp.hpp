// IWYU pragma: private

/// @file fastspi_esp8266_hspi.cpp.hpp
/// Out-of-line half of the register-level HSPI driver (#4925).
///
/// fastspi_esp8266_hspi.h keeps the template and the staging state; the plain
/// functions that touch the registers live here so the header stays a header.
/// Included once, from platforms/esp/8266/_build.cpp.hpp.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP8266)

#include "platforms/cpu_frequency.h"
#include "platforms/esp/8266/fastspi_esp8266_hspi.h"

namespace fl {
namespace esp8266_hspi {

State& state() FL_NO_EXCEPT {
    static State s = {{{0}}, s.stage.bytes, s.stage.bytes + 4, 0};
    return s;
}

void waitIdle() FL_NO_EXCEPT {
    while (SPI1CMD & SPIBUSY) {
    }
}

void commit(State& s, u32 nbytes) FL_NO_EXCEPT {
    const u32 base = s.high ? kHalfWords : 0;
    const u32 nwords = (nbytes + 3) / 4;
    for (u32 i = 0; i < nwords; ++i) {
        SPI1W((base + i)) = s.stage.words[i];  // vendor macro does not parenthesize p
    }
    waitIdle();
    SPI1U = SPIUMOSI | (s.high ? SPIUMOSIH : 0u);
    SPI1U1 = ((nbytes * 8) - 1) << SPILMOSI;
    SPI1CMD = SPIBUSY;
    s.high ^= 1;
    // Chunks double up to a full half: nothing waits for 32 bytes before the
    // wire starts, and each chunk fills while the one before it shifts out.
    u32 next = nbytes * 2;
    if (next > kHalfBytes) {
        next = kHalfBytes;
    }
    s.ptr = s.stage.bytes;
    s.end = s.stage.bytes + next;
}

void begin() FL_NO_EXCEPT {
    State& s = state();
    s.ptr = s.stage.bytes;
    s.end = s.stage.bytes + 4;
}

void flush() FL_NO_EXCEPT {
    State& s = state();
    const u32 nbytes = u32(s.ptr - s.stage.bytes);
    if (nbytes != 0) {
        commit(s, nbytes);
    }
}

void sendBit(u8 bit) FL_NO_EXCEPT {
    flush();
    waitIdle();  // nothing may be on the wire: this command reuses W0
    SPI1U = SPIUMOSI;
    SPI1U1 = 0;  // one bit
    SPI1W0 = bit ? 0x80000000u : 0u;
    SPI1CMD = SPIBUSY;
    waitIdle();
    begin();
}

u32 clockForSpeed(u32 cycles) FL_NO_EXCEPT {
    // Zero-initialized statics: constant initialization, so select() does not
    // pay a thread-safe-init guard on every frame, and the divider search (63
    // candidate periods) runs only when the strip's rate changes.
    static u32 sCycles = 0;
    static u32 sReg = 0;
    if (sReg == 0 || sCycles != cycles) {
        // Ceiling division: SPI_SPEED is itself a floored rate (DATA_RATE_MHZ(6)
        // is 80 / 6 = 13 cycles, i.e. 6.15 MHz), and flooring again would ask
        // for a hair under the rate the divider can reach, costing a whole
        // divider step (6.15 -> 5.71 MHz).
        sCycles = cycles;
        sReg = clockRegister(cycles == 0 ? kApbHz
                                         : u32((u64(FL_CPU_FREQUENCY()) + cycles - 1) / cycles));
    }
    return sReg;
}

}  // namespace esp8266_hspi
}  // namespace fl

#endif  // FL_IS_ESP8266
