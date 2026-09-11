// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

#include "fl/stl/int.h"

namespace fl {
namespace audio {

/// Microphone frequency response correction profile.
/// Each profile provides 16-channel gain corrections to flatten the frequency
/// response of a specific microphone. Derived from manufacturer datasheet data.
///
/// Set on Processor via setMicProfile() — the correction is applied
/// inside EqualizerDetector before per-bin normalization.
enum class MicProfile : u8 {
    None,           ///< No correction (flat response assumed)
    INMP441,        ///< InvenSense INMP441 MEMS mic (most common)
    ICS43434,       ///< InvenSense ICS-43434 MEMS mic
    SPM1423,        ///< Knowles SPM1423 MEMS mic
    GenericMEMS,    ///< Generic MEMS microphone (moderate bass rolloff)
    LineIn,         ///< Line-in input (relatively flat, minor HF rolloff)
};

} // namespace audio
} // namespace fl
