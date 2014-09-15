#ifndef __INC_SMARTMATRIX_T3_H
#define __INC_SMARTMATRIX_T3_H

#ifdef SmartMatrix_h
#include<SmartMatrix.h>

extern SmartMatrix *pSmartMatrix;

// note - dmx simple must be included before FastSPI for this code to be enabled
class CSmartMatrixController : public CLEDController {
  SmartMatrix matrix;

public:
  // initialize the LED controller
  virtual void init() {
      // Initialize 32x32 LED Matrix
    matrix.begin();
    matrix.setBrightness(255);
    matrix.setColorCorrection(ccNone);

    // Clear screen
    clearLeds(0);
    matrix.swapBuffers();
    pSmartMatrix = &matrix;
  }

  // clear out/zero out the given number of leds.
  virtual void clearLeds(int nLeds) {
    const rgb24 black = {0,0,0};
    matrix.fillScreen(black);
    matrix.swapBuffers();
  }

  // set all the leds on the controller to a given color
  virtual void showColor(const struct CRGB & data, int nLeds,CRGB scale) {
    PixelController<RGB> pixels(data, nLeds, scale, getDither());
    rgb24 *md = matrix.backBuffer();
    while(nLeds--) {
      md->red = pixels.loadAndScale0();
      md->green = pixels.loadAndScale1();
      md->blue = pixels.loadAndScale2();
      md++;
      pixels.stepDithering();
    }
    matrix.swapBuffers();
  }

  // note that the uint8_ts will be in the order that you want them sent out to the device.
  // nLeds is the number of RGB leds being written to
  virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
    PixelController<RGB> pixels(data, nLeds, scale, getDither());
#ifdef SMART_MATRIX_CAN_TRIPLE_BUFFER
    rgb24 *md = matrix.getRealBackBuffer();
#else
    rgb24 *md = matrix.backBuffer();
#endif
    while(nLeds--) {
      md->red = pixels.loadAndScale0();
      md->green = pixels.loadAndScale1();
      md->blue = pixels.loadAndScale2();
      md++;
      pixels.advanceData();
      pixels.stepDithering();
    }
    matrix.swapBuffers();
#ifdef SMART_MATRIX_CAN_TRIPLE_BUFFER
    matrix.setBackBuffer((rgb24*)data);
#endif
  }

#ifdef SUPPORT_ARGB
  // as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
  virtual void show(const struct CARGB *data, int nLeds, CRGB scale) = 0;
#endif
};

#endif

#endif
