/// @file _build.cpp
/// @brief Master unity build for third_party/ library
/// Compiles entire third_party/ code into single object file
/// This is the ONLY .cpp file in src/third_party/** that should be compiled

#include "fl/sketch_macros.h"

// Heavy multimedia third_party libraries are excluded on memory-constrained platforms
// (AVR, ATtiny, ESP8266, etc.) because their static data arrays exceed available RAM.
// For example, stb_vorbis alone uses 2KB+ for lookup tables.
// NOTE: This is required due to PlatformIO LTO bug - it uses standard 'ar' instead
// of 'gcc-ar', so LTO bytecode (GIMPLE) isn't processed and unused code/data in
// static libraries cannot be eliminated by the linker.
// Bug: https://github.com/platformio/platform-espressif32/pull/702
#if SKETCH_HAS_LOTS_OF_MEMORY

// Subdirectory implementations (alphabetical order)
#include "third_party/stb/truetype/_build.hpp"
#include "third_party/TJpg_Decoder/_build.hpp"
#include "third_party/TJpg_Decoder/src/_build.hpp"
#include "third_party/cq_kernel/_build.hpp"
#include "third_party/libhelix_mp3/_build.hpp"
#include "third_party/libnsgif/_build.hpp"
#include "third_party/libnsgif/src/_build.hpp"
#include "third_party/mpeg1_decoder/_build.hpp"
#include "third_party/object_fled/src/_build.hpp"
#include "third_party/stb/_build.hpp"

#endif  // SKETCH_HAS_LOTS_OF_MEMORY
