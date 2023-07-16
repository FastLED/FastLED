#ifndef __INC_OCTOWS2811_CONTROLLER_H
#define __INC_OCTOWS2811_CONTROLLER_H

#ifdef USE_OCTOWS2811

// #include "OctoWS2811.h"

FASTLED_NAMESPACE_BEGIN

template<EOrder RGB_ORDER = GRB, uint8_t CHIP = WS2811_800kHz>
class COctoWS2811Controller : public CPixelLEDController<RGB_ORDER, 8, 0xFF> {
  void *framebuffer;
  void *drawbuffer;
  bool wantNullDrawBuffer;
  OctoWS2811 *pocto = nullptr;

  bool tryToAllocate = true;

  void _init(int nLeds) {
    if (pocto == nullptr && tryToAllocate) {
      // Allocate the draw buffer second in case the malloc fails,
      // because it can technically be NULL. In other words,
      // prioritize allocating the frame buffer.
      if (framebuffer == nullptr) {
        framebuffer = malloc(nLeds * 8 * 3);
      }

      if (framebuffer != nullptr) {
        if (!wantNullDrawBuffer && drawbuffer == nullptr) {
          drawbuffer = malloc(nLeds * 8 * 3);
        }
        // Note: A NULL draw buffer is okay

        // byte ordering is handled in show by the pixel controller
        int config = WS2811_RGB;
        config |= CHIP;

        pocto = new OctoWS2811(nLeds, framebuffer, drawbuffer, config);

        if (pocto != nullptr) {
          pocto->begin();
        } else {
          // Don't try to allocate again
          tryToAllocate = false;
        }
      } else {
        // Don't try to allocate again
        tryToAllocate = false;
      }
    }
  }
public:
  // If the frame buffer isn't NULL then the draw buffer won't be
  // internally allocated.
  COctoWS2811Controller(void *framebuf = nullptr, void *drawbuf = nullptr)
      : framebuffer(framebuf),
        drawbuffer(drawbuf),
        wantNullDrawBuffer(framebuf != nullptr) {}

  virtual int size() { return CLEDController::size() * 8; }

  virtual void init() { /* do nothing yet */ }

  typedef union {
    uint8_t bytes[8];
    uint32_t raw[2];
  } Lines;

  virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> & pixels) {
    _init(pixels.size());
    if (pocto == nullptr) {
      return;
    }

    uint8_t *pData = drawbuffer;
    while(pixels.has(1)) {
      Lines b;

      for(int i = 0; i < 8; ++i) { b.bytes[i] = pixels.loadAndScale0(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; ++i) { b.bytes[i] = pixels.loadAndScale1(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; ++i) { b.bytes[i] = pixels.loadAndScale2(i); }
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
