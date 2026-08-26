// Standalone minimp3 translation unit for target-ISA code generation auditing.

#include "fl/stl/compiler_control.h"

#if defined(FL_CODEC_CPU_CODEGEN_ESP_TYPES)
#define ESP32
#include "fl/stl/stdint.h"
#undef ESP32
#else
#include "fl/stl/stdint.h"
#endif

typedef fl::u64 uint64_t;

#include "third_party/minimp3/_build.cpp.hpp"

extern "C" FL_NO_INLINE void
fl_codec_cpu_minimp3_dct32(float* grbuf, int bands) FL_NO_EXCEPT {
    fl::third_party::mp3d_DCT_II(grbuf, bands);
}

extern "C" FL_NO_INLINE void fl_codec_cpu_minimp3_polyphase(
    fl::third_party::mp3d_sample_t* pcm, int channels,
    const float* coefficients) FL_NO_EXCEPT {
    fl::third_party::mp3d_synth_pair(pcm, channels, coefficients);
}
