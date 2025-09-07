

#ifdef ESP32

#include <math.h>  // ok include
#include "sound_util.h"
#include "fl/math.h"
#include "fl/assert.h"


namespace fl {

namespace {
    float inmp441_rms_to_dB(float rms_loudness) {
        // This is a rough approximation of the loudness to dB scale.
        // The data was taken from the following video featuring brown
        // noise: https://www.youtube.com/watch?v=hXetO_bYcMo
        // This linear regression was done on the following data:
        // DB | LOUDNESS
        // ---+---------
        // 50 | 15
        // 55 | 22
        // 60 | 33
        // 65 | 56
        // 70 | 104
        // 75 | 190
        // 80 | 333
        // This will produce an exponential regression of the form:
        //  0.0833 * std::exp(0.119 * x);
        // Below is the inverse exponential regression.
        const float kCoefficient = 0.119f;
        const float kIntercept = 0.0833f;
        const float kInverseCoefficient =
            1.0f / kCoefficient; // Maybe faster to precompute this.
        const float kInverseIntercept = 1.0f / kIntercept;
        double outd =
            std::log(rms_loudness * kInverseIntercept) * kInverseCoefficient;
        return static_cast<float>(outd);
    }
}


float SoundUtil::rms_to_dB(enum MicType type, float rms_loudness) {
    switch (type) {
        case MicTypeInmp441:
            return inmp441_rms_to_dB(rms_loudness);
        default: {
            FL_ASSERT(false, "Unsupported mic type");
            return 0;
        }
    }
}


float SoundUtil::rms(const i16 *samples, size_t num_samples) {
    uint64_t sum_of_squares = 0;
    for (size_t i = 0; i < num_samples; ++i) {
        sum_of_squares += samples[i] * samples[i];
    }
    double mean_square = static_cast<double>(sum_of_squares) / num_samples;
    return static_cast<float>(std::sqrt(mean_square));
}

} // namespace fl

#endif // ESP32
