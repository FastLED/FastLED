// ok no header - CFastLED method body; declaration lives in src/FastLED.h.
/// @file fastled_load.cpp.hpp
/// @brief CFastLED::load(const fl::Fled&) - top-level bundle-load entry
/// point for the .fled subsystem (#3311 PR5). Composes loadChannels +
/// loadScreenMap.
///
/// Note: the issue spec also described enableWasm() / enableMicroPy() +
/// a FledDispatcher singleton for script-host dispatch. That surface is
/// design-only for v1 - the script-host runtimes themselves are out of
/// scope, so wiring the dispatch hooks would be dead code today. If/when
/// the runtimes land they can plug in here.
///
/// Lives in its own TU so the linker can drop load() when the sketch
/// never calls it (linker tree-shake via --gc-sections). Pulling load()
/// transitively pulls loadChannels + loadScreenMap.

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/fled/fled.h"

void CFastLED::load(const fl::Fled& fled) {
    if (!fled) {
        return;
    }
    loadChannels(fled);
    loadScreenMap(fled);
}
