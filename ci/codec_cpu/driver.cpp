#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fl/codec/mp3_memory.h"

// Pinned to the float variant: this driver links minimp3_audit.o, which pins it
// the same way, and the audit's host baselines are keyed by that build's
// symbols. minimp3.h now defaults to fixed point, so relying on the default
// would silently swap the measured decoder.
#define MINIMP3_FLOAT_POINT 1
#include "third_party/minimp3/minimp3.h"
#undef MINIMP3_FLOAT_POINT

#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#define FL_CODEC_CPU_HAS_CALLGRIND
#endif

namespace {

constexpr int STAGE_COUNT = 8;
constexpr int STACK_DEPTH = 32;

struct StageMetrics {
    uint64_t multiplies;
    uint64_t macs;
    uint64_t nanoseconds;
};

StageMetrics gMetrics[STAGE_COUNT] = {};
int gStageStack[STACK_DEPTH] = {};
uint64_t gStartStack[STACK_DEPTH] = {};
int gDepth = 0;

uint64_t monotonicNanoseconds() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

uint64_t cycleCounter() {
#if defined(__clang__)
    return __builtin_readcyclecounter();
#else
#error "The codec CPU audit requires Clang's cycle-counter builtin"
#endif
}

void callgrindStart() {
#if defined(FL_CODEC_CPU_HAS_CALLGRIND)
    CALLGRIND_START_INSTRUMENTATION;
#endif
}

void callgrindStop() {
#if defined(FL_CODEC_CPU_HAS_CALLGRIND)
    CALLGRIND_STOP_INSTRUMENTATION;
#endif
}

void callgrindReset() {
#if defined(FL_CODEC_CPU_HAS_CALLGRIND)
    CALLGRIND_ZERO_STATS;
#endif
}

const char* stageName(int stage) {
    static const char* const names[STAGE_COUNT] = {
        "scalefactors", "huffman", "dequant", "stereo", "reorder",
        "antialias", "imdct", "synthesis",
    };
    return stage >= 0 && stage < STAGE_COUNT ? names[stage] : "invalid";
}

bool loadFixture(const char* path, unsigned char** data, size_t* size) {
    FILE* file = ::fopen(path, "rb");
    if (!file) {
        return false;
    }
    ::fseek(file, 0, SEEK_END);
    const long length = ::ftell(file);
    ::rewind(file);
    if (length <= 0) {
        ::fclose(file);
        return false;
    }
    *data = static_cast<unsigned char*>(::malloc(static_cast<size_t>(length)));
    if (!*data) {
        ::fclose(file);
        return false;
    }
    *size = ::fread(*data, 1, static_cast<size_t>(length), file);
    ::fclose(file);
    if (*size != static_cast<size_t>(length)) {
        ::free(*data);
        *data = nullptr;
        *size = 0;
        return false;
    }
    return true;
}

uint64_t checksumPcm(const int16_t* pcm, int samples, int channels) {
    uint64_t value = 1469598103934665603ULL;
    for (int i = 0; i < samples * channels; ++i) {
        value ^= static_cast<uint16_t>(pcm[i]);
        value *= 1099511628211ULL;
    }
    return value;
}

bool decodeMinimp3(const unsigned char* data, size_t size, uint64_t* checksum,
                   int* frames) {
    const int frames_before = *frames;
    fl::third_party::mp3dec_t* decoder =
        static_cast<fl::third_party::mp3dec_t*>(
            ::malloc(sizeof(fl::third_party::mp3dec_t)));
    fl::third_party::mp3dec_scratch_t* scratch =
        static_cast<fl::third_party::mp3dec_scratch_t*>(
            ::malloc(sizeof(fl::third_party::mp3dec_scratch_t)));
    if (!decoder || !scratch) {
        ::free(scratch);
        ::free(decoder);
        return false;
    }
    int16_t* pcm = static_cast<int16_t*>(::malloc(2304 * sizeof(int16_t)));
    if (!pcm) {
        ::free(scratch);
        ::free(decoder);
        return false;
    }
    fl::third_party::mp3dec_init(decoder);
    size_t offset = 0;
    while (offset + 4 < size) {
        fl::third_party::mp3dec_frame_info_t info = {};
        const int samples = fl::third_party::mp3dec_decode_frame_r(
            decoder, scratch, data + offset, static_cast<int>(size - offset),
            pcm, &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        offset += static_cast<size_t>(info.frame_bytes);
        if (samples > 0 && info.channels > 0) {
            *checksum ^= checksumPcm(pcm, samples, info.channels);
            ++*frames;
        }
    }
    ::free(pcm);
    ::free(scratch);
    ::free(decoder);
    return *frames > frames_before;
}

} // anonymous namespace

extern "C" void fastled_mp3_cpu_stage_enter(int stage) {
    if (stage < 0 || stage >= STAGE_COUNT || gDepth >= STACK_DEPTH) {
        ::abort();
    }
    if (gDepth > 0) {
        const uint64_t now = monotonicNanoseconds();
        gMetrics[gStageStack[gDepth - 1]].nanoseconds +=
            now - gStartStack[gDepth - 1];
    }
    gStageStack[gDepth] = stage;
    gStartStack[gDepth] = monotonicNanoseconds();
    ++gDepth;
}

extern "C" void fastled_mp3_cpu_stage_exit(int stage) {
    if (gDepth <= 0 || gStageStack[gDepth - 1] != stage) {
        ::abort();
    }
    const uint64_t now = monotonicNanoseconds();
    --gDepth;
    gMetrics[stage].nanoseconds += now - gStartStack[gDepth];
    if (gDepth > 0) {
        gStartStack[gDepth - 1] = monotonicNanoseconds();
    }
}

extern "C" void fastled_mp3_cpu_operation(int stage, int multiplies,
                                           int macs) {
    if (multiplies < 0 || macs < 0) {
        ::abort();
    }
    if (stage < 0 && gDepth > 0) {
        stage = gStageStack[gDepth - 1];
    }
    if (stage < 0 || stage >= STAGE_COUNT) {
        ::abort();
    }
    StageMetrics& metrics = gMetrics[stage];
    metrics.multiplies += static_cast<uint64_t>(multiplies);
    metrics.macs += static_cast<uint64_t>(macs);
}

namespace fl {

void* memcpy(void* dst, const void* src, fl::size n) FL_NO_EXCEPT {
    return ::memcpy(dst, src, n);
}

void* memmove(void* dst, const void* src, fl::size n) FL_NO_EXCEPT {
    return ::memmove(dst, src, n);
}

void* memset(void* dst, int value, fl::size n) FL_NO_EXCEPT {
    return ::memset(dst, value, n);
}

namespace third_party {

FL_NO_INLINE void* Mp3MemoryAllocate(
    fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT {
    (void)tag;
    return ::malloc(bytes);
}

void Mp3MemoryFree(void* ptr, fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT {
    (void)bytes;
    (void)tag;
    ::free(ptr);
}

} // namespace third_party
} // namespace fl

int main(int argc, char** argv) {
    if (argc != 2 || ::strcmp(argv[1], "minimp3-float") != 0) {
        ::fprintf(stderr, "usage: codec_cpu_driver minimp3-float\n");
        return 2;
    }
    uint64_t checksum = 0;
    uint64_t cycles = 0;
    int frames = 0;
    static const char* const corpus[] = {
        "tests/data/codec/minimp3/l3-hecommon.bit",
        "tests/data/codec/minimp3/l3-compl-cut.mp3",
        "tests/data/codec/minimp3/l3-he_free.bit",
        "tests/data/codec/minimp3/l3-lame-vbrtag.bit",
        "tests/data/codec/minimp3/M2L3_bitrate_16_all.bit",
    };
    bool ok = true;
    callgrindReset();
    for (unsigned index = 0; index < sizeof(corpus) / sizeof(corpus[0]); ++index) {
        unsigned char* data = nullptr;
        size_t size = 0;
        if (!loadFixture(corpus[index], &data, &size)) {
            return 3;
        }
        callgrindStart();
        const uint64_t cycle_start = cycleCounter();
        const bool decoded = decodeMinimp3(data, size, &checksum, &frames);
        cycles += cycleCounter() - cycle_start;
        callgrindStop();
        ::free(data);
        ok = ok && decoded;
    }
    if (!ok || gDepth != 0 || frames <= 0) {
        return 4;
    }
    for (int stage = 0; stage < STAGE_COUNT; ++stage) {
        ::printf("OPS:backend=%s:stage=%s:multiplies=%llu:macs=%llu\n",
                 argv[1], stageName(stage),
                 static_cast<unsigned long long>(gMetrics[stage].multiplies),
                 static_cast<unsigned long long>(gMetrics[stage].macs));
        ::printf("TIMING:backend=%s:stage=%s:nanoseconds=%llu\n", argv[1],
                 stageName(stage),
                 static_cast<unsigned long long>(gMetrics[stage].nanoseconds));
    }
    ::printf(
        "CPU_AUDIT_RESULT:backend=%s:frames=%d:checksum=%llu:cycles=%llu\n",
        argv[1], frames, static_cast<unsigned long long>(checksum),
        static_cast<unsigned long long>(cycles));
    return 0;
}
