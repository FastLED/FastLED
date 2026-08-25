/// @file _build.cpp.hpp
/// @brief Unity build integration for the vendored minimp3 decoder.

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h"
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION
