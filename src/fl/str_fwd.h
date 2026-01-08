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
class string;
template <fl::size N> class StrN;
using Str = fl::string;  // backwards compatibility

class StringHolder;

} // namespace fl
