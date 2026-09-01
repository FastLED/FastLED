#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fl/codec/mp3_memory.h"

// Pinned to the float variant: this rig links minimp3_audit.o, which is pinned
// the same way, and the two must agree on mp3dec_scratch_t's size. The
// fixed-point build's decode stack is covered by the gated, compiler-derived
// `stack-callgraph` metric rather than by this observation -- see the ledger.
#define MINIMP3_FLOAT_POINT 1
#include "third_party/minimp3/minimp3.h"
#undef MINIMP3_FLOAT_POINT

namespace {

void* volatile gAllocationWitness = nullptr;

} // anonymous namespace

/* These are the block-move primitives the decoder itself calls, and they are
   deliberately plain loops rather than forwarders to libc.

   That is the fix for FastLED#4106. Forwarding to glibc put its `mem*`
   implementation's frames inside the measured window, and those frames are
   large, and they differ between glibc builds and CPU dispatch paths -- which
   is why the metric landed on exact-but-different answers on different CI
   runners while the decoder code was byte-identical. Measured directly: with
   glibc the watermark reads 2952 here, with these loops 2056, and the ~900-byte
   difference is entirely glibc's. It also made the watermark disagree with the
   compiler-derived callgraph figure by 664 bytes; with the loops the two
   independent methods agree to within 24 bytes, which is the audit call
   boundary.

   Measuring glibc was also measuring the wrong thing. The 2 KiB budget this
   audit enforces is an MCU budget, and no MCU links glibc's vectorised
   `memcpy` -- newlib's is a word loop much like these.

   The pointers are `volatile` so the compiler cannot recognise the loop idiom
   and call `memcpy`/`memset` right back, silently restoring the variance. That
   is a guarantee from the standard rather than a build flag someone can drop.
   Slow is fine: nothing here is timed. */
namespace fl {

void* memcpy(void* dst, const void* src, fl::size n) FL_NO_EXCEPT {
    volatile unsigned char* d = static_cast<volatile unsigned char*>(dst);
    const volatile unsigned char* s =
        static_cast<const volatile unsigned char*>(src);
    for (fl::size i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dst;
}

void* memmove(void* dst, const void* src, fl::size n) FL_NO_EXCEPT {
    volatile unsigned char* d = static_cast<volatile unsigned char*>(dst);
    const volatile unsigned char* s =
        static_cast<const volatile unsigned char*>(src);
    if (d < s) {
        for (fl::size i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (fl::size i = n; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dst;
}

void* memset(void* dst, int value, fl::size n) FL_NO_EXCEPT {
    volatile unsigned char* d = static_cast<volatile unsigned char*>(dst);
    for (fl::size i = 0; i < n; ++i) {
        d[i] = static_cast<unsigned char>(value);
    }
    return dst;
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

/* Releases one tagged allocation on every exit path. The audit runs this binary
   under Massif and asserts the codec peak equals the accounting hook's peak, so
   a leak on the error path would not merely leak -- it would corrupt the
   measurement the ledger is built from. Written as a scope guard rather than
   repeated frees because the early return below has to unwind whatever was
   already allocated. */
class TaggedAllocation {
public:
    TaggedAllocation(size_t bytes, fl::third_party::Mp3MemoryTag tag)
        : mBytes(bytes), mTag(tag),
          mPtr(fl::third_party::Mp3MemoryAllocate(bytes, tag)) {}

    ~TaggedAllocation() {
        if (mPtr) {
            fl::third_party::Mp3MemoryFree(mPtr, mBytes, mTag);
        }
    }

    void* get() const { return mPtr; }

private:
    TaggedAllocation(const TaggedAllocation&);
    TaggedAllocation& operator=(const TaggedAllocation&);

    size_t mBytes;
    fl::third_party::Mp3MemoryTag mTag;
    void* mPtr;
};

__attribute__((noinline))
void* decodeThread(void* opaque) {
    DecodeContext* context = static_cast<DecodeContext*>(opaque);
    const size_t stream_bytes = fl::third_party::MP3_MINIMP3_STREAM_BUFFER_SIZE;
    TaggedAllocation stream(stream_bytes,
                            fl::third_party::Mp3MemoryTag::StreamBuffer);
    TaggedAllocation decoder_alloc(
        sizeof(fl::third_party::mp3dec_t),
        fl::third_party::Mp3MemoryTag::DecoderState);
    TaggedAllocation scratch_alloc(
        sizeof(fl::third_party::mp3dec_scratch_t),
        fl::third_party::Mp3MemoryTag::Scratch);
    TaggedAllocation pcm_alloc(2304 * sizeof(int16_t),
                               fl::third_party::Mp3MemoryTag::PcmOutput);
    if (!stream.get() || !decoder_alloc.get() || !scratch_alloc.get() ||
        !pcm_alloc.get()) {
        return nullptr;
    }

    fl::third_party::mp3dec_t* const decoder =
        static_cast<fl::third_party::mp3dec_t*>(decoder_alloc.get());
    fl::third_party::mp3dec_scratch_t* const scratch =
        static_cast<fl::third_party::mp3dec_scratch_t*>(scratch_alloc.get());
    int16_t* const pcm = static_cast<int16_t*>(pcm_alloc.get());

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
    if (argc > 2 || (argc == 2 && ::strcmp(argv[1], "minimp3-float") != 0)) {
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

    DecodeContext context = {fixture, fixture_size, stack, false, 0};
    pthread_t thread;
    if (pthread_create(&thread, &attr, decodeThread, &context) != 0) {
        return 1;
    }
    pthread_join(thread, nullptr);
    ::printf("WATERMARK:backend=minimp3-float decode=%zu\n",
             context.stack_usage);
    if (!context.ok) {
        return 1;
    }

    pthread_attr_destroy(&attr);
    ::free(stack);
    ::free(fixture);
    return 0;
}
