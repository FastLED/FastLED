#include "fl/stl/string.h" // ok no header - public declaration remains in parent directory.
#include "fl/stl/string_interner.h"
#include "fl/stl/string_view.h"
#include "fl/stl/noexcept.h"

namespace fl {

string string::interned(const char* str, fl::size len) FL_NO_EXCEPT {
    if (!str || len == 0) return string();
    // Route through the global interner so identical content
    // returns the same shared StringHolder (O(1) average lookup,
    // matches `string::intern()`'s semantics). Previously this
    // family wrapped a fresh StringHolder per call with no
    // deduplication — see #2961 CR thread + the follow-on commit.
    return global_interner().intern(fl::string_view(str, len));
}

string string::interned(const char* str) FL_NO_EXCEPT {
    if (!str) return string();
    return global_interner().intern(str);
}

string string::interned(const string_view& sv) FL_NO_EXCEPT {
    return global_interner().intern(sv);
}

// String interning method implementation
string& string::intern() {
    // Skip interning if using inline storage (SSO) - already efficient, no heap allocation
    if (isInline()) {
        return *this;
    }

    // Intern via global interner - replaces this string with deduplicated version
    *this = global_interner().intern(*this);
    return *this;
}


} // namespace fl
