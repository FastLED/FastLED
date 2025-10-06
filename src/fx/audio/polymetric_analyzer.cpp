/*  FastLED - Polymetric Rhythm Analysis (Implementation)
    ---------------------------------------------------------
    Analyzes polymetric overlay patterns (e.g., 7/8 over 4/4)
    for complex EDM rhythm detection.

    License: MIT (same spirit as FastLED)
*/

#include "polymetric_analyzer.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/math.h"
#include <math.h>

namespace fl {

PolymetricAnalyzer::PolymetricAnalyzer(const PolymetricConfig& cfg)
    : _cfg(cfg)
    , _phase4_4(0.0f)
    , _phaseOverlay(0.0f)
    , _phase16th(0.0f)
    , _currentBPM(120.0f)
    , _lastBeatTime(0.0f)
    , _beatPeriodMs(500.0f)  // 120 BPM default
    , _inFill(false)
    , _fillDensity(0.0f)
    , _fillStartTime(0.0f)
    , _lastPhase16th(0.0f)
{
}

void PolymetricAnalyzer::reset() {
    _phase4_4 = 0.0f;
    _phaseOverlay = 0.0f;
    _phase16th = 0.0f;
    _lastBeatTime = 0.0f;
    _inFill = false;
    _fillDensity = 0.0f;
    _fillStartTime = 0.0f;
    _lastPhase16th = 0.0f;
}

void PolymetricAnalyzer::setConfig(const PolymetricConfig& cfg) {
    _cfg = cfg;
}

void PolymetricAnalyzer::onBeat(float bpm, float timestamp_ms) {
    if (!_cfg.enable) {
        return;
    }

    _currentBPM = bpm;
    _beatPeriodMs = (60.0f * 1000.0f) / bpm;
    _lastBeatTime = timestamp_ms;

    // Reset 4/4 phase on beat
    _phase4_4 = 0.0f;

    // Update overlay phase
    // For 7/8 over 2 bars of 4/4: overlay completes every 2 bars (8 beats)
    // 7 pulses in the overlay cycle
    float beats_per_overlay_cycle = _cfg.overlay_bars * 4.0f;  // 2 bars * 4 beats = 8 beats
    float overlay_increment = _cfg.overlay_numerator / beats_per_overlay_cycle;  // 7/8 = 0.875 per beat

    _phaseOverlay += overlay_increment;
    if (_phaseOverlay >= 1.0f) {
        _phaseOverlay -= 1.0f;
    }

    // Trigger polymetric beat callback
    if (onPolymetricBeat) {
        onPolymetricBeat(_phase4_4, _phaseOverlay);
    }
}

void PolymetricAnalyzer::update(float timestamp_ms) {
    if (!_cfg.enable) {
        return;
    }

    updatePhases(timestamp_ms);
    detectSubdivisions(timestamp_ms);
    detectFills(timestamp_ms);
}

void PolymetricAnalyzer::updatePhases(float timestamp_ms) {
    if (_beatPeriodMs <= 0.0f) {
        return;
    }

    // Calculate time since last beat
    float time_since_beat = timestamp_ms - _lastBeatTime;

    // Update 4/4 phase
    _phase4_4 = time_since_beat / _beatPeriodMs;
    if (_phase4_4 >= 1.0f) {
        _phase4_4 = 0.0f;  // Reset will happen on next beat
    }

    // Update 16th note phase (4 16ths per beat)
    _phase16th = (_phase4_4 * 4.0f);
    _phase16th -= ::floor(_phase16th);
}

void PolymetricAnalyzer::detectSubdivisions(float /* timestamp_ms */) {
    // Detect 16th note crossings
    if (_phase16th < _lastPhase16th) {
        // Crossed a 16th note boundary
        float swing_offset = getSwingOffset();

        if (onSubdivision) {
            onSubdivision(SubdivisionType::Sixteenth, swing_offset);
        }
    }

    _lastPhase16th = _phase16th;
}

void PolymetricAnalyzer::detectFills(float /* timestamp_ms */) {
    // Simple fill detection based on overlay phase alignment
    // When 7/8 overlay is out of phase with 4/4, it creates tension
    // When they realign, it creates resolution

    float phase_diff = ABS(_phase4_4 - _phaseOverlay);

    // High phase difference = potential fill
    if (phase_diff > 0.6f && !_inFill) {
        _inFill = true;
        _fillDensity = phase_diff;
        if (onFill) {
            onFill(true, _fillDensity);
        }
    } else if (phase_diff < 0.2f && _inFill) {
        _inFill = false;
        if (onFill) {
            onFill(false, 0.0f);
        }
    }
}

float PolymetricAnalyzer::getSwingOffset() const {
    return calculateSwingOffset(_phase16th);
}

float PolymetricAnalyzer::calculateSwingOffset(float phase) const {
    if (_cfg.swing_amount <= 0.0f) {
        return 0.0f;
    }

    // Apply swing to even 16th notes (push them later)
    // Swing is applied every other 16th note
    int subdivision_index = static_cast<int>(phase * 4.0f) % 2;

    if (subdivision_index == 1) {
        // Odd 16th notes get delayed (swing)
        return _cfg.swing_amount;
    } else {
        // Even 16th notes stay on time
        return 0.0f;
    }
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
