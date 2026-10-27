#pragma once

// IWYU pragma: private

/// @file fastspi_esp8266_hspi.h
/// Register-level HSPI (SPI1) output for ESP8266, no Arduino <SPI.h>.
///
/// The ESP8266 has no GPIO matrix: HSPI's MOSI and SCK are fixed to GPIO13
/// and GPIO14, so spi_output_template.h selects this class only for that pin
/// pair and bit-bangs every other one (#4925).
///
/// HSPI sends up to 64 bytes per command from its W0..W15 data buffer. With
/// SPI_USR_DOUT_HIGHPART set, a command reads only W8..W15, so the buffer is
/// split into two 32-byte halves and used ping-pong: the CPU fills the idle
/// half while the other half is on the wire, so pixel scaling and dithering
/// overlap transmission instead of adding to it.
///
/// SPI_SPEED keeps the ESP8266's software-SPI meaning (CPU cycles per bit,
/// from DATA_RATE_MHZ), so the same controller template works on any pins.
/// HSPI runs at the fastest clock not above F_CPU / SPI_SPEED, the rate the
/// divider describes (DATA_RATE_MHZ(6) at 80 MHz: 80 / 13 = 6.15 MHz).
///
/// This driver owns HSPI's mode registers. A sketch that also drives HSPI
/// through Arduino <SPI.h> (an SD card, say) should define
/// FASTLED_ALL_PINS_HARDWARE_SPI so FastLED uses SPI.h too.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/compiler_control.h"
#include "platforms/cpu_frequency.h"
#include "platforms/esp/8266/hspi_clock.h"
#include "fastspi_types.h"
#include "fl/system/fastpin_base.h"

FL_EXTERN_C_BEGIN
extern "C" {
    // IWYU pragma: begin_keep
    #include <esp8266_peri.h>
    // IWYU pragma: end_keep
}
FL_EXTERN_C_END

namespace fl {

namespace esp8266_hspi {

constexpr u8 kHalfWords = 8;  // 32 bytes per ping-pong half

constexpr u8 kHalfBytes = kHalfWords * 4;

/// Ping-pong state shared by every HSPI output: there is one HSPI block.
/// Bytes are staged in RAM, one store each, and copied into the idle half a
/// word at a time when it fills: packing bytes into a word held in memory and
/// polling the busy flag per word cost ~56 cycles per byte (measured), most of
/// a 270-cycle LED, and a busy-flag read alone costs ~15 cycles.
struct State {
    union {
        u32 words[kHalfWords];
        u8 bytes[kHalfBytes];
    } stage;
    u8* ptr;  // next free byte in stage
    u8* end;  // commit when ptr reaches this
    u8 high;  // 1 when the next half to send is W8..W15
};

FASTLED_FORCE_INLINE State& state() FL_NO_EXCEPT {
    static State s = {{{0}}, s.stage.bytes, s.stage.bytes + 4, 0};
    return s;
}

inline void waitIdle() FL_NO_EXCEPT {
    while (SPI1CMD & SPIBUSY) {
    }
}

/// Copy `nbytes` staged bytes into the idle half, then wait for the half on
/// the wire to finish and send this one. The copy needs no wait: a command
/// with SPI_USR_DOUT_HIGHPART reads only W8..W15, one without only W0..W7.
FL_NO_INLINE inline void commit(State& s, u32 nbytes) FL_NO_EXCEPT {
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

FASTLED_FORCE_INLINE void push(u8 b) FL_NO_EXCEPT {
    State& s = state();
    *s.ptr++ = b;
    if (s.ptr == s.end) {
        commit(s, u32(s.end - s.stage.bytes));
    }
}

/// Start a transfer: the first chunk is one 4-byte word (APA102's start
/// frame), so the wire starts almost at once.
inline void begin() FL_NO_EXCEPT {
    State& s = state();
    s.ptr = s.stage.bytes;
    s.end = s.stage.bytes + 4;
}

/// Send whatever is staged. The data is already in W0..W15, so this does
/// not wait for the wire: the next commit, select() or stop() does.
inline void flush() FL_NO_EXCEPT {
    State& s = state();
    const u32 nbytes = u32(s.ptr - s.stage.bytes);
    if (nbytes != 0) {
        commit(s, nbytes);
    }
}

/// Send a single bit. writeBit<>() is a one-clock operation that cannot ride
/// the byte staging buffer, so flush what is staged and run a one-bit HSPI
/// command; MSB first, so the bit is the MSB of the data register.
inline void sendBit(u8 bit) FL_NO_EXCEPT {
    flush();
    waitIdle();  // nothing may be on the wire: this command reuses W0
    SPI1U = SPIUMOSI;
    SPI1U1 = 0;  // one bit
    SPI1W0 = bit ? 0x80000000u : 0u;
    SPI1CMD = SPIBUSY;
    waitIdle();
    begin();
}

}  // namespace esp8266_hspi

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_SPEED>
class ESP8266HspiOutput {
    FL_STATIC_ASSERT(DATA_PIN == 13 && CLOCK_PIN == 14,
                  "ESP8266 HSPI is wired to GPIO13 (MOSI) and GPIO14 (SCK) only");

    Selectable* mPSelect;

    static u32 clockValue() FL_NO_EXCEPT {
        // Ceiling division: SPI_SPEED is itself a floored rate (DATA_RATE_MHZ(6)
        // is 80 / 6 = 13 cycles, i.e. 6.15 MHz), and flooring again here would
        // ask for a hair under the rate the divider can actually reach, costing
        // a whole divider step (6.15 -> 5.71 MHz).
        static const u32 reg = esp8266_hspi::clockRegister(
            SPI_SPEED == 0 ? esp8266_hspi::kApbHz
                           : u32((u64(FL_CPU_FREQUENCY()) + SPI_SPEED - 1) / SPI_SPEED));
        return reg;
    }

public:
    ESP8266HspiOutput() FL_NO_EXCEPT : mPSelect(nullptr) {}
    ESP8266HspiOutput(Selectable* pSelect) FL_NO_EXCEPT : mPSelect(pSelect) {}
    void setSelect(Selectable* pSelect) FL_NO_EXCEPT { mPSelect = pSelect; }

    void init() FL_NO_EXCEPT {
        esp8266_hspi::waitIdle();
        // Route the pads to HSPI (MTCK -> HSPID, MTMS -> HSPICLK).
        GPF(DATA_PIN) = GPFFS(GPFFS_BUS(DATA_PIN));
        GPF(CLOCK_PIN) = GPFFS(GPFFS_BUS(CLOCK_PIN));
        SPI1C = 0;   // MSB first, plain single-bit output
        SPI1C1 = 0;
        SPI1P &= ~(1u << 29);  // CPOL 0
        SPI1U = SPIUMOSI;      // MOSI phase only, data on the rising edge (mode 0)
        SPI1U2 = 0;
        esp8266_hspi::begin();
        release();
    }

    static void stop() FL_NO_EXCEPT {
        esp8266_hspi::flush();
        esp8266_hspi::waitIdle();
    }
    static void wait() FL_NO_EXCEPT {}
    // Hands the last chunk to the hardware without waiting for it to shift out.
    static void waitFully() FL_NO_EXCEPT { esp8266_hspi::flush(); }

    FASTLED_FORCE_INLINE static void writeByte(u8 b) FL_NO_EXCEPT { esp8266_hspi::push(b); }
    static void writeByteNoWait(u8 b) FL_NO_EXCEPT { writeByte(b); }
    static void writeBytePostWait(u8 b) FL_NO_EXCEPT { writeByte(b); }
    FASTLED_FORCE_INLINE static void writeWord(u16 w) FL_NO_EXCEPT {
        writeByte(u8(w >> 8));
        writeByte(u8(w & 0xFF));
    }
    // One clock of the given bit (SM16716 and the FLAG_START_BIT writers).
    template <u8 BIT> static void writeBit(u8 b) FL_NO_EXCEPT {
        esp8266_hspi::sendBit(u8((b >> BIT) & 1u));
    }

    // HSPI is shared, so another strip on these pins may have set a
    // different rate: program the clock at the start of every transfer.
    void select() FL_NO_EXCEPT {
        esp8266_hspi::waitIdle();
        const u32 clk = clockValue();
        // The undivided 80 MHz clock also needs the pad mux bit, as in the
        // Arduino core's SPIClass::setClockDivider.
        if (clk == 0x80000000) {
            GPMUX |= (1u << 9);
        } else {
            GPMUX &= ~(1u << 9);
        }
        SPI1CLK = clk;
        esp8266_hspi::begin();
        if (mPSelect != nullptr) {
            mPSelect->select();
        }
    }

    // A chip select must stay asserted until the tail chunk has shifted out.
    void release() FL_NO_EXCEPT {
        if (mPSelect != nullptr) {
            esp8266_hspi::waitIdle();
            mPSelect->release();
        }
    }

    void endTransaction() FL_NO_EXCEPT {
        waitFully();
        release();
    }

    static void finalizeTransmission() FL_NO_EXCEPT {}

    void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
        select();
        writeBytesValueRaw(value, len);
        waitFully();
        release();
    }

    static void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
        while (len-- > 0) {
            writeByte(value);
        }
    }

    template <class D> void writeBytes(u8* data, int len) FL_NO_EXCEPT {
        select();
        u8* end = data + len;
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len);
        waitFully();
        release();
    }

    void writeBytes(u8* data, int len) FL_NO_EXCEPT { writeBytes<DATA_NOP>(data, len); }

    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    FL_NO_INLINE void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NO_EXCEPT {
        (void)context;
        select();
        int len = pixels.mLen;
        while (pixels.has(1)) {
            writeByte(D::adjust(pixels.loadAndScale0()));
            writeByte(D::adjust(pixels.loadAndScale1()));
            writeByte(D::adjust(pixels.loadAndScale2()));
            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len);
        waitFully();
        release();
    }
};

}  // namespace fl
