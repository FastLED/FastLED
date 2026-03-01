#pragma once

/// Forward declarations for fl::string and related types
/// This lightweight header provides forward declarations only, without
/// pulling in heavy dependencies like vector, optional, variant, etc.
/// Use this when you only need to reference string pointers/references
/// in function signatures or type declarations.

#include "fl/int.h"
#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

namespace fl {

// Forward declarations for string types
class string;  // IWYU pragma: keep
template <fl::size N> class StrN;  // IWYU pragma: keep

class StringHolder;  // IWYU pragma: keep

} // namespace fl
