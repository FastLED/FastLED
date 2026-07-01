// IWYU pragma: private

/// @file rmt_memory_manager_4.cpp.hpp
/// @brief Platform-default singleton constructor for `RmtMemoryManager4`
///        (#3469). Class body itself is header-inline — see the .h
///        for the docblock.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"

// SOC_RMT_MEM_WORDS_PER_CHANNEL is 64 on both classic ESP32 and S2 on
// IDF 4.x. Guarded fallback in case the SOC header isn't pulled in on
// some corners of the toolchain.
#ifndef FASTLED_RMT4_MEM_WORDS_PER_CHANNEL
#if defined(SOC_RMT_MEM_WORDS_PER_CHANNEL)
#define FASTLED_RMT4_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#else
#define FASTLED_RMT4_MEM_WORDS_PER_CHANNEL 64
#endif
#endif

namespace fl {

namespace {

/// @brief Total pool size in words for the current platform.
///        Classic ESP32 = 8 channels × 64 words = 512.
///        ESP32-S2      = 4 channels × 64 words = 256.
constexpr size_t platformPoolWords() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32S2)
    return 4 * FASTLED_RMT4_MEM_WORDS_PER_CHANNEL;
#else
    return 8 * FASTLED_RMT4_MEM_WORDS_PER_CHANNEL;
#endif
}

} // anonymous namespace

RmtMemoryManager4 &RmtMemoryManager4::instance() FL_NO_EXCEPT {
    // Function-local static: single global manager, constructed on
    // first use with the platform's pool size baked in.
    static RmtMemoryManager4 sInstance(platformPoolWords());
    return sInstance;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM &&
       // !FASTLED_RMT5
#endif // FL_IS_ESP32
