// Standalone Helix translation unit for target-ISA code generation auditing --
// the reference counterpart to minimp3_fixed_codegen.cpp. BENCHMARK ONLY.
//
// libhelix_mp3 is RPSL/RCSL-licensed and is NOT part of the FastLED library.
// See ci/codec_cpu/reference/README.md. Nothing under src/ includes this file
// and no user firmware links it; ci/codec_cpu/text_size.py compiles it, reads
// the object with objdump, and throws it away.
//
// It exists because host Callgrind reads 1.00x against Helix while an
// ESP32-C6 reads 1.17x. That ~17% is register pressure, 64-bit operations and
// spill traffic that an x86-64 host with sixteen 64-bit registers never pays.
// Comparing per-function riscv32 code against the same functions on x86-64,
// for both decoders, is how that difference becomes visible without a device.
//
// The fl/stl/stdint.h shim is needed only for the cross build. Helix's headers
// pull in <stdlib.h>, and on riscv32-esp-elf newlib's size_t is `unsigned int`
// while fl::size_t without a platform define is `unsigned long`; the ESP32
// define is what makes the two agree. On the host the platform headers already
// supply everything and including the shim first is what *causes* a conflict,
// so it is left out there. Same conditional as minimp3_fixed_codegen.cpp, for
// the same reason.

#if defined(FL_CODEC_CPU_CODEGEN_ESP_TYPES)
#define ESP32
#include "fl/stl/stdint.h"
#undef ESP32
typedef fl::u64 uint64_t;
typedef fl::i64 int64_t;
#endif

#include "third_party/libhelix_mp3/_build.cpp.hpp"
