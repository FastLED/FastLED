#pragma once

/// @file fl/stl/scratch.h
/// @brief fl::scratch<T>: large scratch buffers that stay off small stacks.
///
/// `fl::scratch<T>` holds one T for the lifetime of a scope. Where it lives
/// is a build-wide choice made by `FL_SCRATCH_STACK_BYTES`:
///
///  - `0` (default on most targets): `T` is a plain member, so a local
///    `fl::scratch<T>` is exactly a local `T` on the call stack. Zero
///    overhead; `operator bool` is always true.
///  - `N > 0`: `T` is bump-allocated from one fixed, N-byte static arena (a
///    "side stack"). Peak RAM is the deepest nesting of live scratches, not
///    the sum over every template instantiation as with function `static`s.
///    The arena never grows. When it is exhausted the constructor leaves the
///    object empty: `operator bool` is false and the caller must skip the
///    work (`FL_WARN` reports it on builds that log warnings).
///
/// Use it for multi-hundred-byte working sets in code reachable from targets
/// with small stacks (`FL_PLATFORM_HAS_SMALL_STACK`), e.g. a rasteriser's
/// edge tables. Small locals should stay plain locals.
///
/// @code
///   struct Frame { fl::i32 acc[64]; fl::u8 edges[32]; };
///   void draw() {
///       fl::scratch<Frame> f;
///       if (!f) return;          // side stack exhausted: skip this call
///       f->acc[0] = 0;
///   }
/// @endcode
///
/// Rules:
///  - Scoped use only. The type is neither copyable nor movable, and
///    releases must be LIFO (debug builds assert on a non-LIFO release,
///    which only happens if a scratch is heap-allocated or otherwise
///    outlives a younger one). Release builds do not assert, except on
///    Emscripten, whose FL_ASSERT is always active (it logs and breaks into
///    the debugger). Either way a non-LIFO release leaves the side stack's
///    top in place, so the released bytes (and any older frame released
///    beneath them) stay reserved for the arena's lifetime. Deliberate
///    (#5327): it never hands live memory out twice, and recovery would need
///    bookkeeping in the inlined release path on the small-stack targets this
///    exists for. A leak shrinks the capacity left for later leases, so a
///    lease larger than what remains comes back empty.
///  - `T` must be trivially destructible (no destructor is ever run).
///  - Not ISR-safe and not thread-safe: never use it from an interrupt
///    handler. A non-zero capacity is rejected on multithreaded builds
///    (`FASTLED_MULTITHREADED`) unless `FL_SCRATCH_ALLOW_MULTITHREADED=1`
///    asserts that only one thread ever touches fl::scratch.
///  - Set `FL_SCRATCH_STACK_BYTES` build-wide (a -D compiler flag), never
///    with a `#define` in one source file, so every translation unit agrees
///    on what fl::scratch is.
///
/// Platform defaults:
///  - ESP8266: 4608 B. Arduino's loop() runs on a fixed 4 KB "cont" stack,
///    while ~80 KB of DRAM leaves room in .bss. 4.5 KB holds drawPolygon's
///    32/64 Exact working set (fl::gfx::detail::PolyScratch<32, 64>: 4208 B,
///    8-aligned, on ESP8266), which makes 32/64 its default polygon tier,
///    plus ~400 B for a small nested user. The arena is only linked in when
///    something uses fl::scratch (`--gc-sections`).
///
///    Budget for new fl::scratch users on ESP8266 (#5325): drawPolygon's
///    32/64 lease reserves up to FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES
///    (4215 B, padding included), leaving 393 B. All leases that are live
///    while drawPolygon runs (including one that calls into it) share that
///    393 B: the sum of (sizeof(T) + alignof(T) - 1) over them must not
///    exceed it, or the draw is skipped. Exhaustion warns (FL_WARN_ONCE)
///    at most once per run and only when warnings are enabled, so a skip
///    may be silent. A lease that is released before drawPolygon runs may
///    use the whole 4608 B. drawPolygon is currently the only fl::scratch
///    user. The default is not raised: every extra byte is .bss taken from
///    every ESP8266 sketch that draws polygons. A sketch that needs more
///    nesting passes -DFL_SCRATCH_STACK_BYTES=N, or a smaller explicit
///    polygon tier such as drawPolygon<int, 12>, which draws at most 12
///    vertices and truncates longer polygons to the first 12. Check
///    fl::scratch_high_water() to confirm.
///  - Everything else: 0. That includes the other small-stack tiers (AVR,
///    LPC804, LPC11xx, Teensy LC, nRF51, STM32G03x, low-density STM32F1).
///    Decided per target in #5326; none gets a non-zero default. Unlike
///    ESP8266, where loop() runs on a fixed 4 KB stack beside ~80 KB of
///    otherwise idle DRAM, on these the stack is whatever SRAM is left
///    below .data/.bss/heap. Moving a working set from the stack into an
///    arena therefore frees no memory: it turns a transient frame into a
///    permanent .bss reservation, held even while nothing draws. The arena
///    is still the right tool when a sketch's RAM map has room for it and
///    its deepest call chain does not, so these targets opt in build-wide:
///      -DFL_SCRATCH_STACK_BYTES=N   (e.g. build_flags in platformio.ini)
///    Size N to the sum of (sizeof(T) + alignof(T) - 1) over the leases live
///    at once (drawPolygon's default 8/16 tier on these targets: about 1 KB
///    for Exact, 0.6 KB for ABuffer; see fl/gfx/canvas.h), then confirm with
///    fl::scratch_high_water(). N >= FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES
///    (4215) also raises the default polygon tier to 32/64, which only fits
///    parts with well over 4 KB of free SRAM (e.g. a 32 KB nRF51 or a
///    10 KB STM32F103x6 with a small canvas, not an Uno or a Teensy LC).

#include "fl/stl/scratch_config.h"  // FL_SCRATCH_STACK_BYTES default
#include "fl/stl/thread_config.h"
#include "fl/stl/int.h"
#include "fl/stl/new.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/align.h"
#include "fl/stl/assert.h"
#include "fl/stl/compiler_control.h"
#include "fl/log/log.h"

#if FL_SCRATCH_STACK_BYTES > 0 && FASTLED_MULTITHREADED && !FL_SCRATCH_ALLOW_MULTITHREADED
#error "FL_SCRATCH_STACK_BYTES > 0 needs a single-threaded build: the side stack is one shared, unlocked arena. Set FL_SCRATCH_ALLOW_MULTITHREADED=1 only if a single thread uses fl::scratch."
#endif

namespace fl {
namespace detail {

/// Mutable state of a side stack. All-zero is the empty stack, so a global
/// instance lives in .bss (no .data copy-in).
struct side_stack_state {
    fl::size top;
    fl::size high_water;
};

/// Reserve `bytes` aligned to `align` (a power of two, measured on the
/// absolute address) from the `capacity`-byte arena at `base`. On success
/// stores the previous top in *saved_top and returns the block; on
/// exhaustion returns nullptr and changes nothing. Inline so that a
/// compile-time base/capacity/size/alignment folds to a few instructions.
FASTLED_FORCE_INLINE void* side_stack_push(side_stack_state& st, u8* base, fl::size capacity,
                             fl::size bytes, fl::size align,
                             fl::size* saved_top) FL_NO_EXCEPT {
    const fl::size top = st.top;
    // reinterpret_cast, not fl::bit_cast: the latter lowers to an
    // out-of-line memcpy call on AVR.
    const fl::uptr here = reinterpret_cast<fl::uptr>(base) + top;  // ok reinterpret cast
    const fl::size pad =
        static_cast<fl::size>((fl::uptr(0) - here) & fl::uptr(align - 1));
    // Subtract rather than add: no fl::size overflow for huge requests.
    if (pad > capacity - top || bytes > capacity - top - pad) {
        return nullptr;
    }
    const fl::size end = top + pad + bytes;
    *saved_top = top;
    st.top = end;
    if (end > st.high_water) {
        st.high_water = end;
    }
    return base + (end - bytes);
}

/// Release a block from side_stack_push(): the top returns to saved_top.
/// Returns false if `block` was not the most recent live block (a non-LIFO
/// release, a contract violation fl::scratch asserts on in debug builds).
/// The top is then left alone, because lowering it would hand still-live
/// younger blocks out again. The cost is a leak instead of an overlap: the
/// released bytes, and anything below them released later, can stay
/// reserved for the life of the arena (later releases of blocks under the
/// leaked range also report false). Deliberate: recovering leaked bytes is
/// not implemented (#5327); see the Rules in the file header.
FASTLED_FORCE_INLINE bool side_stack_pop(side_stack_state& st, const u8* base,
                           fl::size saved_top, const void* block,
                           fl::size bytes) FL_NO_EXCEPT {
    const bool lifo = static_cast<const u8*>(block) + bytes == base + st.top;
    if (lifo) {
        st.top = saved_top;
    }
    return lifo;
}

/// LIFO bump allocator over a caller-provided byte range: the same
/// side_stack_push()/side_stack_pop() core fl::scratch runs on its global
/// arena, packaged as an object so tests can use small arenas of their own.
/// It borrows `base` (no ownership): the buffer must outlive the object.
class side_stack {
  public:
    side_stack(u8* base, fl::size capacity) FL_NO_EXCEPT
        : mBase(base), mCapacity(capacity) {
        mState.top = 0;
        mState.high_water = 0;
    }

    void* push(fl::size bytes, fl::size align, fl::size* saved_top) FL_NO_EXCEPT {
        return side_stack_push(mState, mBase, mCapacity, bytes, align, saved_top);
    }
    bool pop(fl::size saved_top, const void* block, fl::size bytes) FL_NO_EXCEPT {
        return side_stack_pop(mState, mBase, saved_top, block, bytes);
    }

    fl::size capacity() const FL_NO_EXCEPT { return mCapacity; }
    fl::size used() const FL_NO_EXCEPT { return mState.top; }
    fl::size high_water() const FL_NO_EXCEPT { return mState.high_water; }
    void reset_high_water() FL_NO_EXCEPT { mState.high_water = mState.top; }

  private:
    u8* mBase;
    fl::size mCapacity;
    side_stack_state mState;
};

/// Cold path of an exhausted global side stack: FL_WARN_ONCE where warnings
/// are compiled in (an exhausted render path would otherwise warn every
/// frame). Kept out of line so the hot path stays small; empty on builds
/// without warnings.
FL_NO_INLINE inline void scratch_exhausted(fl::size bytes) FL_NO_EXCEPT {
    FL_UNUSED(bytes);
    FL_WARN_ONCE("fl::scratch: side stack exhausted (" << bytes
                 << " B requested); raise FL_SCRATCH_STACK_BYTES");
}

/// Arena policy for fl::scratch: one global, N-byte side stack.
/// fl::scratch uses global_scratch_arena<FL_SCRATCH_STACK_BYTES>. The
/// storage is a template static, so it is only emitted where some
/// fl::scratch actually uses it (and --gc-sections drops it otherwise); the
/// library carries no capacity-specific definitions, and tests can run the
/// production arena at their own N without redefining the macro.
template <fl::size N>
struct global_scratch_arena {
    struct FL_ALIGN_MAX storage {
        u8 bytes[N];
        side_stack_state state;  // zero-initialised: the whole object is .bss
    };
    static storage sStorage;

    FASTLED_FORCE_INLINE static void* acquire(fl::size bytes, fl::size align,
                                              fl::size* saved_top) FL_NO_EXCEPT {
        void* p = side_stack_push(sStorage.state, sStorage.bytes, N, bytes,
                                  align, saved_top);
        if (!p) {
            scratch_exhausted(bytes);
        }
        return p;
    }
    FASTLED_FORCE_INLINE static void release(fl::size saved_top, const void* block,
                                             fl::size bytes) FL_NO_EXCEPT {
        const bool lifo = side_stack_pop(sStorage.state, sStorage.bytes,
                                         saved_top, block, bytes);
        FL_UNUSED(lifo);  // FL_ASSERT compiles to nothing in release builds (not on Emscripten)
        FL_ASSERT(lifo, "fl::scratch released out of LIFO order");
    }
    static fl::size used() FL_NO_EXCEPT { return sStorage.state.top; }
    static fl::size high_water() FL_NO_EXCEPT { return sStorage.state.high_water; }
    static void reset_high_water() FL_NO_EXCEPT {
        sStorage.state.high_water = sStorage.state.top;
    }
    static u8* data() FL_NO_EXCEPT { return sStorage.bytes; }
};

template <fl::size N>
typename global_scratch_arena<N>::storage global_scratch_arena<N>::sStorage;

template <typename T>
struct scratch_holder {
    T value;
};

/// RAII lease of one T from an arena policy `Arena` providing static
/// acquire()/release() (see global_scratch_arena). This is fl::scratch's
/// implementation when FL_SCRATCH_STACK_BYTES > 0; tests plug in their own
/// arena to exercise it on host builds.
template <typename T, typename Arena>
class arena_scratch {
    FL_STATIC_ASSERT(fl::is_trivially_destructible<T>::value,
                  "fl::scratch<T> requires a trivially destructible T");

  public:
    FASTLED_FORCE_INLINE arena_scratch() FL_NO_EXCEPT : mPtr(nullptr), mSavedTop(0) {
        void* p = Arena::acquire(sizeof(T), alignof(T), &mSavedTop);
        if (p) {
            mPtr = &(::new (p) scratch_holder<T>)->value;
        }
    }
    FASTLED_FORCE_INLINE ~arena_scratch() FL_NO_EXCEPT {
        if (mPtr) {
            Arena::release(mSavedTop, mPtr, sizeof(T));
        }
    }
    arena_scratch(const arena_scratch&) FL_NO_EXCEPT = delete;
    arena_scratch& operator=(const arena_scratch&) FL_NO_EXCEPT = delete;

    /// False only when the arena was exhausted; the object is then empty
    /// and must not be dereferenced.
    explicit operator bool() const FL_NO_EXCEPT { return mPtr != nullptr; }
    T* get() FL_NO_EXCEPT { return mPtr; }
    const T* get() const FL_NO_EXCEPT { return mPtr; }
    T& operator*() FL_NO_EXCEPT { return *mPtr; }
    const T& operator*() const FL_NO_EXCEPT { return *mPtr; }
    T* operator->() FL_NO_EXCEPT { return mPtr; }
    const T* operator->() const FL_NO_EXCEPT { return mPtr; }

  private:
    T* mPtr;
    fl::size mSavedTop;
};

/// fl::scratch's implementation when FL_SCRATCH_STACK_BYTES == 0: the T is
/// a plain member, so a local scratch is exactly a local T.
template <typename T>
class inline_scratch {
    FL_STATIC_ASSERT(fl::is_trivially_destructible<T>::value,
                  "fl::scratch<T> requires a trivially destructible T");

  public:
    inline_scratch() FL_NO_EXCEPT {}
    inline_scratch(const inline_scratch&) FL_NO_EXCEPT = delete;
    inline_scratch& operator=(const inline_scratch&) FL_NO_EXCEPT = delete;

    explicit operator bool() const FL_NO_EXCEPT { return true; }
    T* get() FL_NO_EXCEPT { return &mValue; }
    const T* get() const FL_NO_EXCEPT { return &mValue; }
    T& operator*() FL_NO_EXCEPT { return mValue; }
    const T& operator*() const FL_NO_EXCEPT { return mValue; }
    T* operator->() FL_NO_EXCEPT { return &mValue; }
    const T* operator->() const FL_NO_EXCEPT { return &mValue; }

  private:
    T mValue;
};

} // namespace detail

#if FL_SCRATCH_STACK_BYTES == 0

/// Peak side-stack usage in bytes since start-up (or the last reset).
/// Always 0 when FL_SCRATCH_STACK_BYTES is 0.
inline fl::size scratch_high_water() FL_NO_EXCEPT { return 0; }
inline void scratch_reset_high_water() FL_NO_EXCEPT {}

template <typename T>
class scratch : public detail::inline_scratch<T> {};

#else  // FL_SCRATCH_STACK_BYTES > 0

/// Peak side-stack usage in bytes since start-up (or the last reset).
inline fl::size scratch_high_water() FL_NO_EXCEPT {
    return detail::global_scratch_arena<FL_SCRATCH_STACK_BYTES>::high_water();
}
/// Lower the high-water mark to the current usage (for measurements/tests).
inline void scratch_reset_high_water() FL_NO_EXCEPT {
    detail::global_scratch_arena<FL_SCRATCH_STACK_BYTES>::reset_high_water();
}

template <typename T>
class scratch
    : public detail::arena_scratch<
          T, detail::global_scratch_arena<FL_SCRATCH_STACK_BYTES>> {
    // Worst-case alignment padding included, so a T that passes can always
    // be leased from an empty side stack.
    FL_STATIC_ASSERT(sizeof(T) + alignof(T) - 1 <= FL_SCRATCH_STACK_BYTES,
                  "fl::scratch<T>: T (plus alignment padding) is larger than FL_SCRATCH_STACK_BYTES");

  public:
    // Spelled out so they inline like the base's: an implicit destructor
    // was emitted out of line on avr-gcc -Os (+48 B per T).
    FASTLED_FORCE_INLINE scratch() FL_NO_EXCEPT {}
    FASTLED_FORCE_INLINE ~scratch() FL_NO_EXCEPT {}
};

#endif  // FL_SCRATCH_STACK_BYTES

} // namespace fl
