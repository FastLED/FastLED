#ifndef __INC_CLOCKLESS_ARM_K26
#define __INC_CLOCKLESS_ARM_K26

#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))

#define SCALE(S,V) scale8_video(S,V)
// #define SCALE(S,V) scale8(S,V)
FASTLED_NAMESPACE_BEGIN
#define FASTLED_HAS_CLOCKLESS 1

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CLEDController {
  typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
  typedef typename FastPinBB<DATA_PIN>::port_t data_t;

  data_t mPinMask;
  data_ptr_t mPort;
  CMinWait<WAIT_TIME> mWait;
public:
  virtual void init() {
    FastPinBB<DATA_PIN>::setOutput();
    mPinMask = FastPinBB<DATA_PIN>::mask();
    mPort = FastPinBB<DATA_PIN>::port();
  }

  virtual void clearLeds(int nLeds) {
    showColor(CRGB(0, 0, 0), nLeds, 0);
  }

  // set all the leds on the controller to a given color
  virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();

    showRGBInternal(pixels);

    sei();
    mWait.mark();
  }

  virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();

    showRGBInternal(pixels);

    sei();
    mWait.mark();
  }

#ifdef SUPPORT_ARGB
  virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();
    showRGBInternal(pixels);
    sei();
    mWait.mark();
  }
#endif

#define DT1(ADJ) delaycycles<T1-(ADJ+8)>();
#define DT2(ADJ) delaycycles<T2-(ADJ+2)>();
#define DT3(ADJ) delaycycles<((T3-ADJ))>();

#define HI2 FastPin<DATA_PIN>::hi();
#define LO2 FastPin<DATA_PIN>::lo();
#define BC2A(B) if(b&0x80) { FastPin<DATA_PIN>::lo(); }
#define BC2(B) if(b&0x80) { FastPin<DATA_PIN>::lo(); }
// #define BC2(BIT) if(b&0x80) {} else { pGpio->OUTCLR = (1<<DATA_PIN); }
// #define BC2A(BIT) if(b&0x80) {} else { delaycycles<2>(); pGpio->OUTCLR = (1<<DATA_PIN); }
#define LSB1 b <<= 1;

// dither adjustment macro - should be kept in sync w/what's in stepDithering
#define ADJDITHER2(D, E) D = E - D;


// #define CLI_CHK cli(); if(NRF_RTC0->COUNTER > clk_max) { return 0; }
// #define SEI_CHK clk_max = NRF_RTC0->COUNTER + 1; sei();
#define CLI_CHK
#define SEI_CHK

// don't allow more than 800 clocks (50µs) between leds
// #define SEI_CHK LED_TIMER->CC[0] = (WAIT_TIME * (F_CPU/1000000)); LED_TIMER->TASKS_CLEAR; LED_TIMER->EVENTS_COMPARE[0] = 0;
// #define CLI_CHK cli(); if(LED_TIMER->EVENTS_COMPARE[0]) { LED_TIMER->TASKS_STOP = 1; return 0; }

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
  // gcc will use register Y for the this pointer.
  static uint32_t showRGBInternal(PixelController<RGB_ORDER> & pixels) {
    // Setup the pixel controller and load/scale the first byte
    // pixels.preStepFirstByteDithering();
    CRGB scale = pixels.mScale;
    register const uint8_t *pdata = pixels.mData;

    uint8_t d0 = pixels.d[RO(0)];
    uint8_t d1 = pixels.d[RO(1)];
    uint8_t d2 = pixels.d[RO(2)];
    uint8_t e0 = pixels.e[RO(0)];
    uint8_t e1 = pixels.e[RO(1)];
    uint8_t e2 = pixels.e[RO(2)];

#define RAN 0
#define RAN2 0
    register uint32_t b = ~scale8(pdata[RO(0)], scale.raw[RO(0)]);
    ADJDITHER2(d0,e0);

    register uint32_t b2;
    int len = pixels.mLen;

    SEI_CHK;
    while(len >= 1) {
      ADJDITHER2(d0,e0);
      ADJDITHER2(d1,e1);
      ADJDITHER2(d2,e2);

      HI2; DT1(4); BC2A(7); DT2(2); LO2; LSB1; DT3(2);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(4); b2 = pdata[RO(1)];
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = b2 ? qadd8(b2, d1) : 0;
      HI2; DT1(4); BC2(0); DT2(2); LO2; DT3(4);
      b = ~scale8(b2, scale.raw[RO(1)]);
      len--;

      HI2; DT1(4); BC2(7); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(4); b2 = pdata[RO(2)];
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = b2 ? qadd8(b2, d2) : 0;
      HI2; DT1(4); BC2(0); DT2(2); LO2; DT3(4);

      b = ~scale8(b2, scale.raw[RO(2)]);

      HI2; DT1(4); BC2(7); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(4); pdata += 3;
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(4); b2 = pdata[RO(0)];
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = b2 ? qadd8(b2, d0) : 0;
      HI2; DT1(4); BC2(0); DT2(2); LO2; SEI_CHK; sei(); DT3(9);

      b = ~scale8(b2, scale.raw[RO(0)]);
      CLI_CHK;
      delaycycles<RAN2>();
    };


    return 0; // 0x00FFFFFF - _VAL;
  }


};

FASTLED_NAMESPACE_END


#endif // __INC_CLOCKLESS_ARM_D21
