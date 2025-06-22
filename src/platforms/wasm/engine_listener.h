#pragma once

#include "fl/stdint.h"

#include "fl/engine_events.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN
class CLEDController;
FASTLED_NAMESPACE_END


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
