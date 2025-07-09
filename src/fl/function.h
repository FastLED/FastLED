#pragma once
#include "fl/ptr.h"
#include "fl/template_magic.h"
#include "fl/compiler_control.h"
#include "fl/variant.h"
#include "fl/memfill.h"
#include "fl/type_traits.h"

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

    // Type-erased member function callable base
    struct MemberCallableBase {
        virtual R invoke(Args... args) const = 0;
        virtual ~MemberCallableBase() = default;
    };

    // Type-erased non-const member function callable
    struct NonConstMemberCallable : MemberCallableBase {
        void* obj;
        // Union to store member function pointer as raw bytes
        union MemberFuncStorage {
            char bytes[sizeof(R (NonConstMemberCallable::*)(Args...))];
            // Ensure proper alignment
            void* alignment_dummy;
        } member_func_storage;
        
        // Type-erased invoker function - set at construction time
        R (*invoker)(void* obj, const MemberFuncStorage& mfp, Args... args);
        
        template <typename C>
        NonConstMemberCallable(C* o, R (C::*mf)(Args...)) : obj(o) {
            // Store the member function pointer as raw bytes
            static_assert(sizeof(mf) <= sizeof(member_func_storage), 
                         "Member function pointer too large");
            fl::memcopy(member_func_storage.bytes, &mf, sizeof(mf));
            // Set the invoker to a function that knows how to cast back and call
            invoker = &invoke_nonconst_member<C>;
        }
        
        template <typename C>
        static R invoke_nonconst_member(void* obj, const MemberFuncStorage& mfp, Args... args) {
            C* typed_obj = static_cast<C*>(obj);
            R (C::*typed_mf)(Args...);
            fl::memcopy(&typed_mf, mfp.bytes, sizeof(typed_mf));
            return (typed_obj->*typed_mf)(args...);
        }
        
        R invoke(Args... args) const override {
            return invoker(obj, member_func_storage, args...);
        }
    };

    // Type-erased const member function callable  
    struct ConstMemberCallable : MemberCallableBase {
        const void* obj;
        // Union to store member function pointer as raw bytes
        union MemberFuncStorage {
            char bytes[sizeof(R (ConstMemberCallable::*)(Args...) const)];
            // Ensure proper alignment
            void* alignment_dummy;
        } member_func_storage;
        
        // Type-erased invoker function - set at construction time
        R (*invoker)(const void* obj, const MemberFuncStorage& mfp, Args... args);
        
        template <typename C>
        ConstMemberCallable(const C* o, R (C::*mf)(Args...) const) : obj(o) {
            // Store the member function pointer as raw bytes
            static_assert(sizeof(mf) <= sizeof(member_func_storage), 
                         "Member function pointer too large");
            fl::memcopy(member_func_storage.bytes, &mf, sizeof(mf));
            // Set the invoker to a function that knows how to cast back and call
            invoker = &invoke_const_member<C>;
        }
        
        template <typename C>
        static R invoke_const_member(const void* obj, const MemberFuncStorage& mfp, Args... args) {
            const C* typed_obj = static_cast<const C*>(obj);
            R (C::*typed_mf)(Args...) const;
            fl::memcopy(&typed_mf, mfp.bytes, sizeof(typed_mf));
            return (typed_obj->*typed_mf)(args...);
        }
        
        R invoke(Args... args) const override {
            return invoker(obj, member_func_storage, args...);
        }
    };

    // Variant to store any of our callable types inline
    using Storage = Variant<Ptr<CallableBase>, NonConstMemberCallable, ConstMemberCallable>;
    Storage storage_;

    // Helper function to handle default return value for void and non-void types
    template<typename ReturnType>
    typename enable_if<!is_void<ReturnType>::value, ReturnType>::type
    default_return_helper() const {
        return ReturnType{};
    }
    
    template<typename ReturnType>
    typename enable_if<is_void<ReturnType>::value, ReturnType>::type
    default_return_helper() const {
        return;
    }

public:
    function() = default;
    
    // 1) generic constructor for lambdas, free functions, functors
    template <typename F, typename = enable_if_t<!is_member_function_pointer<F>::value>>
    function(F f) {
        storage_ = Ptr<CallableBase>(NewPtr<Callable<F>>(f));
    }
    
    // 2) non‑const member function - now stored inline!
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) {
        storage_ = NonConstMemberCallable(obj, mf);
    }
    
    // 3) const member function - now stored inline!
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) {
        storage_ = ConstMemberCallable(obj, mf);
    }
    
    R operator()(Args... args) const {
        // Direct dispatch using type checking
        if (auto* heap_callable = storage_.template ptr<Ptr<CallableBase>>()) {
            return (*heap_callable)->invoke(args...);
        } else if (auto* nonconst_member = storage_.template ptr<NonConstMemberCallable>()) {
            return nonconst_member->invoke(args...);
        } else if (auto* const_member = storage_.template ptr<ConstMemberCallable>()) {
            return const_member->invoke(args...);
        }
        // This should never happen if the function is properly constructed
        // Return default-constructed R (or throw if you prefer)
        return default_return_helper<R>();
    }
    
    explicit operator bool() const {
        return !storage_.empty();
    }
    
    bool operator==(const function& o) const {
        // For simplicity, just check if both are empty or both are non-empty
        // Full equality would require more complex comparison logic
        return storage_.empty() == o.storage_.empty();
    }
    
    bool operator!=(const function& o) const {
        return !(*this == o);
    }
};

} // namespace fl

FL_DISABLE_WARNING_POP
