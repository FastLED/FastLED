#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fl/codec/mp3_memory.h"
#include "third_party/libhelix_mp3/pub/mp3dec.h"
#include "third_party/minimp3/minimp3.h"

namespace {

void* volatile gAllocationWitness = nullptr;

} // anonymous namespace

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
    void* ptr = ::malloc(bytes);
    gAllocationWitness = ptr;
    return ptr;
}

void Mp3MemoryFree(void* ptr, fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT {
    (void)bytes;
    (void)tag;
    ::free(ptr);
}

} // namespace third_party
} // namespace fl

namespace {

constexpr size_t STACK_BYTES = 64 * 1024;
// Covers the measurement helper and libc call frames. The audit compiles this
// translation unit with -mno-red-zone so bytes below the live stack pointer are
// otherwise available for painting.
constexpr size_t STACK_GUARD = 256;
constexpr unsigned char PATTERN = 0xa5;

struct DecodeContext {
    const unsigned char* data;
    size_t size;
    unsigned char* stack;
    bool minimp3;
    bool ok;
    size_t stack_usage;
};

uintptr_t currentStackPointer() {
#if defined(__x86_64__)
    uintptr_t stack_pointer;
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack_pointer));
    return stack_pointer;
#else
#error "The decode stack watermark currently requires x86-64"
#endif
}

unsigned char* startWatermark(DecodeContext* context) {
    const uintptr_t stack_pointer = currentStackPointer();
    unsigned char* const paint_end = reinterpret_cast<unsigned char*>(
        stack_pointer - STACK_GUARD);
    if (paint_end <= context->stack) {
        return nullptr;
    }
    ::memset(context->stack, PATTERN,
             static_cast<size_t>(paint_end - context->stack));
    return paint_end;
}

void finishWatermark(DecodeContext* context, unsigned char* paint_end) {
    const size_t painted_bytes = static_cast<size_t>(paint_end - context->stack);
    size_t first_changed = painted_bytes;
    for (size_t i = 0; i < painted_bytes; ++i) {
        if (context->stack[i] != PATTERN) {
            first_changed = i;
            break;
        }
    }
    context->stack_usage = STACK_GUARD +
        (first_changed == painted_bytes ? 0 : painted_bytes - first_changed);
}

__attribute__((noinline))
void* decodeThread(void* opaque) {
    DecodeContext* context = static_cast<DecodeContext*>(opaque);
    const size_t stream_bytes = context->minimp3
                                    ? fl::third_party::MP3_MINIMP3_STREAM_BUFFER_SIZE
                                    : fl::third_party::MP3_HELIX_STREAM_BUFFER_SIZE;
    void* stream = fl::third_party::Mp3MemoryAllocate(
        stream_bytes, fl::third_party::Mp3MemoryTag::StreamBuffer);
    if (context->minimp3) {
        fl::third_party::mp3dec_t* decoder =
            static_cast<fl::third_party::mp3dec_t*>(
                fl::third_party::Mp3MemoryAllocate(
                    sizeof(fl::third_party::mp3dec_t),
                    fl::third_party::Mp3MemoryTag::DecoderState));
        fl::third_party::mp3dec_scratch_t* scratch =
            static_cast<fl::third_party::mp3dec_scratch_t*>(
                fl::third_party::Mp3MemoryAllocate(
                    sizeof(fl::third_party::mp3dec_scratch_t),
                    fl::third_party::Mp3MemoryTag::Scratch));
        int16_t* pcm = static_cast<int16_t*>(
            fl::third_party::Mp3MemoryAllocate(
                2304 * sizeof(int16_t),
                fl::third_party::Mp3MemoryTag::PcmOutput));
        fl::third_party::mp3dec_frame_info_t info = {};
        fl::third_party::mp3dec_init(decoder);
        unsigned char* const paint_end = startWatermark(context);
        if (!paint_end) {
            return nullptr;
        }
        const int samples = fl::third_party::mp3dec_decode_frame_r(
            decoder, scratch, context->data, static_cast<int>(context->size),
            pcm, &info);
        finishWatermark(context, paint_end);
        context->ok = samples > 0 && info.frame_bytes > 0;
        fl::third_party::Mp3MemoryFree(
            pcm, 2304 * sizeof(int16_t),
            fl::third_party::Mp3MemoryTag::PcmOutput);
        fl::third_party::Mp3MemoryFree(
            scratch, sizeof(fl::third_party::mp3dec_scratch_t),
            fl::third_party::Mp3MemoryTag::Scratch);
        fl::third_party::Mp3MemoryFree(
            decoder, sizeof(fl::third_party::mp3dec_t),
            fl::third_party::Mp3MemoryTag::DecoderState);
    } else {
        HMP3Decoder decoder = fl::third_party::MP3InitDecoder();
        short* pcm = static_cast<short*>(fl::third_party::Mp3MemoryAllocate(
            2304 * sizeof(short), fl::third_party::Mp3MemoryTag::PcmOutput));
        const int offset = fl::third_party::MP3FindSyncWord(
            context->data, static_cast<int>(context->size));
        if (offset < 0) {
            fl::third_party::Mp3MemoryFree(
                pcm, 2304 * sizeof(short),
                fl::third_party::Mp3MemoryTag::PcmOutput);
            fl::third_party::MP3FreeDecoder(decoder);
            fl::third_party::Mp3MemoryFree(
                stream, stream_bytes,
                fl::third_party::Mp3MemoryTag::StreamBuffer);
            return nullptr;
        }
        const unsigned char* input = context->data + offset;
        size_t remaining = context->size - static_cast<size_t>(offset);
        unsigned char* const paint_end = startWatermark(context);
        if (!paint_end) {
            return nullptr;
        }
        const int result = fl::third_party::MP3Decode(
            decoder, &input, &remaining, pcm, 0);
        finishWatermark(context, paint_end);
        context->ok = result == 0;
        fl::third_party::Mp3MemoryFree(
            pcm, 2304 * sizeof(short),
            fl::third_party::Mp3MemoryTag::PcmOutput);
        fl::third_party::MP3FreeDecoder(decoder);
    }
    fl::third_party::Mp3MemoryFree(
        stream, stream_bytes, fl::third_party::Mp3MemoryTag::StreamBuffer);
    return nullptr;
}

bool loadFixture(unsigned char** data, size_t* size) {
    FILE* file = ::fopen("tests/data/codec/minimp3/l3-hecommon.bit", "rb");
    if (!file) {
        return false;
    }
    ::fseek(file, 0, SEEK_END);
    const long length = ::ftell(file);
    ::rewind(file);
    *data = static_cast<unsigned char*>(::malloc(static_cast<size_t>(length)));
    *size = ::fread(*data, 1, static_cast<size_t>(length), file);
    ::fclose(file);
    return *size == static_cast<size_t>(length);
}

} // anonymous namespace

int main(int argc, char** argv) {
    if (argc > 2 ||
        (argc == 2 && ::strcmp(argv[1], "helix") != 0 &&
         ::strcmp(argv[1], "minimp3-float") != 0)) {
        return 1;
    }
    unsigned char* fixture = nullptr;
    size_t fixture_size = 0;
    if (!loadFixture(&fixture, &fixture_size)) {
        return 1;
    }
    void* raw_stack = nullptr;
    if (posix_memalign(&raw_stack, 4096, STACK_BYTES) != 0) {
        return 1;
    }
    unsigned char* stack = static_cast<unsigned char*>(raw_stack);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setguardsize(&attr, 0);
    pthread_attr_setstack(&attr, stack, STACK_BYTES);

    const char* names[] = {"helix", "minimp3-float"};
    for (int i = 0; i < 2; ++i) {
        if (argc == 2 && ::strcmp(argv[1], names[i]) != 0) {
            continue;
        }
        DecodeContext context = {
            fixture, fixture_size, stack, i == 1, false, 0
        };
        pthread_t thread;
        if (pthread_create(&thread, &attr, decodeThread, &context) != 0) {
            return 1;
        }
        pthread_join(thread, nullptr);
        ::printf("WATERMARK:backend=%s decode=%zu\n",
                 names[i], context.stack_usage);
        if (!context.ok) {
            return 1;
        }
    }

    pthread_attr_destroy(&attr);
    ::free(stack);
    ::free(fixture);
    return 0;
}
