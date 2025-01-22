#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/namespace.h"

namespace fl {
class ScreenMap;
}

FASTLED_NAMESPACE_BEGIN

class CLEDController;


class EngineListener: public fl::EngineEvents::Listener {
public:
    friend class fl::Singleton<EngineListener>;
    static void Init();

private:
    void onEndFrame() override;
    void onStripAdded(CLEDController* strip, uint32_t num_leds) override;
    void onCanvasUiSet(CLEDController* strip, const fl::ScreenMap& screenmap) override;
    EngineListener();
    ~EngineListener();
};


FASTLED_NAMESPACE_END
