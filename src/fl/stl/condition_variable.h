#pragma once

/// @file fl/stl/condition_variable.h
/// @brief Condition variable wrapper providing a consistent interface across platforms
///
/// This header provides fl::condition_variable that wraps platform-specific
/// condition variable implementations. The actual implementation is delegated to
/// platforms/condition_variable.h, which routes to platform-specific headers.

#include "platforms/condition_variable.h"  // IWYU pragma: keep

namespace fl {

// Re-export platform condition_variable into fl namespace
using platforms::condition_variable;
using platforms::cv_status;

} // namespace fl
