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
    null_return_impl() FL_NO_EXCEPT { return R2{}; }

    template <typename R2>
    static typename enable_if<is_void<R2>::value, R2>::type
    null_return_impl() FL_NO_EXCEPT { return; }

    static R null_invoker(const void* /*src*/, Args... /*args*/) FL_NO_EXCEPT {
        return null_return_impl<R>();
    }
    static void null_manager(void* /*dst*/, void* /*src*/, Op /*op*/) FL_NO_EXCEPT {}

    // ---- Per-F invoker --------------------------------------------------------
    // Copies F out of storage so that mutable lambdas remain callable through a
    // const operator(). The previous variant-based implementation used the same
    // pattern.
    template <typename Func>
    static R functor_invoker(const void* src, Args... args) FL_NO_EXCEPT {
        FL_ALIGN_AS(Func) char tmp[sizeof(Func)];
        fl::memcpy(tmp, src, sizeof(Func));
        Func* f = static_cast<Func*>(static_cast<void*>(tmp));
        return (*f)(args...);
    }

    // ---- Trivial manager: F is trivially copyable; memcpy semantics. ----------
    // Shared across all trivially-copyable types of the same size, so the
    // linker collapses many F's into one body (per-size, not per-F).
    template <fl::size N>
    static void trivial_manager(void* dst, void* src, Op op) FL_NO_EXCEPT {
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
    static void non_trivial_manager(void* dst, void* src, Op op) FL_NO_EXCEPT {
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
        R operator()(Args... args) const FL_NO_EXCEPT {
            return (*ptr)(args...);
        }
    };

    // ---- init_with: place F into storage, wire up invoker/manager ------------
    // Tag-dispatches between in-SBO storage (when sizeof(F) fits) and the
    // HeapHolder fallback for larger callables. Both paths go through the same
    // functor_invoker / manager dispatch.
    template <typename Func>
    void init_with(Func&& f) FL_NO_EXCEPT {
        using FBare = typename remove_reference<Func>::type;
        FL_STATIC_ASSERT(alignof(FBare) <= alignof(max_align_t),
                         "Callable requires stricter alignment than SBO provides.");
        init_with_impl(fl::forward<Func>(f),
                       integral_constant<bool, (sizeof(FBare) <= kSboSize)>{});
    }

    template <typename Func>
    void init_with_impl(Func&& f, true_type /* fits in SBO */) FL_NO_EXCEPT {
        using FBare = typename remove_reference<Func>::type;
        new (mBytes) FBare(fl::forward<Func>(f));
        mInvoker = &functor_invoker<FBare>;
        mManager = is_trivially_copyable<FBare>::value
                        ? &trivial_manager<sizeof(FBare)>
                        : &non_trivial_manager<FBare>;
        mHasValue = true;
    }

    template <typename Func>
    void init_with_impl(Func&& f, false_type /* over-SBO; use HeapHolder */) FL_NO_EXCEPT {
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
    void reset() FL_NO_EXCEPT {
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
        R operator()(Args... args) const FL_NO_EXCEPT {
            return (obj->*mf)(args...);
        }
    };

    template <typename C>
    struct ConstMemberWrapper {
        const C* obj;
        R (C::*mf)(Args...) const;
        R operator()(Args... args) const FL_NO_EXCEPT {
            return (obj->*mf)(args...);
        }
    };

public:
    function() FL_NO_EXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        // Initialize storage to zero so a copy of an empty function doesn't
        // memcpy uninitialized memory. (Matches the legacy behavior.)
        fl::memset(mBytes, 0, kSboSize);
    }

    // Copy constructor
    function(const function& other) FL_NO_EXCEPT
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
    function(function&& other) FL_NO_EXCEPT
        : mInvoker(other.mInvoker), mManager(other.mManager), mHasValue(other.mHasValue) {
        fl::memset(mBytes, 0, kSboSize);
        if (mHasValue) {
            mManager(mBytes, other.mBytes, Op::Move);
            // Destroy the moved-from object's storage and leave *other empty.
            other.reset();
        }
    }

    // Copy assignment
    function& operator=(const function& other) FL_NO_EXCEPT {
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
    function& operator=(function&& other) FL_NO_EXCEPT {
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

    ~function() FL_NO_EXCEPT {
        reset();
    }

    // 1) Free function pointer
    function(R (*fp)(Args...)) FL_NO_EXCEPT
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
    function(Func f) FL_NO_EXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(fl::move(f));
    }

    // 3) Non-const member function
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) FL_NO_EXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(NonConstMemberWrapper<C>{obj, mf});
    }

    // 4) Const member function
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) FL_NO_EXCEPT
        : mInvoker(&null_invoker), mManager(&null_manager), mHasValue(false) {
        fl::memset(mBytes, 0, kSboSize);
        init_with(ConstMemberWrapper<C>{obj, mf});
    }

    R operator()(Args... args) const FL_NO_EXCEPT {
        return mInvoker(mBytes, args...);
    }

    explicit operator bool() const FL_NO_EXCEPT { return mHasValue; }

    void clear() FL_NO_EXCEPT { reset(); }

    bool operator==(const function& o) const FL_NO_EXCEPT {
        // Mirrors legacy semantics: just empty / non-empty equality.
        return mHasValue == o.mHasValue;
    }

    bool operator!=(const function& o) const FL_NO_EXCEPT { return !(*this == o); }
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

namespace detail {
// The priority vector is independent of the callback signature. Keep one sort
// implementation for all function_list instantiations with subscribers.
FL_NO_INLINE inline void sortFunctionListPriorities(fl::vector_inlined<int, 16>& priorities) FL_NO_EXCEPT {
    fl::sort(priorities.begin(), priorities.end(), [](int a, int b) FL_NO_EXCEPT { return a > b; });
}
} // namespace detail

namespace detail {
template <typename Signature, bool Large> class FunctionListImpl;

// A stack activation retains the actual callable across an active list move.
// Entry relocation retargets it; entry destruction detaches it. Normal list
// mutations still use the depth guard and deferred compaction below.
template <typename... Args>
struct FunctionListEntry {
    using FunctionType = function<void(Args...)>;
    struct Activation {
        FunctionListEntry* entry;
        FunctionType fn;
        bool valid = true;
        explicit Activation(FunctionListEntry& value) FL_NO_EXCEPT
            : entry(&value), fn(fl::move(value.fn)) { value.active = this; }
        ~Activation() FL_NO_EXCEPT {
            if (entry) {
                entry->active = nullptr;
                // A removed entry still owns its callable until outer cleanup.
                entry->fn = fl::move(fn);
            }
        }
    };

    int id;
    int priority;
    FunctionType fn;
    // Borrowed only during synchronous dispatch; moves retarget the activation,
    // destruction detaches it, and copies clone its actual callable value.
    Activation* active = nullptr;

    FunctionListEntry() FL_NO_EXCEPT : id(-1), priority(0), fn() {}
    FunctionListEntry(int value, int rank, FunctionType callback) FL_NO_EXCEPT
        : id(value), priority(rank), fn(fl::move(callback)) {}
    FunctionListEntry(const FunctionListEntry& other) FL_NO_EXCEPT
        : id(other.id), priority(other.priority), fn(copyFunction(other)) {}
    FunctionListEntry(FunctionListEntry&& other) FL_NO_EXCEPT
        : id(other.id), priority(other.priority), fn(fl::move(other.fn)), active(other.active) {
        if (active) active->entry = this;
        other.active = nullptr;
    }
    FunctionListEntry& operator=(const FunctionListEntry& other) FL_NO_EXCEPT {
        if (this != &other) {
            detach();
            id = other.id;
            priority = other.priority;
            fn = copyFunction(other);
        }
        return *this;
    }
    FunctionListEntry& operator=(FunctionListEntry&& other) FL_NO_EXCEPT {
        if (this != &other) {
            detach();
            id = other.id;
            priority = other.priority;
            fn = fl::move(other.fn);
            active = other.active;
            if (active) active->entry = this;
            other.active = nullptr;
        }
        return *this;
    }
    ~FunctionListEntry() FL_NO_EXCEPT { detach(); }

    bool isValid() const FL_NO_EXCEPT { return id >= 0 && (active ? active->valid : bool(fn)); }
    void clear() FL_NO_EXCEPT {
        id = -1;
        if (active) active->valid = false;
        // Destruction is deferred by the list's dispatch/compaction guard.
    }
    void invoke(Args... args) FL_NO_EXCEPT {
        if (active) active->fn(args...);
        else {
            Activation call(*this);
            call.fn(args...);
        }
    }
  private:
    static FunctionType copyFunction(const FunctionListEntry& other) FL_NO_EXCEPT {
        if (!other.isValid()) return FunctionType();
        return other.active ? other.active->fn : other.fn;
    }
    void detach() FL_NO_EXCEPT {
        if (active) {
            active->entry = nullptr;
            active->valid = false;
            active = nullptr;
        }
    }
};

// Small tiers own their vectors directly and never allocate a State. The same
// mutation/dispatch implementation is also used inside large-tier owned State.
template <typename... Args>
class FunctionListImpl<void(Args...), false> {
    template <typename, bool> friend class FunctionListImpl;
    using FunctionEntry = FunctionListEntry<Args...>;
    using FunctionVector = fl::vector<FunctionEntry>;
    FunctionVector mFunctions;
    FunctionVector mPending;
    int mIdCounter = 0;
    u32 mRevision = 0;
    unsigned mDepth = 0;
    bool mNeedsCompact = false;

    struct DispatchGuard {
        FunctionListImpl& list;
        explicit DispatchGuard(FunctionListImpl& value) FL_NO_EXCEPT : list(value) { ++list.mDepth; }
        ~DispatchGuard() FL_NO_EXCEPT {
            if (--list.mDepth == 0) list.compact();
        }
    };

    void copyFrom(const FunctionListImpl& other) FL_NO_EXCEPT {
        for (const auto& entry : other.mFunctions) {
            if (entry.isValid()) mFunctions.push_back(entry);
        }
        for (const auto& entry : other.mPending) {
            if (entry.isValid()) mFunctions.push_back(entry);
        }
        mIdCounter = other.mIdCounter;
    }
    void replaceWith(FunctionListImpl&& replacement) FL_NO_EXCEPT {
        ++mRevision;
        mIdCounter = replacement.mIdCounter;
        if (mDepth) {
            // Keep the old vector, including running callbacks, alive. The
            // replacement becomes visible after this dispatch tree returns.
            for (auto& entry : mFunctions) entry.clear();
            for (auto& entry : mPending) entry.clear();
            for (auto& entry : replacement.mFunctions) mPending.push_back(fl::move(entry));
            mNeedsCompact = true;
        } else {
            mFunctions = fl::move(replacement.mFunctions);
            mPending.clear();
            mNeedsCompact = false;
        }
    }

  public:
    FunctionListImpl() FL_NO_EXCEPT = default;
    FunctionListImpl(const FunctionListImpl& other) FL_NO_EXCEPT { copyFrom(other); }
    FunctionListImpl(FunctionListImpl&& other) FL_NO_EXCEPT {
        // Moving the vector transfers its allocation, not the running callable.
        // Its stack activation keeps ownership if the destination later mutates
        // or destroys that entry before the original callback returns.
        mFunctions = fl::move(other.mFunctions);
        mPending = fl::move(other.mPending);
        mIdCounter = other.mIdCounter;
        mNeedsCompact = other.mNeedsCompact;
        ++other.mRevision;
    }
    FunctionListImpl& operator=(const FunctionListImpl& other) FL_NO_EXCEPT {
        if (this != &other) replaceWith(FunctionListImpl(other));
        return *this;
    }
    FunctionListImpl& operator=(FunctionListImpl&& other) FL_NO_EXCEPT {
        if (this != &other) replaceWith(FunctionListImpl(fl::move(other)));
        return *this;
    }

    int add(function<void(Args...)> fn, int priority = 0) FL_NO_EXCEPT {
        int id = mIdCounter++;
        auto& destination = mDepth ? mPending : mFunctions;
        destination.push_back(FunctionEntry(id, priority, fl::move(fn)));
        return id;
    }
    void remove(int id) FL_NO_EXCEPT {
        for (auto& entry : mFunctions) {
            if (entry.id == id) {
                entry.clear();
                mNeedsCompact = true;
            }
        }
        for (auto& entry : mPending) {
            if (entry.id == id) entry.clear();
        }
        // Even outside dispatch, capture destructors can mutate this list.
        compact();
    }
    void clear() FL_NO_EXCEPT {
        ++mRevision;
        for (auto& entry : mFunctions) entry.clear();
        for (auto& entry : mPending) entry.clear();
        mNeedsCompact = true;
        compact();
    }
    void compact() FL_NO_EXCEPT {
        if (mDepth || (!mNeedsCompact && mPending.empty())) return;
        // Destructors may register, remove or clear callbacks. Keep them on the
        // deferred path, and retire dead entries before moving any survivors.
        ++mDepth;
        do {
            {
                FunctionVector retired;
                if (mNeedsCompact) {
                    for (auto& entry : mFunctions) {
                        if (!entry.isValid()) retired.push_back(fl::move(entry));
                    }
                    size_t write = 0;
                    for (size_t read = 0; read < mFunctions.size(); ++read) {
                        if (mFunctions[read].isValid()) {
                            if (write != read) mFunctions[write] = fl::move(mFunctions[read]);
                            ++write;
                        }
                    }
                    mFunctions.resize(write);
                    mNeedsCompact = false;
                }
                // A separate batch keeps destructor-driven additions out of the
                // vector being iterated. Both batches die after all moves finish.
                FunctionVector pending = fl::move(mPending);
                for (auto& entry : pending) {
                    if (entry.isValid()) mFunctions.push_back(fl::move(entry));
                }
            }
        } while (mNeedsCompact || !mPending.empty());
        --mDepth;
    }
    fl::size size() const FL_NO_EXCEPT {
        fl::size count = 0;
        for (const auto& entry : mFunctions) if (entry.isValid()) ++count;
        for (const auto& entry : mPending) if (entry.isValid()) ++count;
        return count;
    }
    bool empty() const FL_NO_EXCEPT { return size() == 0; }
    explicit operator bool() const FL_NO_EXCEPT { return !empty(); }
  private:
    bool isDispatching() const FL_NO_EXCEPT { return mDepth != 0; }
    void interrupt() FL_NO_EXCEPT { ++mRevision; }

  public:
    void invoke(Args... args) FL_NO_EXCEPT {
        if (mFunctions.empty()) return;
        compact();
        const size_t invokeSize = mFunctions.size();
        if (!invokeSize) return;
        const u32 revision = mRevision;
        DispatchGuard guard(*this);
        fl::vector_inlined<int, 16> priorities;
        for (size_t i = 0; i < invokeSize; ++i) {
            if (!mFunctions[i].isValid()) continue;
            int priority = mFunctions[i].priority;
            if (priorities.find(priority) == priorities.end()) priorities.push_back(priority);
        }
        sortFunctionListPriorities(priorities);
        for (size_t p = 0; p < priorities.size(); ++p) {
            for (size_t i = 0; i < invokeSize; ++i) {
                if (mRevision != revision) return;
                if (mFunctions[i].isValid() && mFunctions[i].priority == priorities[p]) {
                    mFunctions[i].invoke(args...);
                }
            }
        }
    }
    void operator()(Args... args) FL_NO_EXCEPT { invoke(args...); }
};

#if FL_PLATFORM_HAS_LARGE_MEMORY
FL_NO_INLINE void warnFunctionListAllocationFailure() FL_NO_EXCEPT;

// Large tiers instantiate subscribed callback machinery only on first add().
template <typename... Args>
class FunctionListImpl<void(Args...), true> {
    using Core = FunctionListImpl<void(Args...), false>;
    using Invoker = void (*)(FunctionListImpl&, Args...);
    struct State : Core {
        Invoker mInvoker = &invokeSubscribed;
    };
    fl::shared_ptr<State> mState;

    void replaceWith(fl::shared_ptr<State> replacement) FL_NO_EXCEPT {
        if (mState && mState->isDispatching()) {
            // Keep replacement callbacks pending on the dispatching core, just
            // as the inline-owned tier does. Nested invocation cannot see them.
            Core next;
            if (replacement) next = fl::move(static_cast<Core&>(*replacement));
            mState->Core::replaceWith(fl::move(next));
        } else {
            mState = fl::move(replacement);
        }
    }
    static void invokeSubscribed(FunctionListImpl& self, Args... args) FL_NO_EXCEPT {
        // Reassignment can release the list's owner, so dispatch retains its
        // own strong reference until the depth guard commits deferred changes.
        auto state = self.mState;
        state->invoke(args...);
    }

  public:
    FunctionListImpl() FL_NO_EXCEPT = default;
    // Copies are independent. Allocation failure produces an empty copy,
    // consistently for construction and assignment; the source is unchanged.
    FunctionListImpl(const FunctionListImpl& other) FL_NO_EXCEPT
        : mState(other.mState ? fl::make_shared<State>(*other.mState) : fl::shared_ptr<State>()) {}
    FunctionListImpl(FunctionListImpl&& other) FL_NO_EXCEPT : mState(fl::move(other.mState)) {
        if (mState && mState->isDispatching()) mState->interrupt();
    }
    FunctionListImpl& operator=(const FunctionListImpl& other) FL_NO_EXCEPT {
        if (this != &other) replaceWith(other.mState ? fl::make_shared<State>(*other.mState) : fl::shared_ptr<State>());
        return *this;
    }
    FunctionListImpl& operator=(FunctionListImpl&& other) FL_NO_EXCEPT {
        if (this != &other) {
            if (other.mState && other.mState->isDispatching()) other.mState->interrupt();
            replaceWith(fl::move(other.mState));
        }
        return *this;
    }
    // Returns -1 if the initial State allocation fails; warns once globally.
    int add(function<void(Args...)> fn, int priority = 0) FL_NO_EXCEPT {
        if (!mState) {
            mState = fl::make_shared<State>();
            if (!mState) { warnFunctionListAllocationFailure(); return -1; }
        }
        return mState->add(fl::move(fn), priority);
    }
    void remove(int id) FL_NO_EXCEPT { if (mState) mState->remove(id); }
    void clear() FL_NO_EXCEPT { if (mState) mState->clear(); }
    void compact() FL_NO_EXCEPT { if (mState) mState->compact(); }
    fl::size size() const FL_NO_EXCEPT { return mState ? mState->size() : 0; }
    bool empty() const FL_NO_EXCEPT { return size() == 0; }
    explicit operator bool() const FL_NO_EXCEPT { return !empty(); }
    void invoke(Args... args) FL_NO_EXCEPT {
        if (mState) mState->mInvoker(*this, args...);
    }
    void operator()(Args... args) FL_NO_EXCEPT { invoke(args...); }
};

#endif
} // namespace detail

template <typename... Args>
class function_list<void(Args...)>
    : public detail::FunctionListImpl<void(Args...), FL_PLATFORM_HAS_LARGE_MEMORY> {};

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
