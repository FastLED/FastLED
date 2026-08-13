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

	/// Command word: 8-bit grayscale (CMD[9:8]=00), APDM waveform (CMD[4]=1).
	static constexpr fl::u16 kCommandWord = 0x0010;

	/// Datasheet latch: hold DCKI low for >220 us after last data, then pulse DI 4x.
	static constexpr fl::u32 kLatchDelayUs = 230;

	FASTLED_FORCE_INLINE void writeBit(fl::u8 bit) {
		if (bit) {
			DataPin::hi();
		} else {
			DataPin::lo();
		}
		// Dual-edge: one toggle clocks one bit.
		ClockPin::toggle();
	}

	/// Shift out one 16-bit word MSB-first, one clock edge per bit.
	void sendWord(fl::u16 data) {
		for (int i = 0; i < 16; ++i) {
			writeBit((data & 0x8000) ? 1 : 0);
			data = static_cast<fl::u16>(data << 1);
		}
	}

	void sendLed(fl::u8 c0, fl::u8 c1, fl::u8 c2) {
		sendWord(c0);
		sendWord(c1);
		sendWord(c2);
	}

	void latch() {
		ClockPin::lo();
		DataPin::lo();
		fl::delayMicroseconds(kLatchDelayUs);

		// Four DI pulses while DCKI is held low.
		for (int i = 0; i < 4; ++i) {
			DataPin::hi();
			DataPin::lo();
		}
	}

public:
	MY9221Controller() FL_NO_EXCEPT {}

	virtual void init() {
		DataPin::setOutput();
		ClockPin::setOutput();
		DataPin::lo();
		ClockPin::lo();
		(void)SPI_SPEED;
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		// Ensure known idle levels before the first edge.
		DataPin::lo();
		ClockPin::lo();

		while (pixels.has(1)) {
			fl::u8 ch[4][3] = {
				{0, 0, 0},
				{0, 0, 0},
				{0, 0, 0},
				{0, 0, 0}
			};

			// Map up to 4 CRGB pixels onto one chip: LED0->OUT0 .. LED3->OUT3.
			for (int led = 0; led < 4 && pixels.has(1); ++led) {
				ch[led][0] = pixels.loadAndScale0();
				ch[led][1] = pixels.loadAndScale1();
				ch[led][2] = pixels.loadAndScale2();
				pixels.advanceData();
				pixels.stepDithering();
			}

			sendWord(kCommandWord);

			// Hardware shift order is OUT3, OUT2, OUT1, OUT0.
			for (int out = 3; out >= 0; --out) {
				sendLed(ch[out][0], ch[out][1], ch[out][2]);
			}
		}

		latch();
	}
};
