#pragma once

// Forward declaration for fl::Fled, broken out so FastLED.h can take
// `const fl::Fled&` in CFastLED::load* without pulling the full
// fl/fled/fled.h header into the sketch include graph. Keeps the cost
// of declaring the load surface zero for sketches that never touch
// .fled bundles. See FastLED #3311 PR5.

namespace fl {
class Fled;
}  // namespace fl
