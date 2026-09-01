// Standalone translation unit for fixed-point minimp3 stack/static memory
// auditing.
//
// The sibling minimp3_audit.cpp pulls in the component's unity build, which
// compiles the float decoder. The fixed-point path is the same header with
// MINIMP3_FIXED_POINT defined, so this TU instantiates it directly rather than
// through _build.cpp.hpp -- the two cannot share a unity build without
// colliding, and keeping them in separate objects is what lets the audit report
// their stack and table footprints independently.

#include <stdint.h>
#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "platforms/new.h"

// Scalar deliberately. The 2 KiB decode-stack budget this audit enforces is an
// MCU budget, and no FastLED MCU target compiles the integer SIMD kernels --
// Xtensa, RISC-V and Cortex-M have neither SSE nor NEON. Auditing the host's
// vectorised build would measure a configuration that never ships to the
// targets the budget exists for, and the vector kernels legitimately need more
// stack (the DCT-32 holds its 4x8 scratch as 16-byte vectors rather than
// int32). The SIMD build's own footprint is covered by the bit-exactness and
// perf gates in tests/fl/codec/mp3_fixed_point.hpp.
#define MINIMP3_FIXED_POINT 1
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_NO_SIMD
#undef MINIMP3_FIXED_POINT
