// IWYU pragma: private



#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

enum MicType { // ok plain enum
    MicTypeInmp441,
};

class SoundUtil {
public:
    static float rms(const i16 *samples, size_t num_samples) FL_NOEXCEPT;
    static float rms_to_dB(MicType type, float rms_loudness) FL_NOEXCEPT;
};

}  // namespace fl
