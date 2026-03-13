// IWYU pragma: private
// ok no namespace fl
/// @file platforms/esp/32/ota/ota_lwip.h
/// Trampoline for lwip sockets under Arduino framework.
///
/// The ESP-IDF compat wrapper <lwip/sockets.h> uses #include_next to chain
/// to the real lwip header. Under fbuild's trampoline include system, the
/// #include_next resolution fails because include paths aren't ordered the
/// way #include_next expects.
///
/// This header works around the issue by providing socklen_t and the lwip
/// includes. Callers must use lwip_* function names directly (lwip_socket,
/// lwip_bind, lwip_connect, etc.) instead of POSIX names, because POSIX-name
/// macros would collide with unrelated C++ code (e.g., json::accept()).

#pragma once

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

// Provide socklen_t before lwip uses it
#include "fl/stl/int.h"
#ifndef SOCKLEN_T_DEFINED
#define SOCKLEN_T_DEFINED
typedef fl::u32 socklen_t;
#endif

// IWYU pragma: begin_keep
#include <lwip/sockets.h>
#include <lwip/netdb.h>
// IWYU pragma: end_keep

#endif  // FL_IS_ESP32
