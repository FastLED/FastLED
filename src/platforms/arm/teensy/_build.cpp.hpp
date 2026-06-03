// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\arm\teensy/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)

// Audio-input dispatch lives above the marker because it is conditional
// (the unity-build linter expects the begin-marker immediately above the
// first unconditional same-level include).
#include "platforms/arm/teensy/audio_input_teensy_config.h"
#if FASTLED_USES_TEENSY_AUDIO_INPUT
#define FASTLED_TEENSY_AUDIO_INPUT_IMPL                                      \
    "platforms/arm/teensy/audio_input_teensy.cpp.hpp"
#include FASTLED_TEENSY_AUDIO_INPUT_IMPL
#undef FASTLED_TEENSY_AUDIO_INPUT_IMPL
#endif

// begin current directory includes
#include "platforms/arm/teensy/init_teensy4.cpp.hpp"
#include "platforms/arm/teensy/semaphore_teensy.cpp.hpp"

// begin sub directory includes
#include "platforms/arm/teensy/teensy4_common/_build.cpp.hpp"
