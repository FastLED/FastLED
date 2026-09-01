/* Emits the expected decode of an MP3 bitstream, for the on-device AutoResearch
   fixture. Uses mp3dec_decode_frame_r directly -- the same entry point the
   device test calls -- so the numbers it prints are what correct hardware must
   reproduce bit for bit, not an approximation of them. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platforms/new.h"
#include "fl/codec/mp3_vbr_tag.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include

/* The decoder calls fl::memcpy and friends; this generator links no FastLED
   library, so forward them to the C runtime, exactly as the conformance
   harness does. */
namespace fl {
void* memcpy(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memcpy(d, s, n); }
void* memmove(void* d, const void* s, fl::size n) FL_NO_EXCEPT { return ::memmove(d, s, n); }
void* memset(void* d, int v, fl::size n) FL_NO_EXCEPT { return ::memset(d, v, n); }
}

namespace {

/* FNV-1a over the little-endian PCM bytes. Chosen over a plain sum because it
   is order-sensitive: a decoder that emits the right samples in the wrong
   order, or drops one and duplicates another, still fails. */
uint32_t fnv1a(uint32_t hash, const int16_t* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const uint16_t v = (uint16_t)samples[i];
        hash = (hash ^ (uint8_t)(v & 0xFF)) * 16777619u;
        hash = (hash ^ (uint8_t)(v >> 8)) * 16777619u;
    }
    return hash;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: fixture_gen <bitstream> <max-frames>\n");
        return 2;
    }
    const int max_frames = atoi(argv[2]);
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "cannot read %s\n", argv[1]); return 1;
    }
    fclose(f);

    fl::third_party::mp3dec_t* dec =
        (fl::third_party::mp3dec_t*)malloc(sizeof(fl::third_party::mp3dec_t));
    fl::third_party::mp3dec_scratch_t* scratch =
        (fl::third_party::mp3dec_scratch_t*)malloc(
            sizeof(fl::third_party::mp3dec_scratch_t));
    int16_t* pcm = (int16_t*)malloc(2304 * sizeof(int16_t));
    fl::third_party::mp3dec_init(dec);

    uint32_t hash = 2166136261u;
    size_t offset = 0, samples_total = 0;
    int frames = 0, hz = 0, channels = 0, layer = 0;
    while (offset + 4 < (size_t)n && frames < max_frames) {
        fl::third_party::mp3dec_frame_info_t info;
        memset(&info, 0, sizeof(info));
        const int samples = fl::third_party::mp3dec_decode_frame_r(
            dec, scratch, data + offset, (int)((size_t)n - offset), pcm, &info);
        if (info.frame_bytes <= 0) break;
        offset += (size_t)info.frame_bytes;
        if (samples > 0) {
            hz = info.hz; channels = info.channels; layer = info.layer;
            const size_t count = (size_t)samples * (size_t)info.channels;
            hash = fnv1a(hash, pcm, count);
            samples_total += count;
            ++frames;
        }
    }
    printf("FIXTURE bytes=%zu frames=%d samples=%zu hz=%d channels=%d layer=%d "
           "fnv1a=0x%08X\n",
           offset, frames, samples_total, hz, channels, layer, hash);
    return 0;
}
