// ok no header - CFastLED method body; declaration lives in src/FastLED.h.
/// @file fastled_load_channels.cpp.hpp
/// @brief CFastLED::loadChannels(const fl::Fled&) - bundle-load helper for
/// the .fled subsystem (#3311 PR5). Iterates over the channels section of
/// the parsed bundle and re-uses the runtime FastLED.add(ChannelConfig)
/// path so each strip gets wired in like a hand-coded call would.
///
/// Lives in its own TU so the linker can drop loadChannels when the
/// sketch never calls it (linker tree-shake via --gc-sections).

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/channels/config.h"
#include "fl/fled/fled.h"
#include "fl/stl/shared_ptr.h"

void CFastLED::loadChannels(const fl::Fled& fled) {
    fl::shared_ptr<fl::MultiChannelConfig> mc = fled.channels();
    if (!mc) {
        return;
    }
    for (const auto& cfg : mc->mChannels) {
        if (cfg) {
            FastLED.add(*cfg);
        }
    }
}
