// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#include "fl/math/random.h"
#include "fl/stl/singleton.h"
#include "fl/stl/mutex.h"

namespace fl {

namespace {

struct LockedRandom {
    fl::mutex mtx;
    math::random rng;
};

} // namespace

math::random& default_random() {
    return Singleton<LockedRandom>::instance().rng;
}

} // namespace fl
