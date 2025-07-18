
#pragma once

// Backward compatibility - include the new unique_ptr header
#include "fl/unique_ptr.h"
#include "fl/scoped_array.h"

namespace fl {

// Template alias for backward compatibility
template<typename T, typename Deleter = default_delete<T>>
using scoped_ptr = unique_ptr<T, Deleter>;

} // namespace fl
