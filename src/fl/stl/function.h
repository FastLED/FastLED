#pragma once
#include "fl/stl/shared_ptr.h"  // For shared_ptr (heap fallback for over-SBO lambdas)
#include "fl/stl/type_traits.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/cstring.h"
#include "fl/stl/new.h"   // for placement new operator  // IWYU pragma: keep
#include "fl/stl/align.h"
#include "fl/stl/pair.h"
#include "fl/stl/vector.h"
#include "fl/stl/algorithm.h"  // for fl::sort
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/system/sketch_macros.h"  // for FL_PLATFORM_HAS_LARGE_MEMORY

#ifndef FASTLED_INLINE_LAMBDA_SIZE
  // Small-buffer-optimization size for stored callables. 64 B fits every
  // production lambda observed across the codebase audit; larger captures
  // hit a static_assert in init_with(). User overridable.
  #define FASTLED_INLINE_LAMBDA_SIZE 64
#endif

// `FL_FUNCTION_NO_HEAP_FALLBACK` opt-out (FastLED #3237):
// When defined, constructing an `fl::function` from a callable whose
// captured state exceeds `FASTLED_INLINE_LAMBDA_SIZE` triggers a
// compile-time error instead of silently routing through the
// `HeapHolder<F>` + `shared_ptr<F>` heap fallback. Memory-constrained
// users who cannot tolerate the heap allocation can `#define` this
// before including FastLED headers; the compile failure points to the
// over-SBO call site with a suggestion to either reduce the capture or
// bump `FASTLED_INLINE_LAMBDA_SIZE`. The default behaviour (heap
// fallback via `shared_ptr<F>`) is preserved when the macro is not
// defined -- see the #3237 investigation comment for the audit and
// decision rationale.

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

//----------------------------------------------------------------------------
// is_function_pointer trait - detects function pointers like R(*)(Args...)
//----------------------------------------------------------------------------
template <typename T> struct is_function_pointer {
    static constexpr bool value = false;
};

template <typename R, typename... Args>
struct is_function_pointer<R(*)(Args...)> {
    static constexpr bool value = true;
};

//----------------------------------------------------------------------------
// function<R(Args...)>: type-erasing "std::function" replacement
//
// Design (FastLED #3235 Tier 1 item B, "Option B"):
//   Single small-buffer storage + two function pointers (invoker + manager).
//   The previous 5-alternative `fl::variant<>` based design emitted ~25 thunks
//   per signature; this collapses that to a single invoker pointer set at
//   construction and a single manager pointer that multiplexes copy/move/
//   destroy through an Op enum. Estimated cumulative flash savings: 25-35 KB
//   on Large-memory builds (audited across ~25-30 distinct signatures in
//   src/ + examples/).
//
// Supports free functions, lambdas/functors, and member functions (const &
// non-const) -- the public API is unchanged from the legacy variant-based
// implementation. Member-function support is provided via a thin wrapper
// stored in the SBO; the legacy MemberCallableBase / NonConstMemberCallable /
// ConstMemberCallable class hierarchy goes away.
//----------------------------------------------------------------------------
template <typename> class function;

template <typename R, typename... Args>
class FL_ALIGN function<R(Args...)> {
private:
    static constexpr fl::size kSboSize = FASTLED_INLINE_LAMBDA_SIZE;

    // Multiplexed callable operations -- one entry per phase of the storage's
    // lifecycle. Routed through a single Manager fn ptr to avoid having to
    // emit separate copy/move/destroy thunks per signature.
    enum class Op : fl::u8 { Destroy, Copy, Move };

    // Invoker: knows how to call the stored callable with (Args...).
    using Invoker = R (*)(const void* src, Args...);
    // Manager: multiplexes copy/move/destroy. The semantics by op:
    //   Op::Destroy: invoke ~F() on the object at `dst` (storage bytes).
    //                `src` ignored.
    //   Op::Copy:    copy-construct *this from *other.
    //                `dst` = this->mBytes, `src` = other.mBytes.
    //   Op::Move:    move-construct *this from *other (and leave *other in
    //                a valid-but-unspecified state; reset() handles teardown).
    //                `dst` = this->mBytes, `src` = other.mBytes.
    using Manager = void (*)(void* dst, void* src, Op op);

    // SBO + bookkeeping
    FL_ALIGN_MAX char mBytes[kSboSize];
    Invoker mInvoker;
    Manager mManager;
    bool mHasValue;

    // ---- Helpers: default-construct R for the "empty function called" path ---
    // void R needs special handling because `return R{};` doesn't compile when
    // R is void. Tag-dispatch on is_void<R>.
    template <typename R2>
    static typename enable_if<!is_void<R2>::value, R2>::type
    null_return_impl() FL_NOEXCEPT { return R2{}; }

    template <typename R2>
    static typename enable_if<is_void<R2>::value, R2>::type
    null_return_impl() FL_NOEXCEPT { return; }

    static R null_invoker(const void* /*src*/, Args... /*args*/) FL_NOEXCEPT {
        return null_return_impl<R>();
    }
    static void null_manager(void* /*dst*/, void* /*src*/, Op /*op*/) FL_NOEXCEPT {}

    // ---- Per-F invoker --------------------------------------------------------
    // Copies F out of storage so that mutable lambdas remain callable through a
    // const operator(). The previous variant-based implementation used the same
    // pattern.
    template <typename Func>
    static R functor_invoker(const void* src, Args... args) FL_NOEXCEPT {
        FL_ALIGN_AS(Func) char tmp[sizeof(Func)];
        fl::memcpy(tmp, src, sizeof(Func));
        Func* f = static_cast<Func*>(static_cast<void*>(tmp));
        return (*f)(args...);
    }

    // ---- Trivial manager: F is trivially copyable; memcpy semantics. ----------
    // Shared across all trivially-copyable types of the same size, so the
    // linker collapses many F's into one body (per-size, not per-F).
    template <fl::size N>
    static void trivial_manager(void* dst, void* src, Op op) FL_NOEXCEPT {
        switch (op) {
            case Op::Destroy:
                // Trivially destructible: nothing to do.
                break;
            case Op::Copy:
            case Op::Move:
                fl::memcpy(dst, src, N);
                break;
        }
    }

    // ---- Non-trivial manager: real placement-new + destructor. ----------------
    template <typename Func>
    static void non_trivial_manager(void* dst, void* src, Op op) FL_NOEXCEPT {
        switch (op) {
            case Op::Destroy: {
                Func* f = static_cast<Func*>(dst);
                f->~Func();
                break;
            }
            case Op::Copy: {
                const Func* sf = static_cast<const Func*>(static_cast<const void*>(src));
                new (dst) Func(*sf);
                break;
            }
            case Op::Move: {
                Func* sf = static_cast<Func*>(src);
                new (dst) Func(fl::move(*sf));
                break;
            }
        }
    }

    // ---- Heap fallback: shared_ptr-wrapping holder for over-SBO callables. ---
    // When the user passes a callable bigger than kSboSize, we wrap it in a
    // `HeapHolder<Func>` and store *that* in the SBO. The holder is small
    // (sizeof(shared_ptr<Func>) ~= 8-16 B) and its operator() dispatches through
    // the shared_ptr to call the heap-allocated F. Copy/move/destroy of
    // HeapHolder are handled by shared_ptr's own refcount machinery, so the
    // non_trivial_manager template handles it cleanly.
    template <typename Func>
    struct HeapHolder {
        fl::shared_ptr<Func> ptr;
        R operator()(Args... args) const FL_NOEXCEPT {
            return (*ptr)(args...);
        }
    };

    // ---- init_with: place F into storage, wire up invoker/manager ------------
    // Tag-dispatches between in-SBO storage (when sizeof(F) fits) and the
    // HeapHolder fallback for larger callables. Both paths go through the same
    // functor_invoker / manager dispatch.
    template <typename Func>
    void init_with(Func&& f) FL_NOEXCEPT {
        using FBare = typename remove_reference<Func>::type;
        FL_STATIC_ASSERT(alignof(FBare) <= alignof(max_align_t),
                         "Callable requires stricter alignment than SBO provides.");
        init_with_impl(fl::forward<Func>(f),
                       integral_constant<bool, (sizeof(FBare) <= kSboSize)>{});
    }

    template <typename Func>
    void init_with_impl(Func&& f, true_type /* fits in SBO */) FL_NOEXCEPT {
        using FBare = typename remove_reference<Func>::type;
        new (mBytes) FBare(fl::forward<Func>(f));
        mInvoker = &functor_invoker<FBare>;
        mManager = is_trivially_copyable<FBare>::value
                        ? &trivial_manager<sizeof(FBare)>
                        : &non_trivial_manager<FBare>;
        mHasValue = true;
    }

    template <typename Func>
    void init_with_impl(Func&& f, false_type /* over-SBO; use HeapHolder */) FL_NOEXCEPT {
        using FBare = typename remove_reference<Func>::type;
#ifdef FL_FUNCTION_NO_HEAP_FALLBACK
        // Opt-out for memory-constrained users (FastLED #3237). The default
        // heap-fallback path is suppressed and over-SBO callables fail at
        // compile time. Suggested fixes are baked into the message.
        (void)f;
        FL_STATIC_ASSERT(sizeof(FBare) <= kSboSize,
                         "fl::function: capture exceeds FASTLED_INLINE_LAMBDA_SIZE "
                         "and FL_FUNCTION_NO_HEAP_FALLBACK is defined. "
                         "Either reduce the captured state, or bump "
                         "FASTLED_INLINE_LAMBDA_SIZE for this build, or "
                         "undef FL_FUNCTION_NO_HEAP_FALLBACK to re-enable "
                         "the shared_ptr-backed heap fallback.");
#else
        using Holder = HeapHolder<FBare>;
        FL_STATIC_ASSERT(sizeof(Holder) <= kSboSize,
                         "shared_ptr too large for SBO; bump FASTLED_INLINE_LAMBDA_SIZE.");
        new (mBytes) Holder{fl::make_shared<FBare>(fl::forward<Func>(f))};
        mInvoker = &functor_invoker<Holder>;
        // shared_ptr is not trivially copyable (its copy ctor touches the
        // refcount), so always use the non_trivial_manager path here.
        mManager = &non_trivial_manager<Holder>;
        mHasValue = true;
#endif
    }

    // Reset to empty state; calls destructor on the stored callable if any.
    void reset() FL_NOEXCEPT {
        if (mHasValue) {
            mManager(mBytes, nullptr, Op::Destroy);
            mHasValue = false;
        }
        mInvoker = &null_invoker;
        mManager = &null_manager;
    }

    // ---- Member-function wrappers (stored in the SBO like any other functor) -
    // Each member-fn constructor packages (obj, mf) into a wrapper struct that
    // the SBO path treats as a regular callable. The struct is at class scope
    // so it has external linkage and template instantiation rules are clean.
    template <typename C>
    struct NonConstMemberWrapper {
        C* obj;
        R (C::*mf)(Args...);
        R operator()(Args... args) const FL_NOEXCEPT {
            return (obj->*mf)(args...);
        }
    };

    template <typename C>
    struct ConstMemberWrapper {
        const C* obj;
        R (C::*mf)(Args...) const;
        R operator()(Args... args) const FL_NOEXCEPT {
            return (obj->*mf)(args...);
        }
    };

public:
    function() FL_NOEXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        // Initialize storage to zero so a copy of an empty function doesn't
        // memcpy uninitialized memory. (Matches the legacy behavior.)
        fl::memset(mBytes, 0, kSboSize);
    }

    // Copy constructor
    function(const function& other) FL_NOEXCEPT
        : mInvoker(other.mInvoker), mManager(other.mManager), mHasValue(other.mHasValue) {
        fl::memset(mBytes, 0, kSboSize);
        if (mHasValue) {
            // other's bytes are logically const here, but the Manager signature
            // takes void* for the unified copy/move/destroy contract. Casting
            // away const is safe because Op::Copy reads only from src.
            mManager(mBytes, const_cast<char*>(other.mBytes), Op::Copy);
        }
    }

    // Move constructor
    function(function&& other) FL_NOEXCEPT
        : mInvoker(other.mInvoker), mManager(other.mManager), mHasValue(other.mHasValue) {
        fl::memset(mBytes, 0, kSboSize);
        if (mHasValue) {
            mManager(mBytes, other.mBytes, Op::Move);
            // Destroy the moved-from object's storage and leave *other empty.
            other.reset();
        }
    }

    // Copy assignment
    function& operator=(const function& other) FL_NOEXCEPT {
        if (this == &other) return *this;
        reset();
        mInvoker = other.mInvoker;
        mManager = other.mManager;
        mHasValue = other.mHasValue;
        if (mHasValue) {
            mManager(mBytes, const_cast<char*>(other.mBytes), Op::Copy);
        }
        return *this;
    }

    // Move assignment
    function& operator=(function&& other) FL_NOEXCEPT {
        if (this == &other) return *this;
        reset();
        mInvoker = other.mInvoker;
        mManager = other.mManager;
        mHasValue = other.mHasValue;
        if (mHasValue) {
            mManager(mBytes, other.mBytes, Op::Move);
            other.reset();
        }
        return *this;
    }

    ~function() FL_NOEXCEPT {
        reset();
    }

    // 1) Free function pointer
    function(R (*fp)(Args...)) FL_NOEXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        if (fp) {
            init_with(fp);
        }
    }

    // 2) Lambda / functor (SFINAE excludes fn ptrs, member fn ptrs, and self
    //    to avoid hijacking copy/move construction)
    template <typename Func,
              typename = enable_if_t<!is_member_function_pointer<Func>::value &&
                                     !is_function_pointer<Func>::value &&
                                     !is_same<typename remove_reference<Func>::type, function>::value>>
    function(Func f) FL_NOEXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(fl::move(f));
    }

    // 3) Non-const member function
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) FL_NOEXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(NonConstMemberWrapper<C>{obj, mf});
    }

    // 4) Const member function
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) FL_NOEXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(ConstMemberWrapper<C>{obj, mf});
    }

    R operator()(Args... args) const FL_NOEXCEPT {
        return mInvoker(mBytes, args...);
    }

    explicit operator bool() const FL_NOEXCEPT { return mHasValue; }

    void clear() FL_NOEXCEPT { reset(); }

    bool operator==(const function& o) const FL_NOEXCEPT {
        // Mirrors legacy semantics: just empty / non-empty equality.
        return mHasValue == o.mHasValue;
    }

    bool operator!=(const function& o) const FL_NOEXCEPT { return !(*this == o); }
};

//----------------------------------------------------------------------------
// function_list: Container for managing multiple callbacks with add/remove
//----------------------------------------------------------------------------

// Primary template declaration with static assertion for invalid usage
// Only specializations for function signatures (e.g., function_list<void(Args...)>) are valid
template <typename T>
class function_list {
    FL_STATIC_ASSERT(fl::is_same<T, void>::value && !fl::is_same<T, void>::value,
                  "function_list requires a void returning function signature.");
};

// Partial specialization for function signature syntax: function_list<void(Args...)>
// Supports: function_list<void()>, function_list<void(float)>, function_list<void(u8, float, float)>, etc.
template <typename... Args>
class function_list<void(Args...)> {
  private:
    using FunctionType = function<void(Args...)>;

    struct FunctionEntry {
        int id;
        int priority;
        FunctionType fn;

        FunctionEntry() FL_NOEXCEPT : id(0), priority(0), fn() {}
        FunctionEntry(int idParam, int priorityParam, FunctionType fnParam) FL_NOEXCEPT
            : id(idParam), priority(priorityParam), fn(fnParam) {}
    };

    using FunctionVector = fl::vector<FunctionEntry>;

    FunctionVector mFunctions;
    int mIdCounter = 0;

    bool mNeedsCompact = false;  // True when functions have been cleared during invocation

  public:
    function_list() FL_NOEXCEPT : mFunctions(), mIdCounter(0), mNeedsCompact(false) {}
    function_list(const function_list& other) FL_NOEXCEPT
        : mFunctions(other.mFunctions), mIdCounter(other.mIdCounter), mNeedsCompact(other.mNeedsCompact) {}
    function_list(function_list&& other) FL_NOEXCEPT
        : mFunctions(fl::move(other.mFunctions)), mIdCounter(other.mIdCounter), mNeedsCompact(other.mNeedsCompact) {}
    function_list& operator=(const function_list& other) FL_NOEXCEPT {
        if (this != &other) {
            mFunctions = other.mFunctions;
            mIdCounter = other.mIdCounter;
            mNeedsCompact = other.mNeedsCompact;
        }
        return *this;
    }
    function_list& operator=(function_list&& other) FL_NOEXCEPT {
        if (this != &other) {
            mFunctions = fl::move(other.mFunctions);
            mIdCounter = other.mIdCounter;
            mNeedsCompact = other.mNeedsCompact;
        }
        return *this;
    }
    ~function_list() = default;

    int add(function<void(Args...)> fn, int priority = 0) FL_NOEXCEPT {
        int id = mIdCounter++;
        mFunctions.push_back(FunctionEntry(id, priority, fn));
        return id;
    }

    void remove(int id) FL_NOEXCEPT {
        // During invocation: clear the function for deferred removal
        for (size_t i = 0; i < mFunctions.size(); ++i) {
            if (mFunctions[i].id == id) {
                mFunctions[i].fn.clear();
                mNeedsCompact = true;
            }
        }
    }

    void clear() FL_NOEXCEPT {
        mFunctions.clear();
    }

    // Compact the storage by removing invalid (cleared) callbacks
    // Used internally after invocation if removals occurred
    void compact() FL_NOEXCEPT {
        if (!mNeedsCompact) return;
        size_t write_pos = 0;
        for (size_t read_pos = 0; read_pos < mFunctions.size(); ++read_pos) {
            if (mFunctions[read_pos].fn) {  // Check if function is still valid
                if (write_pos != read_pos) {
                    mFunctions[write_pos] = mFunctions[read_pos];
                }
                write_pos++;
            }
        }
        mFunctions.resize(write_pos);
        mNeedsCompact = false;
    }

    // Size information - counts only valid (non-cleared) functions
    fl::size size() const FL_NOEXCEPT {
        fl::size count = 0;
        for (const auto& entry : mFunctions) {
            if (entry.fn) {
                ++count;
            }
        }
        return count;
    }
    bool empty() const FL_NOEXCEPT { return size() == 0; }

    // Boolean conversion for if (callback) checks
    explicit operator bool() const FL_NOEXCEPT { return !empty(); }

    void invoke(Args... args) FL_NOEXCEPT {
        if (mFunctions.empty()) return;
        // Compact the storage by removing invalid (cleared) callbacks
        compact();
        // Save size at start - newly added callbacks won't execute until next call
        const size_t invoke_size = mFunctions.size();
        // Early return if no callbacks
        if (invoke_size == 0) {
            return;
        }

        // Collect unique priorities into a small vector (usually very few unique values)
        fl::vector_inlined<int, 16> priorities;
        for (size_t i = 0; i < invoke_size; ++i) {
            if (!mFunctions[i].fn) continue;  // Skip already-cleared functions

            int p = mFunctions[i].priority;
            // Only add if not already present
            if (priorities.find(p) == priorities.end()) {
                priorities.push_back(p);
            }
        }

        // Sort priorities (higher priority first) - cheap since there are usually very few unique priorities
        fl::sort(priorities.begin(), priorities.end(), [](int a, int b) FL_NOEXCEPT { return a > b; });

        // Iterate through priorities (highest first), then through functions matching each priority
        for (size_t p_idx = 0; p_idx < priorities.size(); ++p_idx) {
            int current_priority = priorities[p_idx];
            for (size_t i = 0; i < invoke_size; ++i) {
                if (mFunctions.empty()) {
                    return;  // Clear happened.
                }
                if (mFunctions[i].fn && mFunctions[i].priority == current_priority) {
                    mFunctions[i].fn(args...);
                }
            }
        }
        compact();
    }

    // Call operator - syntactic sugar for invoke()
    void operator()(Args... args) FL_NOEXCEPT {
        invoke(args...);
    }
};

// Partial specialization for non-void return types: function_list<R(Args...)> where R != void
// Triggers a compile-time error when attempting to use non-void return types
template <typename R, typename... Args>
class function_list<R(Args...)> {
    FL_STATIC_ASSERT(fl::is_same<R, void>::value,
                  "function_list only supports void return type. "
                  "Use function_list<void(Args...)> instead of function_list<ReturnType(Args...)>.");
};

} // namespace fl

FL_DISABLE_WARNING_POP
