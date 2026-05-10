// audio_reactive.h - Wire AudioProcessor / VibeDetector to FxEngine speed
#pragma once

#include "FastLED.h"
#include "fl/audio/audio_processor.h"
#include "fl/fx/fx_engine.h"
#include "fl/ui/ui.h"

struct AudioReactive {
    fl::shared_ptr<fl::audio::Processor> processor;
    bool autoPump = false;
    uint32_t sampleCount = 0;

    // Create the AudioProcessor, routing through FastLED.add() when possible.
    void begin(fl::UIAudio &audio);

    // Connect vibe levels to FxEngine speed.  Call once after begin().
    void connectToEngine(fl::FxEngine &fxEngine, fl::UICheckbox &enableVibe,
                         fl::UISlider &speedMultiplier,
                         fl::UISlider &baseSpeed, fl::UISlider &timeSpeed);

    // Pump audio manually when auto-pump is unavailable.
    void pump(fl::UIAudio &audio, fl::UICheckbox &enableVibe);
};
