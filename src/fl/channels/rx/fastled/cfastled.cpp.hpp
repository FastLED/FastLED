// ok no header - CFastLED is declared in FastLED.h
#define FASTLED_INTERNAL
// IWYU pragma: private

/// @brief CFastLED runtime RX channel entry point, linked independently.

#include "FastLED.h" // ok include: implements public CFastLED methods
#include "fl/channels/rx/channel.h"
fl::RxChannelPtr CFastLED::addRx(const fl::RxChannelConfig& config) {
    return fl::RxChannel::create(config);
}
