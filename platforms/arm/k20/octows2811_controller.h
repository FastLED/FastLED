#ifndef __INC_OCTOWS2811_CONTROLLER_H
#define __INC_OCTOWS2811_CONTROLLER_H

#ifdef USE_OCTOWS2811

// #include "OctoWS2811.h"

FASTLED_NAMESPACE_BEGIN

template<EOrder RGB_ORDER = GRB, boolean SLOW=false>
class COctoWS2811Controller : public CLEDController {
  OctoWS2811  *pocto;
  uint8_t *drawbuffer,*framebuffer;

  void _init(int nLeds) {
    if(pocto == NULL) {
      drawbuffer = (uint8_t*)malloc(nLeds * 8 * 3);
      framebuffer = (uint8_t*)malloc(nLeds * 8 * 3);

      // byte ordering is handled in show by the pixel controller
      int config = WS2811_RGB;
      if(SLOW) {
        config |= WS2811_400kHz;
      }

      pocto = new OctoWS2811(nLeds, framebuffer, drawbuffer, config);

      pocto->begin();
    }
  }
public:
  COctoWS2811Controller() { pocto = NULL; }


  virtual void init() { /* do nothing yet */ }

  virtual void clearLeds(int nLeds) {
    _init(nLeds);
    showColor(CRGB(0,0,0),nLeds,CRGB(0,0,0));
  }

  virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
    _init(nLeds);
    // Get our pixel values
    PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither());
    uint8_t ball[3][8];
    memset(ball[0],pixels.loadAndScale0(),8);
    memset(ball[1],pixels.loadAndScale1(),8);
    memset(ball[2],pixels.loadAndScale2(),8);

    uint8_t bout[24];
    transpose8x1_MSB(ball[0],bout);
    transpose8x1_MSB(ball[1],bout+8);
    transpose8x1_MSB(ball[2],bout+16);

    uint8_t *pdata = drawbuffer;
    while(nLeds--) {
      memcpy(pdata,bout,24);
      pdata += 24;
    }

    pocto->show();
  }

  typedef union {
    uint8_t bytes[8];
    uint32_t raw[2];
  } Lines;

  virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    _init(nLeds);
    MultiPixelController<8,0xFF,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );

    uint8_t *pData = drawbuffer;
    while(nLeds--) {
      Lines b;

      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale0(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale1(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale2(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      pixels.stepDithering();
      pixels.advanceData();
    }

    pocto->show();
  }
};

FASTLED_NAMESPACE_END

#endif

#endif
