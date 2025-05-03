#pragma once

#include "fl/scoped_ptr.h"
#include "fl/slice.h"
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

    // FFT's are expensive to create, so we cache them. This sets the size of the
    // cache. The default is 8.
    void setFFTCacheSize(size_t size);

  private:
    // Get the FFTImpl for the given arguments.
    FFTImpl &get_or_create(const FFT_Args &args);

    //using HashMap = fl::HashMapLru<FFT_Args, Ptr<FFTImpl>>;
    struct HashMap;
    //HashMap mMap = HashMap(8);
    scoped_ptr<HashMap> mMap;
};

}; // namespace fl