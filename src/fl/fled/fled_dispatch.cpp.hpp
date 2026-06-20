// fl::detail::fledDispatcher() singleton storage.
// Lives in fl.fled+.cpp unity; tree-shakes when nobody calls fledDispatcher().

#include "fl/fled/fled_dispatch.h"

namespace fl {
namespace detail {

FledDispatcher& fledDispatcher() FL_NO_EXCEPT {
    static FledDispatcher sInstance;
    return sInstance;
}

}  // namespace detail
}  // namespace fl
