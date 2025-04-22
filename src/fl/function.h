#pragma once
#include "fl/template_magic.h"
#include "fl/scoped_ptr.h"

namespace fl {

//----------------------------------------------------------------------------
// Function<R(Args...)>: type‐erasing “std::function” replacement
// Supports free functions, lambdas/functors, member functions (const & non‑const)
//----------------------------------------------------------------------------
template <typename> class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
private:
    struct CallableBase {
        virtual R invoke(Args... args)      = 0;
        virtual CallableBase* clone() const = 0;
        virtual ~CallableBase() = default;
    };

    // Wraps a lambda/functor or free function pointer
    template <typename F>
    struct Callable : CallableBase {
        F f;
        Callable(F fn) : f(fn) {}
        R invoke(Args... args) override {
            return f(args...);
        }
        CallableBase* clone() const override {
            return new Callable<F>(f);
        }
    };

    // Wraps a non‑const member function
    template <typename C>
    struct MemCallable : CallableBase {
        C* obj;
        R (C::*mfp)(Args...);
        MemCallable(C* o, R (C::*m)(Args...)) : obj(o), mfp(m) {}
        R invoke(Args... args) override {
            return (obj->*mfp)(args...);
        }
        CallableBase* clone() const override {
            return new MemCallable<C>(obj, mfp);
        }
    };

    // Wraps a const member function
    template <typename C>
    struct ConstMemCallable : CallableBase {
        const C* obj;
        R (C::*mfp)(Args...) const;
        ConstMemCallable(const C* o, R (C::*m)(Args...) const)
          : obj(o), mfp(m) {}
        R invoke(Args... args) override {
            return (obj->*mfp)(args...);
        }
        CallableBase* clone() const override {
            // <-- fixed: use mfp, not mf
            return new ConstMemCallable<C>(obj, mfp);
        }
    };

    scoped_ptr<CallableBase> callable_;

public:
    Function() = default;
    ~Function() { callable_.reset(); }

    Function(const Function& o)
      : callable_(o.callable_ ? o.callable_->clone() : nullptr) {}

    Function(Function&& o) noexcept
      : callable_(o.callable_.release()) {
    }

    Function& operator=(const Function& o) {
        if (this != &o) {
            delete callable_;
            callable_ = o.callable_ ? o.callable_->clone() : nullptr;
        }
        return *this;
    }

    Function& operator=(Function&& o) noexcept {
        if (this != &o) {
            delete callable_;
            callable_ = o.callable_;
            o.callable_ = nullptr;
        }
        return *this;
    }

    // 1) generic constructor for lambdas, free functions, functors
    template <typename F,
              typename = enable_if_t<!is_member_function_pointer<F>::value>>
    Function(F f)
      : callable_(new Callable<F>(f)) {}

    // 2) non‑const member function
    template <typename C>
    Function(R (C::*mf)(Args...), C* obj)
      : callable_(new MemCallable<C>(obj, mf)) {}

    // 3) const member function
    template <typename C>
    Function(R (C::*mf)(Args...) const, const C* obj)
      : callable_(new ConstMemCallable<C>(obj, mf)) {}

    // Invocation
    R operator()(Args... args) const {
        return callable_->invoke(args...);
    }

    explicit operator bool() const {
        return callable_.get() != nullptr;
    }
};

//----------------------------------------------------------------------------
// trait definitions for is_member_function_pointer
//----------------------------------------------------------------------------
template <typename T>
struct is_member_function_pointer {
    static constexpr bool value = false;
};

template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...)> {
    static constexpr bool value = true;
};

template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...) const> {
    static constexpr bool value = true;
};

} // namespace fl
