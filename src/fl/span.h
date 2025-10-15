#pragma once

#include "fl/slice.h"

namespace fl {

// span is an alias for Slice - provides std::span-like API
// Default to dynamic extent for backward compatibility
template<typename T, fl::size Extent = fl::dynamic_extent>
using span = Slice<T, Extent>;

};
