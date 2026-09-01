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
// SIMD is left enabled: the whole point of a CPU row for this variant is to
// track what actually ships on a host, and the vector kernels are what ships.
// That is the opposite of ci/codec_memory/minimp3_fixed_audit.cpp, which pins
// MINIMP3_NO_SIMD because the stack budget it enforces is an MCU budget.

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
