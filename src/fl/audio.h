#pragma once

#include "fl/ptr.h"
#include "fl/vector.h"
#include <stdint.h>

namespace fl {

FASTLED_SMART_PTR(AudioSample);

class AudioSample : public fl::Referent {
  public:
    using VectorPCM = fl::vector<int16_t>;
    ~AudioSample() {}
    template <typename It> void assign(It begin, It end) {
        mSignedPcm.assign(begin, end);
    }
    const VectorPCM &pcm() const { return mSignedPcm; }

  private:
    VectorPCM mSignedPcm;
};
} // namespace fl