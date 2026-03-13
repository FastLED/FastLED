#pragma once

#include "fl/gfx/five_bit_hd_gamma.h"
#include "fl/stl/compiler_control.h"
#include "crgb.h"
#include "eorder.h"
#include "cpixel_ledcontroller.h"
#include "fastspi.h"
// IWYU pragma: begin_keep
#include "platforms/shared/spi_pixel_writer.h"  // ok platform headers
// IWYU pragma: end_keep

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// APA102 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// APA102 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(12)
template <
	fl::u8 DATA_PIN, fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	// APA102 has a bug where long strip can't handle full speed due to clock degredation.
	// This only affects long strips, but then again if you have a short strip does 6 mhz actually slow
	// you down?  Probably not. And you can always bump it up for speed. Therefore we are prioritizing
	// "just works" over "fastest possible" here.
	// https://www.pjrc.com/why-apa102-leds-have-trouble-at-24-mhz/
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(6),
	fl::FiveBitGammaCorrectionMode GAMMA_CORRECTION_MODE = fl::kFiveBitGammaCorrectionMode_Null,
	fl::u32 START_FRAME = 0x00000000,
	fl::u32 END_FRAME = 0xFF000000
>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
	typedef fl::SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() {
		mSPI.writeWord(START_FRAME >> 16);
		mSPI.writeWord(START_FRAME & 0xFFFF);
	}
	void endBoundary(int nLeds) {
		int nDWords = (nLeds/32);
		const fl::u8 b0 = fl::u8(END_FRAME >> 24 & 0x000000ff);
		const fl::u8 b1 = fl::u8(END_FRAME >> 16 & 0x000000ff);
		const fl::u8 b2 = fl::u8(END_FRAME >>  8 & 0x000000ff);
		const fl::u8 b3 = fl::u8(END_FRAME >>  0 & 0x000000ff);
		do {
			mSPI.writeByte(b0);
			mSPI.writeByte(b1);
			mSPI.writeByte(b2);
			mSPI.writeByte(b3);
		} while(nDWords--);
	}

	FASTLED_FORCE_INLINE void writeLed(fl::u8 brightness, fl::u8 b0, fl::u8 b1, fl::u8 b2) {
#ifdef FASTLED_SPI_BYTE_ONLY
		mSPI.writeByte(0xE0 | brightness);
		mSPI.writeByte(b0);
		mSPI.writeByte(b1);
		mSPI.writeByte(b2);
#else
		fl::u16 b = 0xE000 | (brightness << 8) | (fl::u16)b0;
		mSPI.writeWord(b);
		fl::u16 w = b1 << 8;
		w |= b2;
		mSPI.writeWord(w);
#endif
	}

	FASTLED_FORCE_INLINE void write2Bytes(fl::u8 b1, fl::u8 b2) {
#ifdef FASTLED_SPI_BYTE_ONLY
		mSPI.writeByte(b1);
		mSPI.writeByte(b2);
#else
		mSPI.writeWord(fl::u16(b1) << 8 | b2);
#endif
	}

public:
	APA102Controller() {}

	virtual void init() override {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) override {
		switch (GAMMA_CORRECTION_MODE) {
			case fl::kFiveBitGammaCorrectionMode_Null: {
				showPixelsDefault(pixels);
				break;
			}
			case fl::kFiveBitGammaCorrectionMode_BitShift: {
				showPixelsGammaBitShift(pixels);
				break;
			}
		}
	}

private:

	static inline void getGlobalBrightnessAndScalingFactors(
		    PixelController<RGB_ORDER> & pixels,
		    fl::u8* out_s0, fl::u8* out_s1, fl::u8* out_s2, fl::u8* out_brightness) {
#if FASTLED_HD_COLOR_MIXING
		fl::u8 brightness;
		pixels.loadRGBScaleAndBrightness(out_s0, out_s1, out_s2, &brightness);
		struct Math {
			static fl::u16 map(fl::u16 x, fl::u16 in_min, fl::u16 in_max, fl::u16 out_min, fl::u16 out_max) {  // okay static in header
				const fl::u16 run = in_max - in_min;
				const fl::u16 rise = out_max - out_min;
				const fl::u16 delta = x - in_min;
				return (delta * rise) / run + out_min;
			}
		};
		// *out_brightness = Math::map(brightness, 0, 255, 0, 31);
		fl::u16 bri = Math::map(brightness, 0, 255, 0, 31);
		if (bri == 0 && brightness != 0) {
			// Fixes https://github.com/FastLED/FastLED/issues/1908
			bri = 1;
		}
		*out_brightness = static_cast<fl::u8>(bri);
		return;
#else
		fl::u8 s0, s1, s2;
		pixels.loadAndScaleRGB(&s0, &s1, &s2);
#if FASTLED_USE_GLOBAL_BRIGHTNESS == 1
		// This function is pure magic.
		const fl::u16 maxBrightness = 0x1F;
		// Use sequential comparisons to avoid nested fl::max (helps AVR register allocation)
		fl::u8 max_s01 = (s0 > s1) ? s0 : s1;
		fl::u8 max_component = (max_s01 > s2) ? max_s01 : s2;
		fl::u16 brightness = ((((fl::u16)max_component + 1) * maxBrightness - 1) >> 8) + 1;
		s0 = (maxBrightness * s0 + (brightness >> 1)) / brightness;
		s1 = (maxBrightness * s1 + (brightness >> 1)) / brightness;
		s2 = (maxBrightness * s2 + (brightness >> 1)) / brightness;
#else
		const fl::u8 brightness = 0x1F;
#endif  // FASTLED_USE_GLOBAL_BRIGHTNESS
		*out_s0 = s0;
		*out_s1 = s1;
		*out_s2 = s2;
		*out_brightness = static_cast<fl::u8>(brightness);
#endif  // FASTLED_HD_COLOR_MIXING
	}

	// Legacy showPixels implementation.
	inline void showPixelsDefault(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();
		fl::u8 s0, s1, s2, global_brightness;
		getGlobalBrightnessAndScalingFactors(pixels, &s0, &s1, &s2, &global_brightness);
		startBoundary();
		while (pixels.has(1)) {
			fl::u8 c0, c1, c2;
			pixels.loadAndScaleRGB(&c0, &c1, &c2);
			writeLed(global_brightness, c0, c1, c2);
			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());

		mSPI.endTransaction();

		// Finalize transmission (no-op on non-ESP32, flushes Quad-SPI on ESP32)
		mSPI.finalizeTransmission();
	}

	inline void showPixelsGammaBitShift(PixelController<RGB_ORDER> & pixels) {
		static constexpr fl::u16 kBatchSize = 8;
		const fl::u16 n = static_cast<fl::u16>(pixels.size());

		// Extract uniform color scale and brightness (constant across all pixels)
		fl::u8 scale_r, scale_g, scale_b, global_brightness;
		#if FASTLED_HD_COLOR_MIXING
		pixels.loadRGBScaleAndBrightness(&scale_r, &scale_g, &scale_b, &global_brightness);
		#else
		pixels.loadAndScaleRGB(&scale_r, &scale_g, &scale_b);
		global_brightness = 255;
		#endif
		const CRGB colors_scale(scale_r, scale_g, scale_b);

		// RGB reorder indices (compile-time constant from RGB_ORDER template param)
		const fl::u8 b0_index = (RGB_ORDER >> 6) & 0x3;
		const fl::u8 b1_index = (RGB_ORDER >> 3) & 0x3;
		const fl::u8 b2_index = RGB_ORDER & 0x3;

		mSPI.select();
		startBoundary();

		// Process pixels in batches of 8 using stack-allocated buffers
		fl::u16 remaining = n;
		while (remaining > 0) {
			const fl::u16 batch = (remaining < kBatchSize) ? remaining : kBatchSize;
			CRGB input_buf[kBatchSize];
			fl::CRGBA5 gamma_buf[kBatchSize];

			// Copy raw pixel bytes into CRGB input buffer
			for (fl::u16 i = 0; i < batch; ++i) {
				const fl::u8* raw = pixels.getRawPixelData();
				input_buf[i] = CRGB(raw[0], raw[1], raw[2]);
				pixels.advanceData();
			}

			fl::five_bit_hd_gamma_bitshift(
				fl::span<const CRGB>(input_buf, batch), colors_scale, global_brightness,
				fl::span<fl::CRGBA5>(gamma_buf, batch));

			for (fl::u16 i = 0; i < batch; ++i) {
				const CRGB& rgb = gamma_buf[i].color;
				writeLed(gamma_buf[i].brightness_5bit,
				         rgb.raw[b0_index], rgb.raw[b1_index], rgb.raw[b2_index]);
			}

			remaining -= batch;
		}

		endBoundary(n);
		mSPI.endTransaction();
		mSPI.finalizeTransmission();
	}

public:
	/// Get the protocol-safe padding byte for APA102
	/// Used for quad-SPI lane padding when strips have different lengths
	/// @returns 0xFF (end frame continuation byte)
	/// @deprecated Use getPaddingLEDFrame() for synchronized latching
	static constexpr fl::u8 getPaddingByte() { return 0xFF; }

	/// Get padding LED frame for synchronized latching in quad-SPI
	/// Returns a black LED frame to prepend to shorter strips, ensuring
	/// all strips finish transmitting simultaneously for synchronized updates
	/// @returns Black LED frame (4 bytes: brightness=0, RGB=0,0,0)
	static fl::span<const fl::u8> getPaddingLEDFrame() {  // okay static in header
		// APA102 LED frame format: [111BBBBB][B][G][R]
		// Black LED: 0xE0 (brightness=0), RGB=0,0,0
		static const fl::u8 frame[] = {  // okay static in header
			0xE0,  // Brightness byte (111 00000 = brightness 0)
			0x00,  // Blue = 0
			0x00,  // Green = 0
			0x00   // Red = 0
		};
		return fl::span<const fl::u8>(frame, 4);
	}

	/// Get size of padding LED frame in bytes
	/// @returns 4 (APA102 uses 4 bytes per LED)
	static constexpr size_t getPaddingLEDFrameSize() {
		return 4;
	}

	/// Calculate total byte count for APA102 protocol
	/// Used for quad-SPI buffer pre-allocation
	/// @param num_leds Number of LEDs in the strip
	/// @returns Total bytes needed (start frame + LED data + end frame)
	static constexpr size_t calculateBytes(size_t num_leds) {
		// APA102 protocol:
		// - Start frame: 4 bytes (0x00000000)
		// - LED data: 4 bytes per LED (brightness + RGB)
		// - End frame: (num_leds / 32) + 1 DWords = 4 * ((num_leds / 32) + 1) bytes
		return 4 + (num_leds * 4) + (4 * ((num_leds / 32) + 1));
	}
};

/// APA102 high definition controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(24)
template <
	fl::u8 DATA_PIN,
	fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	// APA102 has a bug where long strip can't handle full speed due to clock degredation.
	// This only affects long strips, but then again if you have a short strip does 6 mhz actually slow
	// you down?  Probably not. And you can always bump it up for speed. Therefore we are prioritizing
	// "just works" over "fastest possible" here.
	// https://www.pjrc.com/why-apa102-leds-have-trouble-at-24-mhz/
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(6)
>
class APA102ControllerHD : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	fl::kFiveBitGammaCorrectionMode_BitShift,
	fl::u32(0x00000000),
	fl::u32(0x00000000)> {
public:
  APA102ControllerHD() = default;
  APA102ControllerHD(const APA102ControllerHD&) = delete;
};

/// SK9822 controller class. It's exactly the same as the APA102Controller protocol but with a different END_FRAME and default SPI_SPEED.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(24)
template <
	fl::u8 DATA_PIN,
	fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(12)
>
class SK9822Controller : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	fl::kFiveBitGammaCorrectionMode_Null,
	0x00000000,
	0x00000000
> {
};

/// SK9822 controller class. It's exactly the same as the APA102Controller protocol but with a different END_FRAME and default SPI_SPEED.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(24)
template <
	fl::u8 DATA_PIN,
	fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(12)
>
class SK9822ControllerHD : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	fl::kFiveBitGammaCorrectionMode_BitShift,
	0x00000000,
	0x00000000
> {
};

/// HD107 is just the APA102 with a default 40Mhz clock rate.
template <
	fl::u8 DATA_PIN,
	fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(40)
>
class HD107Controller : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	fl::kFiveBitGammaCorrectionMode_Null,
	0x00000000,
	0x00000000
> {};

/// HD107HD is just the APA102HD with a default 40Mhz clock rate.
template <
	fl::u8 DATA_PIN,
	fl::u8 CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	fl::u32 SPI_SPEED = DATA_RATE_MHZ(40)
>
class HD107HDController : public APA102ControllerHD<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED> {
};
