#pragma once

// IWYU pragma: private
#include "fl/system/fastpin.h"
#include "fl/system/delay.h"
#include "fl/stl/static_assert.h"
#include "platforms/is_platform.h"
#include "platforms/my9221.h"
#include "platforms/shared/spi_bitbang/bitbang_delay.h"

namespace fl {
namespace detail {

// This is a wait-observation seam, not a second serializer. Real targets use
// the SPI-local minimum-cycle backend; host pin models only observe the schedule.
template <int DATA_PIN, fl::u8 CLOCK_PIN> struct My9221Timing {
    typedef fl::detail::My9221InterruptGuard<DATA_PIN> Guard;
    template <fl::u32 NS>
    static FASTLED_FORCE_INLINE void waitNanoseconds() FL_NO_EXCEPT {
        FL_STATIC_ASSERT(FL_CPU_FREQUENCY() > 0, "MY9221 requires a positive CPU frequency");
        FL_STATIC_ASSERT(static_cast<fl::u64>(FL_CPU_FREQUENCY()) <= 0xffffffffULL,
                      "MY9221 CPU frequency must fit unsigned32");
        // Both factors fit unsigned32, so their product and the rounding
        // addition fit unsigned64. Round up: a fractional cycle cannot meet
        // the requested minimum. NS=0 intentionally remains a no-op.
        constexpr fl::u64 cycles =
            (static_cast<fl::u64>(NS) * FL_CPU_FREQUENCY() + 999999999ULL) /
            1000000000ULL;
        fl::detail::SpiCycleWait<static_cast<fl::i64>(cycles)>::run();
    }
    static void waitMicroseconds(fl::u32 us) FL_NO_EXCEPT {
        fl::delayMicroseconds(us);
    }
};

template <int Data, fl::u8 Clock> struct My9221StaticPins {
    FASTLED_FORCE_INLINE void dataHi() const FL_NO_EXCEPT { fl::FastPin<Data>::hi(); }
    FASTLED_FORCE_INLINE void dataLo() const FL_NO_EXCEPT { fl::FastPin<Data>::lo(); }
    FASTLED_FORCE_INLINE void clockToggle() const FL_NO_EXCEPT { fl::FastPin<Clock>::toggle(); }
    FASTLED_FORCE_INLINE void clockLo() const FL_NO_EXCEPT { fl::FastPin<Clock>::lo(); }
};

// One serializer body for static pins and the ESP IRAM callback boundaries.
// Pin operations keep the actual selected FastPin semantics in both routes.
template <typename Pins, typename Timing> struct My9221Serializer {
    static FASTLED_FORCE_INLINE void writeBit(const Pins& pins, fl::u8 bit) FL_NO_EXCEPT {
        if (bit) pins.dataHi();
        else pins.dataLo();
        Timing::template waitNanoseconds<10>();
        pins.clockToggle();
        Timing::template waitNanoseconds<50>();
    }
    static FASTLED_FORCE_INLINE void sendWord(const Pins& pins, fl::u16 data) FL_NO_EXCEPT {
        for (int i = 0; i < 16; ++i) {
            writeBit(pins, (data & 0x8000) ? 1 : 0);
            data = static_cast<fl::u16>(data << 1);
        }
    }
    static FASTLED_FORCE_INLINE void sendLed(const Pins& pins, fl::u8 c0, fl::u8 c1, fl::u8 c2) FL_NO_EXCEPT {
        sendWord(pins, c0);
        sendWord(pins, c1);
        sendWord(pins, c2);
    }
    static FASTLED_FORCE_INLINE void run(const fl::u16* frame, fl::size words,
                                         const Pins& pins) FL_NO_EXCEPT {
        typename Timing::Guard guard;
        pins.dataLo();
        pins.clockLo();
        for (fl::size chip = 0; chip < words; chip += 13) {
            sendWord(pins, frame[chip]);
            for (fl::size channel = 1; channel < 13; channel += 3) {
                sendLed(pins, static_cast<fl::u8>(frame[chip + channel]),
                        static_cast<fl::u8>(frame[chip + channel + 1]),
                        static_cast<fl::u8>(frame[chip + channel + 2]));
            }
        }
        // Final writeBit supplied hold before parking and restoring prior IRQ.
        pins.clockLo();
        pins.dataLo();
    }
};

#if (!defined(FL_IS_ESP8266) && !defined(FL_IS_ESP32)) || defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
template <int Data, fl::u8 Clock> struct My9221EmitterDispatch {
    static FL_NO_INLINE void FL_IRAM run(const fl::u16* frame, fl::size words) FL_NO_EXCEPT {
        const My9221StaticPins<Data, Clock> pins;
        My9221Serializer<My9221StaticPins<Data, Clock>, My9221Timing<Data, Clock>>::run(frame, words, pins);
    }
};
#endif

} // namespace detail
} // namespace fl

