#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/noexcept.h"

// Forward declarations to avoid including heavy headers
namespace fl {
class Frame;
class Fx;
class AudioBatch;

FASTLED_SHARED_PTR(FxLayer);
class FxLayer {
  public:
    void setFx(fl::shared_ptr<Fx> newFx) FL_NO_EXCEPT;

    void draw(fl::u32 now, float speed = 1.0f, const AudioBatch *audio = nullptr) FL_NO_EXCEPT;

    void pause(fl::u32 now) FL_NO_EXCEPT;

    void release() FL_NO_EXCEPT;

    fl::shared_ptr<Fx> getFx() FL_NO_EXCEPT;

    fl::span<CRGB> getSurface() FL_NO_EXCEPT;

  private:
    fl::shared_ptr<Frame> frame;
    fl::shared_ptr<Fx> fx;
    fl::u32 mLastNow = 0;
    bool running = false;
};

} // namespace fl
