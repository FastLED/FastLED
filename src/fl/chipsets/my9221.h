#pragma once

/// @file fl/chipsets/my9221.h
/// @brief MY9221 12-channel LED driver controller
///
/// The MY9221 is a 12-channel constant-current LED driver with a 2-wire DI/DCKI
/// interface. Unlike typical SPI LED chipsets, it samples data on *every* clock
/// edge (dual-edge / DDR clocking), so this controller bit-bangs via FastPin
/// rather than using FastLED's SPI abstraction.
///
/// Channel mapping (chained RGB modules such as Grove Chainable RGB LED):
///   12 channels = 4 RGB LEDs per chip.
///   `addLeds<MY9221, DATA, CLOCK>(leds, N)` drives ceil(N/4) chained chips,
///   padding the last chip's unused channels with zero.
///
/// Grove LED Bar (10 monochrome channels) is out of scope.
///
/// Default command word is 0x0010: 8-bit grayscale + APDM waveform. This matches
/// a bench-verified ESP32 driver and maps natively onto 8-bit CRGB. 16-bit
/// grayscale mode is left as future work.
///
/// Protocol per chip (208 bits):
///   16-bit command word, then 12 x 16-bit grayscale words in OUT3..OUT0 order
///   (each OUT is DA/DB/DC = RGB_ORDER channels 0/1/2), then a pin-level latch.

#include "pixeltypes.h"
#include "crgb.h"
#include "eorder.h"
#include "fl/stl/noexcept.h"
#include "fl/system/fastpin.h"
#include "fl/system/delay.h"
#include "fl/stl/vector.h"
#include "fl/log/log.h"
#include "fl/stl/static_assert.h"
#include "platforms/my9221_emitter.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MY9221 definition - dual-edge (DDR) clocked 12-channel driver
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// MY9221 controller class.
/// @tparam DATA_PIN the data (DI) pin for these LEDs
/// @tparam CLOCK_PIN the clock (DCKI) pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED unused - retained for `_FL_MAP_CLOCKED_CHIPSET` API compatibility.
///                    The MY9221 uses dual-edge bit-banging, not SPI.
template <int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER = RGB, fl::u32 SPI_SPEED = DATA_RATE_MHZ(1)>
class MY9221Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::FastPin<DATA_PIN> DataPin;
	typedef fl::FastPin<CLOCK_PIN> ClockPin;
	typedef fl::detail::My9221Timing<DATA_PIN, CLOCK_PIN> Timing;
	fl::vector<fl::u16> mFrame; // 13 words (26 bytes) per configured chip.
	fl::u32 mStopDelayUs = 1;

	/// Command word: 8-bit grayscale (CMD[9:8]=00), APDM waveform (CMD[4]=1).
	static constexpr fl::u16 kCommandWord = 0x0010;

	/// Datasheet latch: hold DCKI low for >220 us after last data, then pulse DI 4x.
	static constexpr fl::u32 kLatchDelayUs = 230;


	void latch() FL_NO_EXCEPT {
		ClockPin::lo();
		DataPin::lo();
		Timing::waitMicroseconds(kLatchDelayUs);

		// Four DI pulses while DCKI is held low.
		for (int i = 0; i < 4; ++i) {
            DataPin::hi();
            Timing::template waitNanoseconds<80>(); // Request >70ns; target codegen remains separately verified.
            DataPin::lo();
            Timing::template waitNanoseconds<240>(); // Request >230ns; not a physical measurement.
        }
        Timing::waitMicroseconds(mStopDelayUs);
    }

    bool prepareFrame(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT {
        mFrame.clear();
        if (pixels.mLenRemaining <= 0) return false;
        const fl::size count = static_cast<fl::size>(pixels.mLenRemaining);
        const fl::size chips = count / 4 + (count % 4 != 0 ? 1 : 0);
        if (chips > static_cast<fl::size>(-1) / 26) {
            FL_WARN("MY9221 frame exceeds addressable storage");
            return false;
        }
        const fl::u64 stop_us = (200ULL + static_cast<fl::u64>(chips) * 10) / 1000 + 1;
        if (stop_us > static_cast<fl::u32>(-1)) {
            FL_WARN("MY9221 cascade stop exceeds supported delay range");
            return false;
        }
        const fl::size words = chips * 13;
        // reserve leaves capacity unchanged when the resource returns null;
        // check before resize so no element write can exceed actual capacity.
        mFrame.reserve(words);
        if (mFrame.capacity() < words) {
            FL_WARN("MY9221 could not allocate complete frame");
            return false;
        }
        mFrame.resize(words);
        if (mFrame.size() != words) return false;
        mStopDelayUs = static_cast<fl::u32>(stop_us);
        fl::size pos = 0;
        while (pixels.has(1)) {
            fl::u8 ch[4][3] = {};
            for (int led = 0; led < 4 && pixels.has(1); ++led) {
                ch[led][0] = pixels.loadAndScale0();
                ch[led][1] = pixels.loadAndScale1();
                ch[led][2] = pixels.loadAndScale2();
                pixels.advanceData();
                pixels.stepDithering();
            }
            mFrame[pos++] = kCommandWord;
            for (int out = 3; out >= 0; --out) {
                mFrame[pos++] = ch[out][0];
                mFrame[pos++] = ch[out][1];
                mFrame[pos++] = ch[out][2];
            }
        }
        return pos == words;
    }

public:
	MY9221Controller() FL_NO_EXCEPT : mFrame(fl::detail::my9221FrameResource()) {}

	virtual void init() FL_NO_EXCEPT {
		DataPin::setOutput();
		ClockPin::setOutput();
		DataPin::lo();
		ClockPin::lo();
		(void)SPI_SPEED;
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT {
        // No allocation, pixel scaling or dithering occurs between active
        // edges, including chip boundaries. Empty/failed frames emit nothing.
        if (!prepareFrame(pixels)) return;
        const fl::u16* frame = mFrame.data();
        const fl::size words = mFrame.size();
        fl::detail::My9221EmitterDispatch<DATA_PIN, CLOCK_PIN>::run(frame, words);
        latch();
        // Target WCET/cache/NMI bounds remain separate requirements:
        // interrupt masking and minimum waits alone do not prove <=7200ns.

	}
};
