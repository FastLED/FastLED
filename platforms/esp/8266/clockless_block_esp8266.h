#ifndef __INC_CLOCKLESS_BLOCK_ESP8266_H
#define __INC_CLOCKLESS_BLOCK_ESP8266_H

#define FASTLED_HAS_BLOCKLESS 1
#define PORTA_FIRST_PIN 0

#define PORT_MASK (((1<<LANES)-1) & 0xFFFF)
#define MIN(X,Y) (((X)<(Y)) ? (X):(Y))
#define USED_LANES (MIN(LANES,16))
#define LAST_PIN (FIRST_PIN + USED_LANES - 1)

FASTLED_NAMESPACE_BEGIN

template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 40>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual int size() { return CLEDController::size() * LANES; }

	virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) {
		mWait.wait();
		uint32_t clocks = showRGBInternal(pixels);
		// #if FASTLED_ALLOW_INTTERUPTS == 0
		// Adjust the timer
		// long microsTaken = CLKS_TO_MICROS(clocks);
		// MS_COUNTER += (1 + (microsTaken / 1000));
		// #endif

		mWait.mark();
	}

  template<int PIN> static void initPin() {
    if(PIN >= FIRST_PIN && PIN <= LAST_PIN) {
      FastPin<PIN>::setOutput();
    }
  }

  virtual void init() {
    initPin<0>();
    initPin<1>();
    initPin<2>();
    initPin<3>();
    initPin<4>();
    initPin<5>();
    initPin<6>();
    initPin<7>();
    initPin<8>();
    initPin<9>();
    initPin<10>();
    initPin<11>();
    initPin<12>();
    initPin<13>();
    initPin<14>();
    initPin<15>();
    mPinMask = FastPin<FIRST_PIN>::mask();
    mPort = FastPin<FIRST_PIN>::port();
  }

  virtual uint16_t getMaxRefreshRate() const { return 400; }

	typedef union {
		uint8_t bytes[12];
		uint16_t shorts[6];
		uint32_t raw[3];
	} Lines;

#define ESP_ADJUST (2*(F_CPU/24000000))
  template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, PixelController<RGB_ORDER, LANES, PORT_MASK> &pixels) { // , register uint32_t & b2)  {
		register Lines b2;
		if(USED_LANES>8) {
			transpose8<1,2>(b.bytes,b2.bytes);
			transpose8<1,2>(b.bytes+8,b2.bytes+1);
		} else {
			transpose8x1(b.bytes,b2.bytes);
		}
		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; i < (USED_LANES/2); i++) {
			while(__clock_cycles() < next_mark);
			next_mark = __clock_cycles() + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;

			while((next_mark - __clock_cycles()) > (T2+T3+ESP_ADJUST));
			if(USED_LANES>8) {
				*FastPin<FIRST_PIN>::cport() = ((~b2.shorts[i]) & PORT_MASK) << FIRST_PIN;
			} else {
				*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK) << FIRST_PIN;
			}

			while((next_mark - __clock_cycles()) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
			b.bytes[i+(USED_LANES/2)] = pixels.template loadAndScale<PX>(pixels,i+(USED_LANES/2),d,scale);
		}

		// if folks use an odd numnber of lanes, get the last byte's value here
		if(USED_LANES & 0x01) {
			b.bytes[USED_LANES-1] = pixels.template loadAndScale<PX>(pixels,USED_LANES-1,d,scale);
		}

		for(register uint32_t i = USED_LANES/2; i < 8; i++) {
			while(__clock_cycles() < next_mark);
			next_mark = __clock_cycles() + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;
			while((next_mark - __clock_cycles()) > (T2+T3+ESP_ADJUST));
			if(USED_LANES>8) {
				*FastPin<FIRST_PIN>::cport() = ((~b2.shorts[i]) & PORT_MASK) << FIRST_PIN;
			} else {
				// b2.bytes[0] = 0;
				*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK) << FIRST_PIN;
			}

			while((next_mark - __clock_cycles()) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

		}
	}

  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
		static uint32_t showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {
		// Get access to the clock
    uint32_t _start = __clock_cycles();

		// Setup the pixel controller and load/scale the first byte
		allpixels.preStepFirstByteDithering();
		register Lines b0;

		allpixels.preStepFirstByteDithering();
		for(int i = 0; i < USED_LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		os_intr_lock();
		uint32_t next_mark = __clock_cycles() + (T1+T2+T3);

		while(allpixels.has(1)) {
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
      os_intr_lock();
			// if interrupts took longer than 45µs, punt on the current frame
			if(__clock_cycles() > next_mark) {
				if((__clock_cycles()-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return __clock_cycles(); }
			}
			#endif
			allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, allpixels);
      #if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_unlock();
			#endif
		};

    os_intr_unlock();
    return __clock_cycles() - _start;
	}
};

FASTLED_NAMESPACE_END
#endif
