// ok no header - implementation for fl/channels/options.h

#include "fl/channels/options.h"

namespace fl {

bool ChannelOptions::setTargetWhite(Chromaticity white) FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
    if (!validChromaticity(white)) return false;
    mColorProfile.mTargetWhite = white;
    mColorProfile.mHasTargetWhite = true;
    return true;
#else
    FL_UNUSED(white);
    return false;
#endif
}

Chromaticity ChannelOptions::targetWhite() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
    return mColorProfile.mTargetWhite;
#else
    return Chromaticity();
#endif
}

bool ChannelOptions::hasTargetWhite() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
    return mColorProfile.mHasTargetWhite;
#else
    return false;
#endif
}

}  // namespace fl
