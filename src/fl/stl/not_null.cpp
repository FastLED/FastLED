///////////////////////////////////////////////////////////////////////////////
// not_null.cpp - Low-level assertion implementation for fl::not_null<T>
//
// Provides platform-specific assertion failure handling without depending on
// fl/stl/assert.h (which might depend on not_null in the future, creating
// circular dependencies).
//
// Platform-specific behavior matches the expectations documented in not_null.h:
// - Desktop/Host: stderr output + abort()
// - ESP32: Serial output via fl::println() + abort()
// - WASM: Browser console warning + debugger breakpoint
// - AVR: Minimal output due to memory constraints + abort()
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/not_null.h"
#include "fl/stl/assert.h"

namespace fl {
namespace detail {

// Low-level assertion failure handler for not_null
// Called when a nullptr is detected during construction or assignment
// Platform-specific implementation to avoid circular dependencies
void not_null_assert_failed(const char* message) {
    FL_ASSERT(false, message);
}

} // namespace detail
} // namespace fl
