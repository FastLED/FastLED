#pragma once

/// @file minimp3_variants.hpp
/// @brief Extra minimp3 instantiations used by the fixed-vs-float golden gates.
///
/// The Phase 3 acceptance gates compare the fixed-point decoder against the
/// float decoder over the same corpus, which means both have to exist in the
/// same test binary. `minimp3.h` supports that: undefining its two include
/// guards and setting `MINIMP3_NAMESPACE` builds another complete copy in a
/// fresh namespace, and the header releases every macro it owns on the way out
/// so the second copy re-derives its own SIMD configuration instead of
/// inheriting the first one's.
///
/// Both copies here are built with `MINIMP3_STAGE_DUMP` pointing at
/// `fl::mp3StageDump`, so a test can watch one kernel at a time rather than
/// only comparing final PCM. The production decoder in `libfastled` is
/// untouched by any of this — it is a third instantiation, in `fl::third_party`,
/// with the hook compiled out.

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Sink for the per-stage taps. Both overloads exist because the float and
/// fixed variants hand back different sample types; whichever variant is
/// decoding picks its overload by ordinary overload resolution.
class Mp3StageSink {
  public:
    virtual ~Mp3StageSink() = default;
    virtual void onStage(int stage, int channel, const float* buf,
                         int count) FL_NO_EXCEPT = 0;
    virtual void onStage(int stage, int channel, const fl::i32* buf,
                         int count) FL_NO_EXCEPT = 0;
};

namespace detail {
inline Mp3StageSink*& mp3StageSinkSlot() FL_NO_EXCEPT {
    static Mp3StageSink* sink = nullptr;
    return sink;
}
} // namespace detail

inline void SetMp3StageSink(Mp3StageSink* sink) FL_NO_EXCEPT {
    detail::mp3StageSinkSlot() = sink;
}

inline void ClearMp3StageSink() FL_NO_EXCEPT {
    detail::mp3StageSinkSlot() = nullptr;
}

inline void mp3StageDump(int stage, int channel, const float* buf,
                         int count) FL_NO_EXCEPT {
    if (Mp3StageSink* sink = detail::mp3StageSinkSlot()) {
        sink->onStage(stage, channel, buf, count);
    }
}

inline void mp3StageDump(int stage, int channel, const fl::i32* buf,
                         int count) FL_NO_EXCEPT {
    if (Mp3StageSink* sink = detail::mp3StageSinkSlot()) {
        sink->onStage(stage, channel, buf, count);
    }
}

} // namespace fl

// ---- float variant, instrumented ------------------------------------------
#undef MINIMP3_H
#undef _MINIMP3_IMPLEMENTATION_GUARD
#define MINIMP3_NAMESPACE minimp3_float_probe
#define MINIMP3_STAGE_DUMP fl::mp3StageDump
#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_STAGE_DUMP
#undef MINIMP3_NAMESPACE

// ---- fixed-point variant, instrumented ------------------------------------
#undef MINIMP3_H
#undef _MINIMP3_IMPLEMENTATION_GUARD
#define MINIMP3_NAMESPACE minimp3_fixed_probe
#define MINIMP3_FIXED_POINT 1
#define MINIMP3_STAGE_DUMP fl::mp3StageDump
#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_STAGE_DUMP
#undef MINIMP3_FIXED_POINT
#undef MINIMP3_NAMESPACE

namespace fl {

/// Uniform surface over one minimp3 instantiation, so a golden test can be
/// written once and run against either variant.
#define FL_MP3_VARIANT_ADAPTER(AdapterName, NS)                                \
    struct AdapterName {                                                       \
        using decoder_type = NS::mp3dec_t;                                     \
        using scratch_type = NS::mp3dec_scratch_t;                             \
        using info_type = NS::mp3dec_frame_info_t;                             \
        using sample_type = NS::mp3d_sample_t;                                 \
        static const char* name() FL_NO_EXCEPT { return #AdapterName; }        \
        static void init(decoder_type* dec) FL_NO_EXCEPT {                     \
            NS::mp3dec_init(dec);                                              \
        }                                                                      \
        static bool isFixedPoint() FL_NO_EXCEPT {                              \
            return NS::mp3dec_is_fixed_point() != 0;                           \
        }                                                                      \
        static bool dspIsInteger() FL_NO_EXCEPT {                              \
            return NS::mp3dec_dsp_is_integer() != 0;                           \
        }                                                                      \
        static int decodeFrame(decoder_type* dec, scratch_type* scratch,       \
                               const fl::u8* mp3, int bytes,                   \
                               sample_type* pcm, info_type* info)              \
            FL_NO_EXCEPT {                                                     \
            return NS::mp3dec_decode_frame_r(dec, scratch, mp3, bytes, pcm,    \
                                             info);                            \
        }                                                                      \
    }

FL_MP3_VARIANT_ADAPTER(Minimp3FloatVariant, fl::minimp3_float_probe);
FL_MP3_VARIANT_ADAPTER(Minimp3FixedVariant, fl::minimp3_fixed_probe);

} // namespace fl
