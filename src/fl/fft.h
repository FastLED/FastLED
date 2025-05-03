#pragma once

#include "fl/hash_map_lru.h"
#include "fl/pair.h"
#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include "fl/fft_impl.h"

namespace fl {

class FFT {
  public:
    FFT();
    ~FFT();

    void run(const Slice<const int16_t> &sample, FFTBins *out,
             const FFT_Args &args = FFT_Args());

    void clear();
    size_t size() const;

  private:
    // Get the FFTImpl for the given arguments.
    FFTImpl &get_or_create(const FFT_Args &args);

    using HashMap = fl::HashMapLru<FFT_Args, Ptr<FFTImpl>>;
    //HashMap mMap = HashMap(8);
    scoped_ptr<HashMap> mMap;
};

}; // namespace fl