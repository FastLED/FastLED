#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"

#include "fl/system/engine_events.h"
#include "fl/stl/noexcept.h"
namespace fl {
class CLEDController;
}  // namespace fl
namespace fl {
class ScreenMap;
}

namespace fl {

class EngineListener : public fl::EngineEvents::Listener {
  public:
    friend class fl::Singleton<EngineListener>;
    static void Init() FL_NO_EXCEPT;

  private:
    void onEndFrame() FL_NO_EXCEPT override;
    void onStripAdded(CLEDController *strip, u32 num_leds) FL_NO_EXCEPT override;
    void onCanvasUiSet(CLEDController *strip,
                       const fl::ScreenMap &screenmap) override FL_NO_EXCEPT;
    EngineListener() FL_NO_EXCEPT;
    ~EngineListener();
};

} // namespace fl
