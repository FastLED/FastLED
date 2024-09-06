#ifndef __INC_CLOCKLESS_BLOCK_ESP8266_H
#define __INC_CLOCKLESS_BLOCK_ESP8266_H

#include <stdint.h>
#include "namespace.h"
#include "clock_cycles.h"
#include "esp_intr_alloc.h"
#include "eorder.h"
#include "transpose8x1_noinline.h"
#include "force_inline.h"

#define FASTLED_HAS_BLOCKLESS 1

#define PORT_MASK (((1<<LANES)-1) & 0x0000FFFFL)
#define MIN(X,Y) (((X)<(Y)) ? (X):(Y))
#define USED_LANES (MIN(LANES,4))
#define REAL_FIRST_PIN 12
#define LAST_PIN (12 + USED_LANES - 1)

// These are completely made up but allow the code to compile.
// It looks like esp32 has a more flexible pin mapping than esp8266.
// so these might actually work.
#define PORTD_FIRST_PIN 12
#define PORTA_FIRST_PIN 14
#define PORTB_FIRST_PIN 16

FASTLED_NAMESPACE_BEGIN

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern uint32_t _frame_cnt;
extern uint32_t _retry_cnt;
#endif

FASTLED_FORCE_INLINE void interrupt_unlock() {
	// ets_intr_unlock();
	// TODO: imlement interrupt_unlock?
	// These functions were mined out of the code below and
	// made no-ops here. We probably want to implement them.
}

FASTLED_FORCE_INLINE void interrupt_lock()  {
	// ets_intr_lock();
	// TODO: imlement interrupt_lock?
}

template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
    typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<FIRST_PIN>::port_t data_t;

	// Verify that the pin is valid
	static_assert(FastPin<FIRST_PIN>::validpin(), "Invalid pin specified");

	data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

public:
    virtual int size() { return CLEDController::size() * LANES; }

    virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) {
	// mWait.wait();
	/*uint32_t clocks = */
	int cnt=FASTLED_INTERRUPT_RETRY_COUNT;
	while(!showRGBInternal(pixels) && cnt--) {
	    // ets_intr_unlock();
		interrupt_unlock();
#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
	    ++_retry_cnt;
#endif
	    delayMicroseconds(WAIT_TIME * 10);
	    // ets_intr_lock();
		interrupt_lock();
	}
	// #if FASTLED_ALLOW_INTTERUPTS == 0
	// Adjust the timer
	// long microsTaken = CLKS_TO_MICROS(clocks);
	// MS_COUNTER += (1 + (microsTaken / 1000));
	// #endif
	
	// mWait.mark();
    }

    template<int PIN> static void initPin() {
	if(PIN >= REAL_FIRST_PIN && PIN <= LAST_PIN) {
	    _ESPPIN<PIN, 1<<(PIN & 0xFF), true>::setOutput();
	    // FastPin<PIN>::setOutput();
	}
    }

    virtual void init() {
	// Only supportd on pins 12-15
        // SZG: This probably won't work (check pins definitions in fastpin_esp32)
	initPin<12>();
	initPin<13>();
	initPin<14>();
	initPin<15>();
	mPinMask = FastPin<FIRST_PIN>::mask();
	mPort = FastPin<FIRST_PIN>::port();
	
	// Serial.print("Mask is "); Serial.println(PORT_MASK);
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }
    
    typedef union {
	uint8_t bytes[8];
	uint16_t shorts[4];
	uint32_t raw[2];
    } Lines;

#define ESP_ADJUST 0 // (2*(F_CPU/24000000))
#define ESP_ADJUST2 0
    template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(FASTLED_REGISTER uint32_t & last_mark, FASTLED_REGISTER Lines & b, PixelController<RGB_ORDER, LANES, PORT_MASK> &pixels) { // , FASTLED_REGISTER uint32_t & b2)  {
	Lines b2 = b;
	transpose8x1_noinline(b.bytes,b2.bytes);
	
	FASTLED_REGISTER uint8_t d = pixels.template getd<PX>(pixels);
	FASTLED_REGISTER uint8_t scale = pixels.template getscale<PX>(pixels);
	
	for(FASTLED_REGISTER uint32_t i = 0; i < USED_LANES; ++i) {
	    while((__clock_cycles() - last_mark) < (T1+T2+T3));
	    last_mark = __clock_cycles();
	    *FastPin<FIRST_PIN>::sport() = PORT_MASK << REAL_FIRST_PIN;
	    
	    uint32_t nword = ((uint32_t)(~b2.bytes[7-i]) & PORT_MASK) << REAL_FIRST_PIN;
	    while((__clock_cycles() - last_mark) < (T1-6));
	    *FastPin<FIRST_PIN>::cport() = nword;
	    
	    while((__clock_cycles() - last_mark) < (T1+T2));
	    *FastPin<FIRST_PIN>::cport() = PORT_MASK << REAL_FIRST_PIN;
	    
	    b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
	}

	for(FASTLED_REGISTER uint32_t i = USED_LANES; i < 8; ++i) {
	    while((__clock_cycles() - last_mark) < (T1+T2+T3));
	    last_mark = __clock_cycles();
	    *FastPin<FIRST_PIN>::sport() = PORT_MASK << REAL_FIRST_PIN;
	    
	    uint32_t nword = ((uint32_t)(~b2.bytes[7-i]) & PORT_MASK) << REAL_FIRST_PIN;
	    while((__clock_cycles() - last_mark) < (T1-6));
	    *FastPin<FIRST_PIN>::cport() = nword;
	    
	    while((__clock_cycles() - last_mark) < (T1+T2));
	    *FastPin<FIRST_PIN>::cport() = PORT_MASK << REAL_FIRST_PIN;
	}
    }

    // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
    // gcc will use register Y for the this pointer.
    static uint32_t showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {
	
	// Setup the pixel controller and load/scale the first byte
	Lines b0;
	
	for(int i = 0; i < USED_LANES; ++i) {
	    b0.bytes[i] = allpixels.loadAndScale0(i);
	}
	allpixels.preStepFirstByteDithering();
	
	// ets_intr_lock();
	interrupt_lock();
	uint32_t _start = __clock_cycles();
	uint32_t last_mark = _start;
	
	while(allpixels.has(1)) {
	    // Write first byte, read next byte
	    writeBits<8+XTRA0,1>(last_mark, b0, allpixels);
	    
	    // Write second byte, read 3rd byte
	    writeBits<8+XTRA0,2>(last_mark, b0, allpixels);
	    allpixels.advanceData();
	    
	    // Write third byte
	    writeBits<8+XTRA0,0>(last_mark, b0, allpixels);
	    
#if (FASTLED_ALLOW_INTERRUPTS == 1)
	    // ets_intr_unlock();
		interrupt_unlock();
#endif
	    
	    allpixels.stepDithering();
	    
#if (FASTLED_ALLOW_INTERRUPTS == 1)
	    // ets_intr_lock();
		interrupt_lock();
	    // if interrupts took longer than 45µs, punt on the current frame
	    if((int32_t)(__clock_cycles()-last_mark) > 0) {
		if((int32_t)(__clock_cycles()-last_mark) > (T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) { interrupt_unlock(); return 0; }
	    }
#endif
	};
	
	// ets_intr_unlock();
	interrupt_unlock();
#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
	++_frame_cnt;
#endif
	return __clock_cycles() - _start;
    }
};

FASTLED_NAMESPACE_END
#endif
