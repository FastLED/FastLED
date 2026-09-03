// Fixed-point minimp3 instantiation for the CPU audit driver.
//
// The CPU audit measures both variants in one binary, and they cannot share a
// translation unit -- the header is included twice with different DSP paths and
// different mp3dec_scratch_t layouts. This TU carries the fixed-point build
// under its own namespace so the float build (ci/codec_memory/minimp3_audit.cpp,
// pinned to MINIMP3_FLOAT_POINT and linked into the same driver) keeps the
// unqualified fl::third_party names its ledger rows and host baselines are
// keyed to.
//
// This TU does not pin a SIMD setting because it does not get to: every CPU
// audit build passes -DMINIMP3_NO_SIMD through _common_compile_flags(), for the
// instrumented and plain drivers alike. So the whole CPU audit measures the
// scalar path.
//
// That is the right choice for the operation ledger, which is meant to be a
// portable, host-independent count of the arithmetic the algorithm performs --
// pinning it to scalar is what keeps it stable when a kernel is vectorised, and
// it is why the fixed and float builds report identical synthesis and antialias
// multiply counts. It is also why FastLED#4109's IMDCT kernel does not perturb
// these numbers.
//
// Whether the *host counter and Callgrind* halves should instead measure the
// vectorised build -- which is what actually ships -- is a fair question, but it
// applies equally to the float row that has been measured this way since #4053,
// so changing it belongs in its own issue rather than here.

#include <stdint.h>
#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "platforms/new.h"

#define MINIMP3_NAMESPACE minimp3_fixed
#define MINIMP3_FIXED_POINT 1
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_FIXED_POINT
#undef MINIMP3_NAMESPACE
