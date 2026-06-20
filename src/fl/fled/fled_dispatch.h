#pragma once

// fl::detail::FledDispatcher - construct-on-first-use singleton wiring the
// optional script back-ends to CFastLED::load (#3311 PR5). Default-empty
// fl::function slots mean a sketch that never calls FastLED.enableWasm()
// or FastLED.enableMicroPy() (and whose .fled bundles carry no script)
// never references this singleton, so it tree-shakes cleanly.

#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {

class Fled;  // fwd

namespace detail {

// Slot holder for the bundle-load dispatch table. PR5 only declares the
// slots; the actual loaders (and the public CFastLED::enableWasm /
// enableMicroPy setters that install them) land in PR6.
class FledDispatcher {
  public:
    fl::function<void(const Fled&)> wasmLoader;     // default-empty
    fl::function<void(const Fled&)> microPyLoader;  // default-empty
};

// Magic-static accessor. The singleton lives in fled_dispatch.cpp.hpp.
FledDispatcher& fledDispatcher() FL_NO_EXCEPT;

}  // namespace detail
}  // namespace fl
