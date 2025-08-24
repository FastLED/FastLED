

#include "fl/int.h"

namespace fl {

enum MicType {
    MicTypeInmp441,
};

class SoundUtil {
public:
    static float rms(const i16 *samples, size_t num_samples);
    static float rms_to_dB(MicType type, float rms_loudness);
};

}  // namespace fl
