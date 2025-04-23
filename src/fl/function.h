#pragma once
#include "fl/template_magic.h"
#include "fl/ptr.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

namespace fl {

//----------------------------------------------------------------------------
// Function<R(Args...)>: type‐erasing “std::function” replacement
// Supports free functions, lambdas/functors, member functions (const & non‑const)
//----------------------------------------------------------------------------
template <typename> class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
private:
    struct CallableBase : public Referent {
        virtual R invoke(Args... args)      = 0;
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
    };

    Ptr<CallableBase> callable_;

public:
    Function() = default;
    ~Function()  = default;

    Function(const Function& o)
      : callable_(o.callable_) {}

    Function(Function&& o) noexcept {
        callable_.swap(o.callable_);
    }

    Function& operator=(const Function& o) {
        if (this != &o) {
            callable_ = o.callable_;
        }
        return *this;
    }

    // doesn't work with our containers.
    // Function& operator=(Function&& o) noexcept {
    //     if (this != &o) {
    //         callable_ = o.callable_;
    //         o.callable_.reset(nullptr);
    //     }
    //     return *this;
    // }

    // 1) generic constructor for lambdas, free functions, functors
    template <typename F,
              typename = enable_if_t<!is_member_function_pointer<F>::value>>
    Function(F f)
      : callable_(NewPtr<Callable<F>>(f)) {}

    // 2) non‑const member function
    template <typename C>
    Function(R (C::*mf)(Args...), C* obj)
      : callable_(NewPtr<MemCallable<C>>(obj, mf)) {}

    // 3) const member function
    template <typename C>
    Function(R (C::*mf)(Args...) const, const C* obj)
      : callable_(NewPtr<ConstMemCallable<C>>(obj, mf)) {}

    // Invocation
    R operator()(Args... args) const {
        return callable_->invoke(args...);
    }

    explicit operator bool() const {
        return callable_ != nullptr;
    }

    bool operator==(const Function& o) const {
        return callable_ == o.callable_;
    }

    bool operator!=(const Function& o) const {
        return callable_ != o.callable_;
    }
};

} // namespace fl


#pragma GCC diagnostic pop

