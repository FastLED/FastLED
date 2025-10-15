#pragma once

#include "fl/slice.h"

namespace fl {

// Slice is an alias for span - provides backward compatibility
// Default to dynamic extent for backward compatibility
template<typename T, fl::size Extent = fl::dynamic_extent>
using Slice = span<T, Extent>;

};
