#pragma once

#include "fl/stl/stdint.h"

#include "fl/engine_events.h"
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
    static void Init();

  private:
    void onEndFrame() override;
    void onStripAdded(CLEDController *strip, uint32_t num_leds) override;
    void onCanvasUiSet(CLEDController *strip,
                       const fl::ScreenMap &screenmap) override;
    EngineListener();
    ~EngineListener();
};

} // namespace fl
