// Standalone fixed-point minimp3 translation unit for target-ISA code
// generation auditing -- the integer counterpart to minimp3_codegen.cpp.
//
// The wrappers take int32_t because the fixed-point kernels do; the float TU's
// take float. That is why this is a separate file rather than a define flipped
// in the other one.

#include "fl/stl/compiler_control.h"

#if defined(FL_CODEC_CPU_CODEGEN_ESP_TYPES)
#define ESP32
#include "fl/stl/stdint.h"
#undef ESP32
#else
#include "fl/stl/stdint.h"
#endif

// fl/stl/stdint.h publishes uint32_t/int32_t at global scope but, for the
// 64-bit pair, emits `u64`/`i64` rather than `uint64_t`/`int64_t`. minimp3's
// scratch alignment member needs the first and the whole fixed-point pipeline
// needs the second, and this TU's include path is `-I src` only -- no platform
// header arrives to supply them. Declared here rather than in the shared header
// because changing that one carries a conflicting-typedef risk its own comments
// document at length. See FastLED#4110.
typedef fl::u64 uint64_t;
typedef fl::i64 int64_t;

#define MINIMP3_FIXED_POINT 1
#include "third_party/minimp3/_build.cpp.hpp"
#undef MINIMP3_FIXED_POINT

extern "C" FL_NO_INLINE void
fl_codec_cpu_minimp3_fixed_dct32(int32_t* grbuf, int bands) FL_NO_EXCEPT {
    fl::third_party::mp3d_DCT_II(grbuf, bands);
}

extern "C" FL_NO_INLINE void fl_codec_cpu_minimp3_fixed_polyphase(
    fl::third_party::mp3d_sample_t* pcm, int channels,
    const int32_t* coefficients) FL_NO_EXCEPT {
    fl::third_party::mp3d_synth_pair(pcm, channels, coefficients);
}
