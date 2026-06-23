// IWYU pragma: private

#ifndef __INC_OCTOWS2811_CONTROLLER_H
#define __INC_OCTOWS2811_CONTROLLER_H

#ifdef USE_OCTOWS2811

// IWYU pragma: begin_keep
#include <OctoWS2811.h>
// IWYU pragma: end_keep
#include "fl/stl/allocator.h"
#include "fl/stl/noexcept.h"
namespace fl {
template<EOrder RGB_ORDER = GRB, u8 CHIP = WS2811_800kHz>
class COctoWS2811Controller : public CPixelLEDController<RGB_ORDER, 8, 0xFF> {
  OctoWS2811  *pocto;
  u8 *drawbuffer,*framebuffer;

  void _init(int nLeds) FL_NOEXCEPT {
    if(pocto == nullptr) {
      drawbuffer = (u8*)fl::malloc(nLeds * 8 * 3);
      framebuffer = (u8*)fl::malloc(nLeds * 8 * 3);

      // byte ordering is handled in show by the pixel controller
      int config = WS2811_RGB;
      config |= CHIP;

      pocto = new OctoWS2811(nLeds, framebuffer, drawbuffer, config);  // ok bare allocation

      pocto->begin();
    }
  }
public:
  COctoWS2811Controller() { pocto = nullptr; }
  virtual int size() { return CLEDController::size() * 8; }

  virtual void init() { /* do nothing yet */ }

  virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> &pixels) FL_NOEXCEPT {
    u32 size = pixels.size();
    u32 sizeTimes8 = 8U * size;
    _init(size);

    u32 index = 0;
    while (pixels.has(1)) {
      for (int lane = 0; lane < 8; lane++) {
        u8 r = pixels.loadAndScale0(lane);
        u8 g = pixels.loadAndScale1(lane);
        u8 b = pixels.loadAndScale2(lane);
        pocto->setPixel(index, r, g, b);
        index += size;
      }
      index -= sizeTimes8;
      index++;
      pixels.stepDithering();
      pixels.advanceData();
    }

    pocto->show();
  }

};
}  // namespace fl
#endif

#endif
