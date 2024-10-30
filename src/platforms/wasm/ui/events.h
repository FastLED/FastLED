#pragma once

#include <stdint.h>

#include "engine_events.h"
#include "namespace.h"



FASTLED_NAMESPACE_BEGIN

class CLEDController;
class ScreenMap;

class EngineListener: public EngineEvents::Listener {
public:
    friend class Singleton<EngineListener>;
    static void Init();

private:
    void onEndFrame() override;
    void onStripAdded(CLEDController* strip, uint32_t num_leds) override;
    void onCanvasUiSet(CLEDController* strip, const ScreenMap& screenmap) override;
    EngineListener();
    ~EngineListener();
};


FASTLED_NAMESPACE_END
