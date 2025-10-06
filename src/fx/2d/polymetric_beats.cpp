/*  FastLED - Polymetric Beat Visualization (Implementation)
    ---------------------------------------------------------
    Audio-reactive particle system for polymetric rhythm detection.

    License: MIT (same spirit as FastLED)
*/

#include "polymetric_beats.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/math.h"

namespace fl {

PolymetricBeats::PolymetricBeats(const XYMap& xyMap, const PolymetricBeatsConfig& cfg)
    : Fx2d(xyMap)
    , _cfg(cfg)
    , _beatDetector(cfg.beat_cfg)
    , _particles(cfg.particle_cfg)
    , _lastDrawTime(0)
    , _shouldClear(false)
{
    // Update particle system dimensions to match XYMap
    RhythmParticlesConfig particle_cfg = _particles.config();
    particle_cfg.width = xyMap.getWidth();
    particle_cfg.height = xyMap.getHeight();
    _particles.setConfig(particle_cfg);

    // Wire callbacks
    wireCallbacks();
}

void PolymetricBeats::wireCallbacks() {
    // Wire beat detector callbacks to particle system

    // Onset callbacks
    _beatDetector.onOnsetBass = [this](float confidence, float timestamp_ms) {
        _particles.onOnsetBass(confidence, timestamp_ms);
    };

    _beatDetector.onOnsetMid = [this](float confidence, float timestamp_ms) {
        _particles.onOnsetMid(confidence, timestamp_ms);
    };

    _beatDetector.onOnsetHigh = [this](float confidence, float timestamp_ms) {
        _particles.onOnsetHigh(confidence, timestamp_ms);
    };

    // Beat callback
    _beatDetector.onBeat = [this](float /* confidence */, float /* bpm */, float /* timestamp_ms */) {
        if (_cfg.clear_on_beat) {
            _shouldClear = true;
        }
    };

    // Polymetric beat callback
    _beatDetector.onPolymetricBeat = [this](float phase4_4, float phase7_8) {
        _particles.onBeat(phase4_4, phase7_8);
    };

    // Subdivision callback
    _beatDetector.onSubdivision = [this](SubdivisionType subdiv, float swing_offset) {
        _particles.onSubdivision(static_cast<int>(subdiv), swing_offset);
    };

    // Fill callback
    _beatDetector.onFill = [this](bool starting, float density) {
        _particles.onFill(starting, density);
    };
}

void PolymetricBeats::processAudio(const float* samples, int n) {
    // Process audio through beat detector
    _beatDetector.processFrame(samples, n);
}

void PolymetricBeats::draw(DrawContext context) {
    // Get delta time
    uint32_t now = context.now;
    float dt = 0.016f;  // Default ~60 FPS
    if (_lastDrawTime > 0) {
        dt = (now - _lastDrawTime) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;  // Cap at 100ms to avoid huge jumps
    }
    _lastDrawTime = now;

    CRGB* leds = context.leds;
    int num_leds = mNumLeds;

    // Apply background fade (or clear if requested)
    if (_shouldClear) {
        for (int i = 0; i < num_leds; i++) {
            leds[i] = CRGB::Black;
        }
        _shouldClear = false;
    } else if (_cfg.background_fade < 255) {
        for (int i = 0; i < num_leds; i++) {
            leds[i].nscale8(_cfg.background_fade);
        }
    }

    // Update particle physics
    _particles.update(dt);

    // Render particles to LED buffer
    _particles.render(leds, num_leds);
}

void PolymetricBeats::setConfig(const PolymetricBeatsConfig& cfg) {
    bool detector_changed = false;
    bool particles_changed = false;

    // Check what changed
    if (&cfg.beat_cfg != &_cfg.beat_cfg) {
        detector_changed = true;
    }
    if (&cfg.particle_cfg != &_cfg.particle_cfg) {
        particles_changed = true;
    }

    _cfg = cfg;

    if (detector_changed) {
        _beatDetector.setConfig(cfg.beat_cfg);
    }

    if (particles_changed) {
        _particles.setConfig(cfg.particle_cfg);
    }

    // Re-wire callbacks in case configuration invalidated them
    wireCallbacks();
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
