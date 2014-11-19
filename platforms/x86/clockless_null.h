#ifndef __CLOCKLESS_NULL_H
#define __CLOCKLESS_NULL_H


template <int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CLEDController {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {

	}

// #define ADV_RGB
#define ADV_RGB if(maskbit & PORT_MASK) { rgbdata += nLeds; } maskbit <<= 1;

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
	  // add display here
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
	  // Add display here
	}
#endif

	//	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, Lines & b3, MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &pixels) { }

};

#endif //
