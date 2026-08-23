#pragma once

/// @file platforms/embedded_fs.h
/// @brief Central distribution header for embedded (on-chip flash) storage.
///
/// Selects exactly one platform fragment supplying
/// `fl::platforms::makeEmbeddedFs(bool)`. The public entry point is
/// `fl::getEmbeddedFs()` in `fl/fs/embedded/embedded_fs.h`; this
/// header is how that reaches a backend without the backend's name
/// appearing anywhere in `fl/` (FastLED #4007).
///
/// Adding a platform: drop a `*_embedded_fs.impl.hpp` fragment beside its
/// siblings, defining `fl::platforms::makeEmbeddedFs`, and add a branch
/// here. Fragments are `.impl.hpp` — plain headers composed into the
/// caller's translation unit — so they need no `_build.cpp.hpp` of their
/// own and stay inside the tree-shakable TU.

#include "platforms/is_platform.h"

// ESP8266 is opt-in, not automatic. Its Arduino core carries littlefs as a git
// submodule at libraries/LittleFS/lib/littlefs, and fbuild caches the core
// without it -- the directory is created empty, so <LittleFS.h> is present and
// resolves, then fails on its own `#include "../lib/littlefs/lfs.h"`. A
// FL_HAS_INCLUDE guard cannot see that: the header exists, its contents do not.
// Tracked as FastLED/fbuild#1380. Define FASTLED_ESP8266_EMBEDDED_FS to opt in
// on a toolchain whose core is complete; this can go back to automatic once
// fbuild packages the submodule.
#if defined(FL_IS_ESP32) || (defined(FL_IS_ESP8266) && defined(FASTLED_ESP8266_EMBEDDED_FS))
// ESP cores back on-chip flash with LittleFS.
#include "platforms/esp/fs/embedded_fs_esp.hpp"
#else
// No embedded storage on this platform (host/stub, WASM, AVR, Teensy,
// ARM). RP2040 also ships LittleFS and should gain a fragment here.
#include "platforms/shared/embedded_fs_noop.hpp"
#endif

// ok no namespace fl — this is a dispatch header; the fragments it
// selects declare fl::platforms::makeEmbeddedFs themselves.
