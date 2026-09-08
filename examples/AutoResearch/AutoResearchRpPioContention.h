#pragma once

#include "fl/stl/json.h"

namespace autoresearch {

/// Reproduce FastLED#1471's PIO state-machine contention on real RP silicon.
///
/// Starves every PIO state machine the way Adafruit TinyUSB does, then builds
/// a clockless controller and checks that FastLED declines cleanly instead of
/// stealing a state machine out from under the other library.
fl::json runRpPioContentionTest();

} // namespace autoresearch
