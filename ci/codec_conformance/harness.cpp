// Decode one MPEG audio bitstream with the shipping fixed-point decoder and
// report PSNR against its reference PCM.
//
// Deliberately standalone rather than a unit test: the full ISO conformance
// suite is 83 vector pairs and 19 MB of reference PCM, which is not something
// to vendor into a library people clone. ci/codec_conformance/run.py fetches
// them from the pinned upstream revision and drives this binary once per
// vector.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__)
#include <immintrin.h>
#endif

#include "fl/codec/mp3_memory.h"

#if defined(CONFORMANCE_FLOAT)
#define MINIMP3_FLOAT_POINT 1
#else
#define MINIMP3_FIXED_POINT 1
#endif
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION

namespace fl {
void* memcpy(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memcpy(d, s, n); }
void* memmove(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memmove(d, s, n); }
void* memset(void* d, int v, fl::size n) FL_NO_EXCEPT { return ::memset(d, v, n); }
namespace third_party {
void* Mp3MemoryAllocate(fl::size b, Mp3MemoryTag) FL_NO_EXCEPT { return ::malloc(b); }
void Mp3MemoryFree(void* p, fl::size, Mp3MemoryTag) FL_NO_EXCEPT { ::free(p); }
}
}

namespace {

/* Owns a malloc'd block and closes over free(). The harness is short-lived and
   nothing here would leak in a way that matters, but the repo asks for RAII
   over raw owning pointers and there is no reason for a test tool to be the
   exception. */
class Buffer {
public:
    Buffer() : mData(NULL), mSize(0) {}
    ~Buffer() { ::free(mData); }

    bool load(const char* path) {
        FILE* file = ::fopen(path, "rb");
        if (!file) {
            return false;
        }
        ::fseek(file, 0, SEEK_END);
        const long length = ::ftell(file);
        ::rewind(file);
        if (length < 0) {
            ::fclose(file);
            return false;
        }
        unsigned char* data =
            (unsigned char*)::malloc(length ? (size_t)length : 1);
        if (!data) {
            ::fclose(file);
            return false;
        }
        if (length && ::fread(data, 1, (size_t)length, file) != (size_t)length) {
            ::free(data);
            ::fclose(file);
            return false;
        }
        ::fclose(file);
        ::free(mData);
        mData = data;
        mSize = (size_t)length;
        return true;
    }

    const unsigned char* data() const { return mData; }
    size_t size() const { return mSize; }

private:
    Buffer(const Buffer&);
    Buffer& operator=(const Buffer&);
    unsigned char* mData;
    size_t mSize;
};

template <typename T>
class Owned {
public:
    explicit Owned(size_t bytes) : mPtr((T*)::malloc(bytes)) {}
    ~Owned() { ::free(mPtr); }
    T* get() const { return mPtr; }

private:
    Owned(const Owned&);
    Owned& operator=(const Owned&);
    T* mPtr;
};

/* log10 without libm: the harness links no math library on purpose, and this
   only has to be accurate enough to compare against a 60 dB floor. */
double log10_approx(double x) {
    int exponent = 0;
    while (x >= 10.0) { x /= 10.0; ++exponent; }
    while (x < 1.0)   { x *= 10.0; --exponent; }
    double z = (x - 1.0) / (x + 1.0), z2 = z * z, sum = 0.0, term = z;
    for (int k = 1; k <= 19; k += 2) { sum += term / k; term *= z2; }
    return exponent + 2.0 * sum / 2.302585092994046;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        ::fprintf(stderr, "usage: harness <bitstream> <reference.pcm>\n");
        return 2;
    }
    Buffer bitstream, referenceBytes;
    if (!bitstream.load(argv[1]) || !referenceBytes.load(argv[2])) {
        ::printf("RESULT status=load_failed\n");
        return 1;
    }
    const unsigned char* data = bitstream.data();
    const long n = (long)bitstream.size();
    const int16_t* ref = (const int16_t*)referenceBytes.data();
    const size_t refn = referenceBytes.size() / 2;

    Owned<fl::third_party::mp3dec_t> decoder(sizeof(fl::third_party::mp3dec_t));
    Owned<fl::third_party::mp3dec_scratch_t> scratchStore(
        sizeof(fl::third_party::mp3dec_scratch_t));
    Owned<int16_t> pcmStore(2304 * sizeof(int16_t));
    fl::third_party::mp3dec_t* dec = decoder.get();
    fl::third_party::mp3dec_scratch_t* scratch = scratchStore.get();
    int16_t* pcm = pcmStore.get();
    if (!dec || !scratch || !pcm) {
        ::printf("RESULT status=oom\n");
        return 1;
    }
    fl::third_party::mp3dec_init(dec);

    double squared_error = 0.0;
    size_t compared = 0, produced = 0;
    int frames = 0, layer = 0, hz = 0, channels = 0;
    size_t offset = 0;
    while (offset + 4 < (size_t)n) {
        fl::third_party::mp3dec_frame_info_t info;
        ::memset(&info, 0, sizeof(info));
        const int samples = fl::third_party::mp3dec_decode_frame_r(
            dec, scratch, data + offset, (int)(n - offset), pcm, &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        offset += (size_t)info.frame_bytes;
        if (samples <= 0) {
            continue;
        }
        ++frames;
        if (!layer) { layer = info.layer; hz = info.hz; channels = info.channels; }
        for (int i = 0; i < samples * info.channels; ++i) {
            if (produced < refn) {
                const double d = (double)ref[produced] - pcm[i];
                squared_error += d * d;
                ++compared;
            }
            ++produced;
        }
    }
    if (!compared) {
        ::printf("RESULT status=no_overlap frames=%d produced=%zu reference=%zu\n",
                 frames, produced, refn);
        return 1;
    }
    const double mse = squared_error / compared;
    const double psnr =
        mse == 0.0 ? 999.0 : 10.0 * log10_approx((32767.0 * 32767.0) / mse);
    ::printf("RESULT status=ok psnr=%.2f layer=%d hz=%d channels=%d frames=%d "
             "produced=%zu reference=%zu\n",
             psnr, layer, hz, channels, frames, produced, refn);
    return 0;
}
