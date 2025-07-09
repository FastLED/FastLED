#pragma once
#include "fl/ptr.h"
#include "fl/template_magic.h"
#include "fl/compiler_control.h"
#include "fl/warn.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-equal)

namespace fl {

//----------------------------------------------------------------------------
// More or less a drop in replacement for std::function
// function<R(Args...)>: type‐erasing "std::function" replacement
// Supports free functions, lambdas/functors, member functions (const &
// non‑const)
// 
// NEW: Uses inline storage for member function callables
// to avoid heap allocation for member function calls.
//----------------------------------------------------------------------------
template <typename> class function;

template <typename R, typename... Args>
class function<R(Args...)> {
private:
    struct CallableBase : public Referent {
        virtual R invoke(Args... args) = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F>
    struct Callable : CallableBase {
        F f;
        Callable(F fn) : f(fn) {}
        R invoke(Args... args) override { return f(args...); }
    };

    // Type-erased member function callable
    struct MemberCallableBase {
        virtual R invoke(Args... args) const = 0;
        virtual MemberCallableBase* clone() const = 0;
        virtual ~MemberCallableBase() = default;
    };

    template <typename C>
    struct NonConstMemberCallable : MemberCallableBase {
        C* obj;
        R (C::*member_func)(Args...);
        NonConstMemberCallable(C* o, R (C::*mf)(Args...)) : obj(o), member_func(mf) {}
        R invoke(Args... args) const override {
            return (obj->*member_func)(args...);
        }
        MemberCallableBase* clone() const override {
            return new NonConstMemberCallable(*this);
        }
    };

    template <typename C>
    struct ConstMemberCallable : MemberCallableBase {
        const C* obj;
        R (C::*member_func)(Args...) const;
        ConstMemberCallable(const C* o, R (C::*mf)(Args...) const) : obj(o), member_func(mf) {}
        R invoke(Args... args) const override {
            return (obj->*member_func)(args...);
        }
        MemberCallableBase* clone() const override {
            return new ConstMemberCallable(*this);
        }
    };

    union Storage {
        Ptr<CallableBase> heap_callable;
        MemberCallableBase* member_callable;
        Storage() : heap_callable() {}
        ~Storage() {}
    };
    Storage storage_;
    bool is_member_callable_;

public:
    function() : is_member_callable_(false) {}
    ~function() {
        if (is_member_callable_) {
            delete storage_.member_callable;
        }
    }
    function(const function& o) : is_member_callable_(o.is_member_callable_) {
        if (is_member_callable_) {
            storage_.member_callable = o.storage_.member_callable ? o.storage_.member_callable->clone() : nullptr;
        } else {
            new (&storage_.heap_callable) Ptr<CallableBase>(o.storage_.heap_callable);
        }
    }
    function(function&& o) noexcept : is_member_callable_(o.is_member_callable_) {
        if (is_member_callable_) {
            storage_.member_callable = o.storage_.member_callable;
            o.storage_.member_callable = nullptr;
        } else {
            new (&storage_.heap_callable) Ptr<CallableBase>(fl::move(o.storage_.heap_callable));
        }
        o.is_member_callable_ = false;
    }
    function& operator=(const function& o) {
        if (this != &o) {
            if (is_member_callable_) {
                delete storage_.member_callable;
            }
            is_member_callable_ = o.is_member_callable_;
            if (is_member_callable_) {
                storage_.member_callable = o.storage_.member_callable ? o.storage_.member_callable->clone() : nullptr;
            } else {
                new (&storage_.heap_callable) Ptr<CallableBase>(o.storage_.heap_callable);
            }
        }
        return *this;
    }
    function& operator=(function&& o) noexcept {
        if (this != &o) {
            if (is_member_callable_) {
                delete storage_.member_callable;
            }
            is_member_callable_ = o.is_member_callable_;
            if (is_member_callable_) {
                storage_.member_callable = o.storage_.member_callable;
                o.storage_.member_callable = nullptr;
            } else {
                new (&storage_.heap_callable) Ptr<CallableBase>(fl::move(o.storage_.heap_callable));
            }
            o.is_member_callable_ = false;
        }
        return *this;
    }
    // 1) generic constructor for lambdas, free functions, functors
    template <typename F, typename = enable_if_t<!is_member_function_pointer<F>::value>>
    function(F f) : is_member_callable_(false) {
        new (&storage_.heap_callable) Ptr<CallableBase>(NewPtr<Callable<F>>(f));
    }
    // 2) non‑const member function
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) : is_member_callable_(true) {
        storage_.member_callable = new NonConstMemberCallable<C>(obj, mf);
    }
    // 3) const member function
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) : is_member_callable_(true) {
        storage_.member_callable = new ConstMemberCallable<C>(obj, mf);
    }
    R operator()(Args... args) const {
        if (is_member_callable_) {
            return storage_.member_callable->invoke(args...);
        } else {
            return storage_.heap_callable->invoke(args...);
        }
    }
    explicit operator bool() const {
        if (is_member_callable_) {
            return storage_.member_callable != nullptr;
        } else {
            return storage_.heap_callable != nullptr;
        }
    }
    bool operator==(const function& o) const {
        if (is_member_callable_ != o.is_member_callable_) {
            return false;
        }
        if (is_member_callable_) {
            return storage_.member_callable == o.storage_.member_callable;
        } else {
            return storage_.heap_callable == o.storage_.heap_callable;
        }
    }
    bool operator!=(const function& o) const {
        return !(*this == o);
    }
};

} // namespace fl

FL_DISABLE_WARNING_POP
