/// #include <Arduino.h>
// #include <iostream>
// #include "audio_types.h"
// // #include "defs.h"
// #include "thirdparty/cq_kernel/cq_kernel.h"
// #include "thirdparty/cq_kernel/kiss_fftr.h"
// #include "util.h"

#define FASTLED_INTERNAL 1

#include "FastLED.h"

#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

#include "fl/array.h"
#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/str.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"

// #define SAMPLES IS2_AUDIO_BUFFER_LEN
#define AUDIO_SAMPLE_RATE 44100
#define SAMPLES 512
#define BANDS 16
#define SAMPLING_FREQUENCY AUDIO_SAMPLE_RATE
#define MAX_FREQUENCY 4698.3
#define MIN_FREQUENCY 174.6
#define MIN_VAL 5000 // Equivalent to 0.15 in Q15

#define PRINT_HEADER 1

namespace fl {


FFT::FFT() {
   mMap.reset(new HashMap(8));
};

FFT::~FFT() = default;

void FFT::run(const Slice<const int16_t> &sample, FFTBins *out,
              const FFT_Args &args) {
    FFT_Args args2 = args;
    args2.samples = sample.size();
    get_or_create(args2).run(sample, out);
}

void FFT::clear() { mMap->clear(); }

size_t FFT::size() const { return mMap->size(); }

} // namespace fl
