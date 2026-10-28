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

// ESP8266 is now automatic, like ESP32 (#4947). fbuild packages the ESP8266
// core's littlefs submodule (FastLED/fbuild#1380), and the adapter
// (`platforms/arduino/fs/embedded_fs_arduino.hpp`) still guards on
// FL_HAS_INCLUDE(<LittleFS.h>) / <FS.h> and degrades to a null filesystem when
// the library is absent, so no opt-in define is needed. The retired
// `FL_ESP8266_EMBEDDED_FS` gate previously worked around the empty submodule.
#if defined(FL_IS_ESP32) || defined(FL_IS_ESP8266)
// ESP cores back on-chip flash with LittleFS.
#include "platforms/esp/fs/embedded_fs_esp.hpp"
#elif defined(FL_IS_WASM)
// The browser VFS stands in for on-chip flash so a LittleFS sketch runs
// unmodified in the web preview.
#include "platforms/wasm/fs/embedded_fs_wasm.hpp"
#elif defined(FL_IS_STUB)
// The host has no on-chip flash. It does have a directory, which is the
// concept this API names, so a unit test can point at one and exercise the
// real read path instead of only the null fallback (#4007 question 3). Null
// until a test asks, so nothing that does not ask changes behaviour -- and
// null unconditionally in a host build that is not a test build, which the
// fragment decides for itself rather than making this dispatcher care.
#include "platforms/stub/fs/embedded_fs_stub.hpp"
#else
// No embedded storage on this platform (AVR, Teensy, ARM).
// RP2040 also ships LittleFS and should gain a fragment here.
#include "platforms/shared/embedded_fs_noop.hpp"
#endif

// ok no namespace fl — this is a dispatch header; the fragments it
// selects declare fl::platforms::makeEmbeddedFs themselves.
