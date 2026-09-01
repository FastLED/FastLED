// Standalone minimp3 translation unit for target-ISA code generation auditing.
//
// The float variant, pinned. The wrappers below take `float*`, the kernel
// needles in ci/codec_cpu/audit.py name the float kernels, and the codegen
// baselines in codec_cpu_trend.json are keyed to them -- so this TU must not
// follow minimp3.h's default, which is fixed point since FastLED#4056.
// FastLED#4110 covers adding the fixed build's cross-target codegen row, which
// needs its own int32_t-typed wrappers rather than a flipped define here.

#include "fl/stl/compiler_control.h"

#if defined(FL_CODEC_CPU_CODEGEN_ESP_TYPES)
#define ESP32
#include "fl/stl/stdint.h"
#undef ESP32
#else
#include "fl/stl/stdint.h"
#endif

typedef fl::u64 uint64_t;

#define MINIMP3_FLOAT_POINT 1
#include "third_party/minimp3/_build.cpp.hpp"
#undef MINIMP3_FLOAT_POINT

extern "C" FL_NO_INLINE void
fl_codec_cpu_minimp3_dct32(float* grbuf, int bands) FL_NO_EXCEPT {
    fl::third_party::mp3d_DCT_II(grbuf, bands);
}

extern "C" FL_NO_INLINE void fl_codec_cpu_minimp3_polyphase(
    fl::third_party::mp3d_sample_t* pcm, int channels,
    const float* coefficients) FL_NO_EXCEPT {
    fl::third_party::mp3d_synth_pair(pcm, channels, coefficients);
}
