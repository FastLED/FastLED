/// @file shared_ptr.cpp.hpp
/// Implementation file for shared_ptr ControlBlockBase
///
/// This file provides out-of-line definitions for ControlBlockBase's member
/// functions to prevent UBSAN "invalid vptr" errors when shared_ptr objects
/// cross shared library boundaries.
///
/// Problem: When tests are compiled as shared libraries (.dylib/.dll) and loaded
/// by a runner executable, each binary gets its own copy of inline functions and
/// vtables. UBSAN checks that the vptr matches the expected vtable when accessing
/// members, causing false positives when the control block was created in a
/// different binary than where it's being accessed.
///
/// Solution: By defining these functions out-of-line, each binary has its own
/// implementation that correctly matches its own vtable. The control block
/// operations stay within the same binary that created the control block.

#include "fl/stl/shared_ptr.h"

namespace fl {
namespace detail {

// Out-of-line destructor definition to anchor the vtable to this translation unit.
ControlBlockBase::~ControlBlockBase() = default;

// Reference count increment - must be out-of-line to prevent cross-binary vtable issues.
void ControlBlockBase::add_shared_ref() {
    if (shared_count != NO_TRACKING_VALUE) {
        ++shared_count;
    }
}

// Reference count decrement - must be out-of-line to prevent cross-binary vtable issues.
// Returns true if the reference count reached zero (object should be destroyed).
bool ControlBlockBase::remove_shared_ref() {
    if (shared_count == NO_TRACKING_VALUE) {
        return false;  // Never destroy in no-tracking mode
    }
    return (--shared_count == 0);
}

// Check if this control block is in no-tracking mode.
bool ControlBlockBase::is_no_tracking() const {
    return shared_count == NO_TRACKING_VALUE;
}

} // namespace detail
} // namespace fl
