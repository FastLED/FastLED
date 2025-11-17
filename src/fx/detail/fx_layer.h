#pragma once

#include "ftl/stdint.h"
#include "crgb.h"
#include "ftl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "ftl/shared_ptr.h"  // For shared_ptr

// Forward declarations to avoid including heavy headers
namespace fl {
class Frame;
class Fx;

FASTLED_SHARED_PTR(FxLayer);
class FxLayer {
  public:
    void setFx(fl::shared_ptr<Fx> newFx);

    void draw(fl::u32 now);

    void pause(fl::u32 now);

    void release();

    fl::shared_ptr<Fx> getFx();

    CRGB *getSurface();

  private:
    fl::shared_ptr<Frame> frame;
    fl::shared_ptr<Fx> fx;
    bool running = false;
};

} // namespace fl
