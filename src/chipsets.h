#ifndef __INC_CHIPSETS_H
#define __INC_CHIPSETS_H

#include "pixeltypes.h"
#include "five_bit_hd_gamma.h"
#include "fl/force_inline.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "eorder.h"



#ifndef FASTLED_CLOCKLESS_USES_NANOSECONDS
 #if defined(FASTLED_TEENSY4)
   #define FASTLED_CLOCKLESS_USES_NANOSECONDS 1
 #elif defined(ESP32)
   #include "third_party/espressif/led_strip/src/enabled.h"
   // RMT 5.1 driver converts from nanoseconds to RMT ticks.
   #if FASTLED_RMT5
	 #define FASTLED_CLOCKLESS_USES_NANOSECONDS 1
   #else
   	 #define FASTLED_CLOCKLESS_USES_NANOSECONDS 0
   #endif
 #else
   #define FASTLED_CLOCKLESS_USES_NANOSECONDS 0
 #endif  // FASTLED_TEENSY4
#endif  // FASTLED_CLOCKLESS_USES_NANOSECONDS


#ifdef __IMXRT1062__
#include "platforms/arm/k20/clockless_objectfled.h"
#endif

/// @file chipsets.h
/// Contains the bulk of the definitions for the various LED chipsets supported.



/// @defgroup Chipsets LED Chipset Controllers
/// Implementations of ::CLEDController classes for various led chipsets.
///
/// @{

#if defined(ARDUINO) //&& defined(SoftwareSerial_h)


#if defined(SoftwareSerial_h) || defined(__SoftwareSerial_h)
#include <SoftwareSerial.h>

#define HAS_PIXIE

FASTLED_NAMESPACE_BEGIN

/// Adafruit Pixie controller class
/// @tparam DATA_PIN the pin to write data out on
/// @tparam RGB_ORDER the RGB ordering for the LED data
template<uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PixieController : public CPixelLEDController<RGB_ORDER> {
	SoftwareSerial Serial;
	CMinWait<2000> mWait;

public:
	PixieController() : Serial(-1, DATA_PIN) {}

protected:
	/// Initialize the controller
	virtual void init() {
		Serial.begin(115200);
		mWait.mark();
	}

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWait.wait();
		while(pixels.has(1)) {
			uint8_t r = pixels.loadAndScale0();
			Serial.write(r);
			uint8_t g = pixels.loadAndScale1();
			Serial.write(g);
			uint8_t b = pixels.loadAndScale2();
			Serial.write(b);
			pixels.advanceData();
			pixels.stepDithering();
		}
		mWait.mark();
	}

};

// template<SoftwareSerial & STREAM, EOrder RGB_ORDER = RGB>
// class PixieController : public PixieBaseController<STREAM, RGB_ORDER> {
// public:
// 	virtual void init() {
// 		STREAM.begin(115200);
// 	}
// };

FASTLED_NAMESPACE_END
#endif
#endif

// Emulution layer to support RGBW leds on RGB controllers. This works by creating
// a side buffer dedicated for the RGBW data. The RGB data is then converted to RGBW
// and sent to the delegate controller for rendering as if it were RGB data.
FASTLED_NAMESPACE_BEGIN
template <
	typename CONTROLLER,
	EOrder RGB_ORDER = GRB>  // Default on WS2812>
class RGBWEmulatedController
    : public CPixelLEDController<RGB_ORDER, CONTROLLER::LANES_VALUE,
                                 CONTROLLER::MASK_VALUE> {
  public:
    // ControllerT is a helper class.  It subclasses the device controller class
    // and has three methods to call the three protected methods we use.
    // This is janky, but redeclaring public methods protected in a derived class
    // is janky, too.

    // N.B., byte order must be RGB.
	typedef CONTROLLER ControllerBaseT;
    class ControllerT : public CONTROLLER {
        friend class RGBWEmulatedController<CONTROLLER, RGB_ORDER>;
        void *callBeginShowLeds(int size) { return ControllerBaseT::beginShowLeds(size); }
        void callShow(CRGB *data, int nLeds, uint8_t brightness) {
            ControllerBaseT::show(data, nLeds, brightness);
        }
        void callEndShowLeds(void *data) { ControllerBaseT::endShowLeds(data); }
    };


    static const int LANES = CONTROLLER::LANES_VALUE;
    static const uint32_t MASK = CONTROLLER::MASK_VALUE;

    // The delegated controller must do no reordering.
    static_assert(RGB == CONTROLLER::RGB_ORDER_VALUE, "The delegated controller MUST NOT do reordering");

    RGBWEmulatedController(const Rgbw& rgbw = RgbwDefault()) {
        this->setRgbw(rgbw);
    };
    ~RGBWEmulatedController() { delete[] mRGBWPixels; }

	virtual void *beginShowLeds(int size) override {
		return mController.callBeginShowLeds(Rgbw::size_as_rgb(size));
	}

	virtual void endShowLeds(void *data) override {
		return mController.callEndShowLeds(data);
	}

    virtual void showPixels(PixelController<RGB_ORDER, LANES, MASK> &pixels) override {
        // Ensure buffer is large enough
        ensureBuffer(pixels.size());
		Rgbw rgbw = this->getRgbw();
        uint8_t *data = reinterpret_cast<uint8_t *>(mRGBWPixels);
        while (pixels.has(1)) {
            pixels.stepDithering();
            pixels.loadAndScaleRGBW(rgbw, data, data + 1, data + 2, data + 3);
            data += 4;
            pixels.advanceData();
        }

		// Force the device controller to a state where it passes data through
		// unmodified: color correction, color temperature, dither, and brightness
		// (passed as an argument to show()).  Temporarily enable the controller,
		// show the LEDs, and disable it again.
		//
		// The device controller is in the global controller list, so if we 
		// don't keep it disabled, it will refresh again with unknown brightness,
		// temperature, etc.

		mController.setCorrection(CRGB(255, 255, 255));
		mController.setTemperature(CRGB(255, 255, 255));
		mController.setDither(DISABLE_DITHER);

		mController.setEnabled(true);
		mController.callShow(mRGBWPixels, Rgbw::size_as_rgb(pixels.size()), 255);
		mController.setEnabled(false);
    }

  private:
    // Needed by the interface.
    void init() override {
		mController.init();
		mController.setEnabled(false);
	}

    void ensureBuffer(int32_t num_leds) {
        if (num_leds != mNumRGBLeds) {
            mNumRGBLeds = num_leds;
            // The delegate controller expects the raw pixel byte data in multiples of 3.
            // In the case of src data not a multiple of 3, then we need to
            // add pad bytes so that the delegate controller doesn't walk off the end
            // of the array and invoke a buffer overflow panic.
            uint32_t new_size = Rgbw::size_as_rgb(num_leds);
            delete[] mRGBWPixels;
            mRGBWPixels = new CRGB[new_size];
			// showPixels may never clear the last two pixels.
			for (uint32_t i = 0; i < new_size; i++) {
				mRGBWPixels[i] = CRGB(0, 0, 0);
			}

			mController.setLeds(mRGBWPixels, new_size);
        }
    }

    CRGB *mRGBWPixels = nullptr;
    int32_t mNumRGBLeds = 0;
    int32_t mNumRGBWLeds = 0;
    ControllerT mController; // Real controller.
};

/// @defgroup ClockedChipsets Clocked Chipsets
/// Nominally SPI based, these chipsets have a data and a clock line.
/// @{

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LPD8806 controller class - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// LPD8806 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(12)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB,  uint32_t SPI_SPEED = DATA_RATE_MHZ(12) >
class LPD8806Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;

	class LPD8806_ADJUST {
	public:
		// LPD8806 spec wants the high bit of every rgb data byte sent out to be set.
		FASTLED_FORCE_INLINE static uint8_t adjust(FASTLED_REGISTER uint8_t data) { return ((data>>1) | 0x80) + ((data && (data<254)) & 0x01); }
		FASTLED_FORCE_INLINE static void postBlock(int len, void* context = NULL) {
			SPI* pSPI = static_cast<SPI*>(context);
			pSPI->writeBytesValueRaw(0, ((len*3+63)>>6));
		}

	};

	SPI mSPI;

public:
	LPD8806Controller()  {}
	virtual void init() {
		mSPI.init();
	}

protected:

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.template writePixels<0, LPD8806_ADJUST, RGB_ORDER>(pixels, &mSPI);
	}
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WS2801 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// WS2801 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(1)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(1)>
class WS2801Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;
	CMinWait<1000>  mWaitDelay;

public:
	WS2801Controller() {}

	/// Initialize the controller
	virtual void init() {
		mSPI.init();
	  mWaitDelay.mark();
	}

protected:

	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWaitDelay.wait();
		mSPI.template writePixels<0, DATA_NOP, RGB_ORDER>(pixels, NULL);
		mWaitDelay.mark();
	}
};

/// WS2803 controller class.
/// @copydetails WS2801Controller
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(25)>
class WS2803Controller : public WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_SPEED> {};

/// LPD6803 controller class (LPD1101).
/// 16 bit (1 bit const "1", 5 bit red, 5 bit green, 5 bit blue).
/// In chip CMODE pin must be set to 1 (inside oscillator mode).
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(12)
/// @see Datasheet: https://cdn-shop.adafruit.com/datasheets/LPD6803.pdf
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(12)>
class LPD6803Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() { mSPI.writeByte(0); mSPI.writeByte(0); mSPI.writeByte(0); mSPI.writeByte(0); }
	void endBoundary(int nLeds) { int nDWords = (nLeds/32); do { mSPI.writeByte(0xFF); mSPI.writeByte(0x00); mSPI.writeByte(0x00); mSPI.writeByte(0x00); } while(nDWords--); }

public:
	LPD6803Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		startBoundary();
		while(pixels.has(1)) {
            FASTLED_REGISTER uint16_t command;
            command = 0x8000;
            command |= (pixels.loadAndScale0() & 0xF8) << 7; // red is the high 5 bits
            command |= (pixels.loadAndScale1() & 0xF8) << 2; // green is the middle 5 bits
			mSPI.writeByte((command >> 8) & 0xFF);
            command |= pixels.loadAndScale2() >> 3 ; // blue is the low 5 bits
			mSPI.writeByte(command & 0xFF);

			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());
		mSPI.waitFully();
		mSPI.release();
	}

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	uint8_t DATA_PIN, uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	// APA102 has a bug where long strip can't handle full speed due to clock degredation.
	// This only affects long strips, but then again if you have a short strip does 6 mhz actually slow
	// you down?  Probably not. And you can always bump it up for speed. Therefore we are prioritizing
	// "just works" over "fastest possible" here.
	// https://www.pjrc.com/why-apa102-leds-have-trouble-at-24-mhz/
	uint32_t SPI_SPEED = DATA_RATE_MHZ(6),
	FiveBitGammaCorrectionMode GAMMA_CORRECTION_MODE = kFiveBitGammaCorrectionMode_Null,
	uint32_t START_FRAME = 0x00000000,
	uint32_t END_FRAME = 0xFF000000
>
class APA102Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void startBoundary() {
		mSPI.writeWord(START_FRAME >> 16);
		mSPI.writeWord(START_FRAME & 0xFFFF);
	}
	void endBoundary(int nLeds) {
		int nDWords = (nLeds/32);
		const uint8_t b0 = uint8_t(END_FRAME >> 24 & 0x000000ff);
		const uint8_t b1 = uint8_t(END_FRAME >> 16 & 0x000000ff);
		const uint8_t b2 = uint8_t(END_FRAME >>  8 & 0x000000ff);
		const uint8_t b3 = uint8_t(END_FRAME >>  0 & 0x000000ff);
		do {
			mSPI.writeByte(b0);
			mSPI.writeByte(b1);
			mSPI.writeByte(b2);
			mSPI.writeByte(b3);
		} while(nDWords--);
	}

	FASTLED_FORCE_INLINE void writeLed(uint8_t brightness, uint8_t b0, uint8_t b1, uint8_t b2) {
#ifdef FASTLED_SPI_BYTE_ONLY
		mSPI.writeByte(0xE0 | brightness);
		mSPI.writeByte(b0);
		mSPI.writeByte(b1);
		mSPI.writeByte(b2);
#else
		uint16_t b = 0xE000 | (brightness << 8) | (uint16_t)b0;
		mSPI.writeWord(b);
		uint16_t w = b1 << 8;
		w |= b2;
		mSPI.writeWord(w);
#endif
	}

	FASTLED_FORCE_INLINE void write2Bytes(uint8_t b1, uint8_t b2) {
#ifdef FASTLED_SPI_BYTE_ONLY
		mSPI.writeByte(b1);
		mSPI.writeByte(b2);
#else
		mSPI.writeWord(uint16_t(b1) << 8 | b2);
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
			case kFiveBitGammaCorrectionMode_Null: {
				showPixelsDefault(pixels);
				break;
			}
			case kFiveBitGammaCorrectionMode_BitShift: {
				showPixelsGammaBitShift(pixels);
				break;
			}
		}
	}

private:

	static inline void getGlobalBrightnessAndScalingFactors(
		    PixelController<RGB_ORDER> & pixels,
		    uint8_t* out_s0, uint8_t* out_s1, uint8_t* out_s2, uint8_t* out_brightness) {
#if FASTLED_HD_COLOR_MIXING
		uint8_t brightness;
		pixels.getHdScale(out_s0, out_s1, out_s2, &brightness);
		struct Math {
			static uint16_t map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
				const uint16_t run = in_max - in_min;
				const uint16_t rise = out_max - out_min;
				const uint16_t delta = x - in_min;
				return (delta * rise) / run + out_min;
			}
		};
		*out_brightness = Math::map(brightness, 0, 255, 0, 31);
		return;
#else
		uint8_t s0, s1, s2;
		pixels.loadAndScaleRGB(&s0, &s1, &s2);
#if FASTLED_USE_GLOBAL_BRIGHTNESS == 1
		// This function is pure magic.
		const uint16_t maxBrightness = 0x1F;
		uint16_t brightness = ((((uint16_t)max(max(s0, s1), s2) + 1) * maxBrightness - 1) >> 8) + 1;
		s0 = (maxBrightness * s0 + (brightness >> 1)) / brightness;
		s1 = (maxBrightness * s1 + (brightness >> 1)) / brightness;
		s2 = (maxBrightness * s2 + (brightness >> 1)) / brightness;
#else
		const uint8_t brightness = 0x1F;
#endif  // FASTLED_USE_GLOBAL_BRIGHTNESS
		*out_s0 = s0;
		*out_s1 = s1;
		*out_s2 = s2;
		*out_brightness = static_cast<uint8_t>(brightness);
#endif  // FASTLED_HD_COLOR_MIXING
	}

	// Legacy showPixels implementation.
	inline void showPixelsDefault(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();
		uint8_t s0, s1, s2, global_brightness;
		getGlobalBrightnessAndScalingFactors(pixels, &s0, &s1, &s2, &global_brightness);
		startBoundary();
		while (pixels.has(1)) {
			uint8_t c0, c1, c2;
			pixels.loadAndScaleRGB(&c0, &c1, &c2);
			writeLed(global_brightness, c0, c1, c2);
			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());

		mSPI.waitFully();
		mSPI.release();
	}

	inline void showPixelsGammaBitShift(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();
		startBoundary();
		while (pixels.has(1)) {
			// Load raw uncorrected r,g,b values.
			uint8_t brightness, c0, c1, c2;  // c0-c2 is the RGB data re-ordered for pixel
			pixels.loadAndScale_APA102_HD(&c0, &c1, &c2, &brightness);
			writeLed(brightness, c0, c1, c2);
			pixels.stepDithering();
			pixels.advanceData();
		}
		endBoundary(pixels.size());
		mSPI.waitFully();
		mSPI.release();
	}
};

/// APA102 high definition controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(24)
template <
	uint8_t DATA_PIN,
	uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	// APA102 has a bug where long strip can't handle full speed due to clock degredation.
	// This only affects long strips, but then again if you have a short strip does 6 mhz actually slow
	// you down?  Probably not. And you can always bump it up for speed. Therefore we are prioritizing
	// "just works" over "fastest possible" here.
	// https://www.pjrc.com/why-apa102-leds-have-trouble-at-24-mhz/
	uint32_t SPI_SPEED = DATA_RATE_MHZ(6)
>
class APA102ControllerHD : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN, 
	RGB_ORDER,
	SPI_SPEED,
	kFiveBitGammaCorrectionMode_BitShift,
	uint32_t(0x00000000),
	uint32_t(0x00000000)> {
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
	uint8_t DATA_PIN,
	uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	uint32_t SPI_SPEED = DATA_RATE_MHZ(12)
>
class SK9822Controller : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	kFiveBitGammaCorrectionMode_Null,
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
	uint8_t DATA_PIN,
	uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	uint32_t SPI_SPEED = DATA_RATE_MHZ(12)
>
class SK9822ControllerHD : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	kFiveBitGammaCorrectionMode_BitShift,
	0x00000000,
	0x00000000
> {
};


/// HD107 is just the APA102 with a default 40Mhz clock rate.
template <
	uint8_t DATA_PIN,
	uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	uint32_t SPI_SPEED = DATA_RATE_MHZ(40)
>
class HD107Controller : public APA102Controller<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED,
	kFiveBitGammaCorrectionMode_Null,
	0x00000000,
	0x00000000
> {};

/// HD107HD is just the APA102HD with a default 40Mhz clock rate.
template <
	uint8_t DATA_PIN,
	uint8_t CLOCK_PIN,
	EOrder RGB_ORDER = RGB,
	uint32_t SPI_SPEED = DATA_RATE_MHZ(40)\
>
class HD107HDController : public APA102ControllerHD<
	DATA_PIN,
	CLOCK_PIN,
	RGB_ORDER,
	SPI_SPEED> {
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// P9813 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// P9813 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(10)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(10)>
class P9813Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeBoundary() { mSPI.writeWord(0); mSPI.writeWord(0); }

	FASTLED_FORCE_INLINE void writeLed(uint8_t r, uint8_t g, uint8_t b) {
		FASTLED_REGISTER uint8_t top = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
		mSPI.writeByte(top); mSPI.writeByte(b); mSPI.writeByte(g); mSPI.writeByte(r);
	}

public:
	P9813Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mSPI.select();

		writeBoundary();
		while(pixels.has(1)) {
			writeLed(pixels.loadAndScale0(), pixels.loadAndScale1(), pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
		writeBoundary();
		mSPI.waitFully();

		mSPI.release();
	}

};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SM16716 definition - takes data/clock/select pin values (N.B. should take an SPI definition?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// SM16716 controller class.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam CLOCK_PIN the clock pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
/// @tparam SPI_SPEED the clock divider used for these LEDs.  Set using the ::DATA_RATE_MHZ / ::DATA_RATE_KHZ macros.  Defaults to ::DATA_RATE_MHZ(16)
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER = RGB, uint32_t SPI_SPEED = DATA_RATE_MHZ(16)>
class SM16716Controller : public CPixelLEDController<RGB_ORDER> {
	typedef SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED> SPI;
	SPI mSPI;

	void writeHeader() {
		// Write out 50 zeros to the spi line (6 blocks of 8 followed by two single bit writes)
		mSPI.select();
		mSPI.template writeBit<0>(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.template writeBit<0>(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.writeByte(0);
		mSPI.waitFully();
		mSPI.release();
	}

public:
	SM16716Controller() {}

	virtual void init() {
		mSPI.init();
	}

protected:
	/// @copydoc CPixelLEDController::showPixels()
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		// Make sure the FLAG_START_BIT flag is set to ensure that an extra 1 bit is sent at the start
		// of each triplet of bytes for rgb data
		// writeHeader();
		mSPI.template writePixels<FLAG_START_BIT, DATA_NOP, RGB_ORDER>(pixels, NULL);
		writeHeader();
	}

};

/// @} ClockedChipsets


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clockless template instantiations - see clockless.h for how the timing values are used
//

#ifdef FASTLED_HAS_CLOCKLESS
/// @defgroup ClocklessChipsets Clockless Chipsets
/// These chipsets have only a single data line. 
///
/// The clockless chipset controllers use the same base class
/// and the same protocol, but with varying timing periods.
///
/// These controllers have 3 control points in their cycle for each bit:
///   @code
///   At T=0        : the line is raised hi to start a bit
///   At T=T1       : the line is dropped low to transmit a zero bit
///   At T=T1+T2    : the line is dropped low to transmit a one bit
///   At T=T1+T2+T3 : the cycle is concluded (next bit can be sent)
///   @endcode
///
/// The units used for T1, T2, and T3 is nanoseconds.  
///
/// For 8MHz/16MHz/24MHz frequencies, these values are also guaranteed
/// to be integral multiples of an 8MHz clock (125ns increments).
///
/// @note The base class, ClocklessController, is platform-specific.
/// @{

// Allow clock that clockless controller is based on to have different
// frequency than the CPU.
#if !defined(CLOCKLESS_FREQUENCY)
    #define CLOCKLESS_FREQUENCY F_CPU
#endif

// We want to force all avr's to use the Trinket controller when running at 8Mhz, because even the 328's at 8Mhz
// need the more tightly defined timeframes.
#if defined(__LGT8F__) || (CLOCKLESS_FREQUENCY == 8000000 || CLOCKLESS_FREQUENCY == 16000000 || CLOCKLESS_FREQUENCY == 24000000) || defined(FASTLED_DOXYGEN) //  || CLOCKLESS_FREQUENCY == 48000000 || CLOCKLESS_FREQUENCY == 96000000) // 125ns/clock

/// Frequency multiplier for each clockless data interval.
/// @see Notes in @ref ClocklessChipsets
#define FMUL (CLOCKLESS_FREQUENCY/8000000)

/// GE8822 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GE8822Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER, 4> {};

/// LPD1886 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 3 * FMUL, 2 * FMUL, RGB_ORDER, 4> {};

/// LPD1886 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz_8bit : public ClocklessController<DATA_PIN, 2 * FMUL, 3 * FMUL, 2 * FMUL, RGB_ORDER> {};

/// WS2812 controller class @ 800 KHz.
/// @tparam DATA_PIN the data pin for these LEDs
/// @tparam RGB_ORDER the RGB ordering for these LEDs
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// WS2815 controller class @ 400 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2815Controller : public ClocklessController<DATA_PIN, 2 * FMUL, 9 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// WS2811 controller class @ 800 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 4 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// DP1903 controller class @ 800 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class DP1903Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 8 * FMUL, 2 * FMUL, RGB_ORDER> {};

/// DP1903 controller class @ 400 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class DP1903Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 16 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// WS2813 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>                                                             //not tested
class WS2813Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 4 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// WS2811 controller class @ 400 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 10 * FMUL, 6 * FMUL, RGB_ORDER> {};

/// SK6822 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6822Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 8 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// SM16703 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SM16703Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 4 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// SK6812 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6812Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 3 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// UCS1903 controller class @ 400 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, 4 * FMUL, 12 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// UCS1903B controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 4 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// UCS1904 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, 3 * FMUL, 3 * FMUL, 4 * FMUL, RGB_ORDER> {};

/// UCS2903 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS2903Controller : public ClocklessController<DATA_PIN, 2 * FMUL, 6 * FMUL, 2 * FMUL, RGB_ORDER> {};

/// TM1809 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER> {};

/// TM1803 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, 6 * FMUL, 9 * FMUL, 6 * FMUL, RGB_ORDER> {};

/// TM1829 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 5 * FMUL, 3 * FMUL, RGB_ORDER, 0, true, 500> {};

/// GW6205 controller class @ 400 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public ClocklessController<DATA_PIN, 6 * FMUL, 7 * FMUL, 6 * FMUL, RGB_ORDER, 4> {};

/// UCS1904 controller class @ 800 KHz.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public ClocklessController<DATA_PIN, 2 * FMUL, 4 * FMUL, 4 * FMUL, RGB_ORDER, 4> {};

/// PL9823 controller class.
/// @copydetails WS2812Controller800Khz
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PL9823Controller : public ClocklessController<DATA_PIN, 3 * FMUL, 8 * FMUL, 3 * FMUL, RGB_ORDER> {};

// UCS1912 - Note, never been tested, this is according to the datasheet
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1912Controller : public ClocklessController<DATA_PIN, 2 * FMUL, 8 * FMUL, 3 * FMUL, RGB_ORDER> {};


#else

// Allow overclocking of the clockless family of leds. 1.2 would be
// 20% overclocking. In tests WS2812 can be overclocked at 20%, but
// various manufacturers may be different.  This is a global value
// which is overridable by each supported chipset.
#ifdef FASTLED_LED_OVERCLOCK
#warning "FASTLED_LED_OVERCLOCK has been changed to FASTLED_OVERCLOCK. Please update your code."
#define FASTLED_OVERCLOCK FASTLED_LED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK
#define FASTLED_OVERCLOCK 1.0
#else
#ifndef FASTLED_OVERCLOCK_SUPPRESS_WARNING
#warning "FASTLED_OVERCLOCK is now active, #define FASTLED_OVERCLOCK_SUPPRESS_WARNING to disable this warning"
#endif
#endif

// WS2812 can be overclocked pretty aggressively, however, there are
// some excellent articles that you should read about WS2812 overclocking
// and corruption for a large number of LEDs.
// https://wp.josh.com/2014/05/16/why-you-should-give-your-neopixel-bits-room-to-breathe/
// https://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
#ifndef FASTLED_OVERCLOCK_WS2812
#define FASTLED_OVERCLOCK_WS2812 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2811
#define FASTLED_OVERCLOCK_WS2811 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2813
#define FASTLED_OVERCLOCK_WS2813 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2815
#define FASTLED_OVERCLOCK_WS2815 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_SK6822
#define FASTLED_OVERCLOCK_SK6822 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_SK6812
#define FASTLED_OVERCLOCK_SK6812 FASTLED_OVERCLOCK
#endif

/// Calculates the number of cycles for the clockless chipset (which may differ from CPU cycles)
/// @see ::NS()
#if FASTLED_CLOCKLESS_USES_NANOSECONDS
// just use raw nanosecond values for the teensy4
#define C_NS(_NS) _NS
#else
#define C_NS(_NS) (((_NS * ((CLOCKLESS_FREQUENCY / 1000000L)) + 999)) / 1000)
#endif

// Allow overclocking various LED chipsets in the clockless family.
// Clocked chips like the APA102 don't need this because they allow
// you to control the clock speed directly.
#define C_NS_WS2812(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_WS2812)))
#define C_NS_WS2811(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_WS2811)))
#define C_NS_WS2813(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_WS2813)))
#define C_NS_WS2815(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_WS2815)))
#define C_NS_SK6822(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_SK6822)))
#define C_NS_SK6812(_NS) (C_NS(int(_NS / FASTLED_OVERCLOCK_SK6812)))



// At T=0        : the line is raised hi to start a bit
// At T=T1       : the line is dropped low to transmit a zero bit
// At T=T1+T2    : the line is dropped low to transmit a one bit
// At T=T1+T2+T3 : the cycle is concluded (next bit can be sent)
//
// Python script to calculate the values for T1, T2, and T3 for FastLED:
// Note: there is a discussion on whether this python script is correct or not:
//  https://github.com/FastLED/FastLED/issues/1806
//
//  print("Enter the values of T0H, T0L, T1H, T1L, in nanoseconds: ")
//  T0H = int(input("  T0H: "))
//  T0L = int(input("  T0L: "))
//  T1H = int(input("  T1H: "))
//  T1L = int(input("  T1L: "))
//  
//  duration = max(T0H + T0L, T1H + T1L)
//  
//  print("The max duration of the signal is: ", duration)
//  
//  T1 = T0H
//  T2 = T1H
//  T3 = duration - T0H - T0L
//  
//  print("T1: ", T1)
//  print("T2: ", T2)
//  print("T3: ", T3)


// GE8822 - 350ns 660ns 350ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GE8822Controller800Khz : public ClocklessController<DATA_PIN, C_NS(350), C_NS(660), C_NS(350), RGB_ORDER, 4> {};

// GW6205@400khz - 800ns, 800ns, 800ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller400Khz : public ClocklessController<DATA_PIN, C_NS(800), C_NS(800), C_NS(800), RGB_ORDER, 4> {};

// GW6205@400khz - 400ns, 400ns, 400ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class GW6205Controller800Khz : public ClocklessController<DATA_PIN, C_NS(400), C_NS(400), C_NS(400), RGB_ORDER, 4> {};

// UCS1903 - 500ns, 1500ns, 500ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903Controller400Khz : public ClocklessController<DATA_PIN, C_NS(500), C_NS(1500), C_NS(500), RGB_ORDER> {};

// UCS1903B - 400ns, 450ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1903BController800Khz : public ClocklessController<DATA_PIN, C_NS(400), C_NS(450), C_NS(450), RGB_ORDER> {};

// UCS1904 - 400ns, 400ns, 450ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1904Controller800Khz : public ClocklessController<DATA_PIN, C_NS(400), C_NS(400), C_NS(450), RGB_ORDER> {};

// UCS2903 - 250ns, 750ns, 250ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS2903Controller : public ClocklessController<DATA_PIN, C_NS(250), C_NS(750), C_NS(250), RGB_ORDER> {};

// TM1809 - 350ns, 350ns, 550ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1809Controller800Khz : public ClocklessController<DATA_PIN, C_NS(350), C_NS(350), C_NS(450), RGB_ORDER> {};

// WS2811 - 320ns, 320ns, 640ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller800Khz : public ClocklessController<DATA_PIN, C_NS_WS2811(320), C_NS_WS2811(320), C_NS_WS2811(640), RGB_ORDER> {};

// WS2813 - 320ns, 320ns, 640ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2813Controller : public ClocklessController<DATA_PIN, C_NS_WS2813(320), C_NS_WS2813(320), C_NS_WS2813(640), RGB_ORDER> {};

#ifndef FASTLED_WS2812_T1
#define FASTLED_WS2812_T1 250
#endif

#ifndef FASTLED_WS2812_T2
#define FASTLED_WS2812_T2 625
#endif

#ifndef FASTLED_WS2812_T3
#define FASTLED_WS2812_T3 375
#endif


#if defined(__IMXRT1062__) && !defined(FASTLED_NOT_USES_OBJECTFLED)
#if defined(FASTLED_USES_OBJECTFLED)
#warning "FASTLED_USES_OBJECTFLED is now implicit for Teensy 4.0/4.1 for WS2812 and is no longer needed."
#endif
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812Controller800Khz:
	public fl::ClocklessController_ObjectFLED_WS2812<
		DATA_PIN,
		RGB_ORDER> {
 public:
    typedef fl::ClocklessController_ObjectFLED_WS2812<DATA_PIN, RGB_ORDER> Base;
	WS2812Controller800Khz(): Base(FASTLED_OVERCLOCK) {}
};
#elif defined(FASTLED_USES_ESP32S3_I2S)
#include "platforms/esp/32/clockless_i2s_esp32s3.h"
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812Controller800Khz:
	public fl::ClocklessController_I2S_Esp32_WS2812<
		DATA_PIN,
		RGB_ORDER
	> {};
#else
// WS2812 - 250ns, 625ns, 375ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2812Controller800Khz : public ClocklessController<
	DATA_PIN,
	C_NS_WS2812(FASTLED_WS2812_T1),
	C_NS_WS2812(FASTLED_WS2812_T2),
	C_NS_WS2812(FASTLED_WS2812_T3),
	RGB_ORDER> {};
#endif  // defined(FASTLED_USES_OBJECTFLED)


// WS2811@400khz - 800ns, 800ns, 900ns
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2811Controller400Khz : public ClocklessController<DATA_PIN, C_NS_WS2811(800), C_NS_WS2811(800), C_NS_WS2811(900), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2815Controller : public ClocklessController<DATA_PIN, C_NS_WS2815(250), C_NS_WS2815(1090), C_NS_WS2815(550), RGB_ORDER> {};

// 750NS, 750NS, 750NS
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1803Controller400Khz : public ClocklessController<DATA_PIN, C_NS(700), C_NS(1100), C_NS(700), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller800Khz : public ClocklessController<DATA_PIN, C_NS(340), C_NS(340), C_NS(550), RGB_ORDER, 0, true, 500> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class TM1829Controller1600Khz : public ClocklessController<DATA_PIN, C_NS(100), C_NS(300), C_NS(200), RGB_ORDER, 0, true, 500> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz : public ClocklessController<DATA_PIN, C_NS(200), C_NS(400), C_NS(200), RGB_ORDER, 4> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class LPD1886Controller1250Khz_8bit : public ClocklessController<DATA_PIN, C_NS(200), C_NS(400), C_NS(200), RGB_ORDER> {};


template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6822Controller : public ClocklessController<DATA_PIN, C_NS_SK6822(375), C_NS_SK6822(1000), C_NS_SK6822(375), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SK6812Controller : public ClocklessController<DATA_PIN, C_NS_SK6812(300), C_NS_SK6812(300), C_NS_SK6812(600), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class SM16703Controller : public ClocklessController<DATA_PIN, C_NS(300), C_NS(600), C_NS(300), RGB_ORDER> {};

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class PL9823Controller : public ClocklessController<DATA_PIN, C_NS(350), C_NS(1010), C_NS(350), RGB_ORDER> {};

// UCS1912 - Note, never been tested, this is according to the datasheet
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
class UCS1912Controller : public ClocklessController<DATA_PIN, C_NS(250), C_NS(1000), C_NS(350), RGB_ORDER> {};
#endif
/// @} ClocklessChipsets


// WS2816 - is an emulated controller that emits 48 bit pixels by forwarding
// them to a platform specific WS2812 controller.  The WS2812 controller
// has to output twice as many 24 bit pixels.
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB>
class WS2816Controller
    : public CPixelLEDController<RGB_ORDER, 
                                 WS2812Controller800Khz<DATA_PIN, RGB>::LANES_VALUE,
                                 WS2812Controller800Khz<DATA_PIN, RGB>::MASK_VALUE> {

public:

	// ControllerT is a helper class.  It subclasses the device controller class
	// and has three methods to call the three protected methods we use.
	// This is janky, but redeclaring public methods protected in a derived class
	// is janky, too.

    // N.B., byte order must be RGB.
	typedef WS2812Controller800Khz<DATA_PIN, RGB> ControllerBaseT;
	class ControllerT : public ControllerBaseT {
		friend class WS2816Controller<DATA_PIN, RGB_ORDER>;
		void *callBeginShowLeds(int size) { return ControllerBaseT::beginShowLeds(size); }
		void callShow(CRGB *data, int nLeds, uint8_t brightness) {
			ControllerBaseT::show(data, nLeds, brightness);
		}
		void callEndShowLeds(void *data) { ControllerBaseT::endShowLeds(data); }
	};

    static const int LANES = ControllerT::LANES_VALUE;
    static const uint32_t MASK = ControllerT::MASK_VALUE;

    WS2816Controller() {}
    ~WS2816Controller() {
        mController.setLeds(nullptr, 0);
        delete [] mData;
    }

    virtual void *beginShowLeds(int size) override {
        mController.setEnabled(true);
		void *result = mController.callBeginShowLeds(2 * size);
        mController.setEnabled(false);
        return result;
    }

    virtual void endShowLeds(void *data) override {
        mController.setEnabled(true);
		mController.callEndShowLeds(data);
        mController.setEnabled(false);
    }

    virtual void showPixels(PixelController<RGB_ORDER, LANES, MASK> &pixels) override {
        // Ensure buffer is large enough
        ensureBuffer(pixels.size());

		// expand and copy all the pixels
		size_t out_index = 0;
        while (pixels.has(1)) {
            pixels.stepDithering();

			uint16_t s0, s1, s2;
            pixels.loadAndScale_WS2816_HD(&s0, &s1, &s2);
			uint8_t b0_hi = s0 >> 8;
			uint8_t b0_lo = s0 & 0xFF;
			uint8_t b1_hi = s1 >> 8;
			uint8_t b1_lo = s1 & 0xFF;
			uint8_t b2_hi = s2 >> 8;
			uint8_t b2_lo = s2 & 0xFF;

			mData[out_index] = CRGB(b0_hi, b0_lo, b1_hi);
			mData[out_index + 1] = CRGB(b1_lo, b2_hi, b2_lo);

            pixels.advanceData();
			out_index += 2;
        }

		// ensure device controller won't modify color values
        mController.setCorrection(CRGB(255, 255, 255));
        mController.setTemperature(CRGB(255, 255, 255));
        mController.setDither(DISABLE_DITHER);

		// output the data stream
        mController.setEnabled(true);
#ifdef BOUNCE_SUBCLASS
		mController.callShow(mData, 2 * pixels.size(), 255);
#else
        mController.show(mData, 2 * pixels.size(), 255);
#endif
        mController.setEnabled(false);
    }

private:
    void init() override {
        mController.init();
        mController.setEnabled(false);
    }

    void ensureBuffer(int size_8bit) {
        int size_16bit = 2 * size_8bit;
        if (mController.size() != size_16bit) {
            delete [] mData;
            CRGB *new_leds = new CRGB[size_16bit];
			mData = new_leds;
            mController.setLeds(new_leds, size_16bit);
        }
    }

    CRGB *mData = 0;
    ControllerT mController;
};


#endif
/// @} Chipsets

FASTLED_NAMESPACE_END

#endif
