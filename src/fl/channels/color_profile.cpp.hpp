#pragma once

#include "fl/channels/color_profile.h"

namespace fl {
namespace detail {

// One definition per program. These back FastLED.setColorManagementStrict()
// and FastLED.setDefaultSourceProfile(); they must not be header-inline or
// each shared module would observe its own copy.
bool& colorProfileStrictMode() FL_NO_EXCEPT {
    static bool strict = false;
    return strict;
}

SourceProfile& defaultSourceProfile() FL_NO_EXCEPT {
    static SourceProfile profile = SourceProfile::linearSrgb();
    return profile;
}

}  // namespace detail
}  // namespace fl
