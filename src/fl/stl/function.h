#pragma once
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/type_traits.h"
#include "fl/compiler_control.h"
#include "fl/stl/variant.h"
#include "fl/stl/cstring.h"
#include "fl/stl/new.h"   // for placement new operator
#include "fl/stl/bit_cast.h"
#include "fl/align.h"
#include "fl/stl/pair.h"
#include "fl/stl/vector.h"
#include "fl/stl/algorithm.h"  // for fl::sort

#ifndef FASTLED_INLINE_LAMBDA_SIZE
#define FASTLED_INLINE_LAMBDA_SIZE 64
#endif

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
// More or less a drop in replacement for std::function
// function<R(Args...)>: type‐erasing "std::function" replacement
// Supports free functions, lambdas/functors, member functions (const &
// non‑const)
// 
// NEW: Uses inline storage for member functions, free functions, and small
// lambdas/functors. Only large lambdas/functors use heap allocation.
//----------------------------------------------------------------------------
template <typename> class function;



template <typename R, typename... Args>
class FL_ALIGN function<R(Args...)> {
private:
    struct CallableBase {
        virtual R invoke(Args... args) = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F>
    struct Callable : CallableBase {
        F f;
        Callable(F fn) : f(fn) {}
        R invoke(Args... args) override { return f(args...); }
    };

    // Type-erased free function callable - stored inline!
    struct FreeFunctionCallable {
        R (*func_ptr)(Args...);
        
        FreeFunctionCallable(R (*fp)(Args...)) : func_ptr(fp) {}
        
        R invoke(Args... args) const {
            return func_ptr(args...);
        }
    };

    // Type-erased small lambda/functor callable - stored inline!
    // Size limit for inline storage - configurable via preprocessor define

    static constexpr fl::size kInlineLambdaSize = FASTLED_INLINE_LAMBDA_SIZE;
    
    struct InlinedLambda {
        // Storage for the lambda/functor object
        // Use aligned storage to ensure proper alignment for any type
        FL_ALIGN_MAX char bytes[kInlineLambdaSize];
        
        // Type-erased invoker and destructor function pointers
        R (*invoker)(const InlinedLambda& storage, Args... args);
        void (*destructor)(InlinedLambda& storage);
        
        template <typename Function>
        InlinedLambda(Function f) {
            static_assert(sizeof(Function) <= kInlineLambdaSize, 
                         "Lambda/functor too large for inline storage");
            static_assert(alignof(Function) <= alignof(max_align_t), 
                         "Lambda/functor requires stricter alignment than storage provides");
            
            // Initialize the entire storage to zero to avoid copying uninitialized memory
            fl::memset(bytes, 0, kInlineLambdaSize);
            
            // Construct the lambda/functor in-place
            new (bytes) Function(fl::move(f));
            
            // Set up type-erased function pointers
            invoker = &invoke_lambda<Function>;
            destructor = &destroy_lambda<Function>;
        }
        
        // Copy constructor
        InlinedLambda(const InlinedLambda& other) 
            : invoker(other.invoker), destructor(other.destructor) {
            // This is tricky - we need to copy the stored object
            // For now, we'll use memcopy (works for trivially copyable types)
            fl::memcpy(bytes, other.bytes, kInlineLambdaSize);
        }
        
        // Move constructor
        InlinedLambda(InlinedLambda&& other) 
            : invoker(other.invoker), destructor(other.destructor) {
            fl::memcpy(bytes, other.bytes, kInlineLambdaSize);
            // Reset the other object to prevent double destruction
            other.destructor = nullptr;
        }
        
        ~InlinedLambda() {
            if (destructor) {
                destructor(*this);
            }
        }
        
        template <typename FUNCTOR>
        static R invoke_lambda(const InlinedLambda& storage, Args... args) {
            // Use placement new to safely access the stored lambda
            FL_ALIGN_AS(FUNCTOR) char temp_storage[sizeof(FUNCTOR)];
            // Copy the lambda from storage
            fl::memcpy(temp_storage, storage.bytes, sizeof(FUNCTOR));
            // Get a properly typed pointer to the copied lambda (non-const for mutable lambdas)
            FUNCTOR* f = static_cast<FUNCTOR*>(static_cast<void*>(temp_storage));
            // Invoke the lambda
            return (*f)(args...);
        }
        
        template <typename FUNCTOR>
        static void destroy_lambda(InlinedLambda& storage) {
            // For destruction, we need to call the destructor on the actual object
            // that was constructed with placement new in storage.bytes
            // We use the standard library approach: create a properly typed pointer
            // using placement new, then call the destructor through that pointer
            
            // This is the standard-compliant way to get a properly typed pointer
            // to an object that was constructed with placement new
            FUNCTOR* obj_ptr = static_cast<FUNCTOR*>(static_cast<void*>(storage.bytes));
            obj_ptr->~FUNCTOR();
        }
        
        R invoke(Args... args) const {
            return invoker(*this, args...);
        }
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
            fl::memcpy(member_func_storage.bytes, &mf, sizeof(mf));
            // Set the invoker to a function that knows how to cast back and call
            invoker = &invoke_nonconst_member<C>;
        }
        
        template <typename C>
        static R invoke_nonconst_member(void* obj, const MemberFuncStorage& mfp, Args... args) {
            C* typed_obj = static_cast<C*>(obj);
            R (C::*typed_mf)(Args...);
            fl::memcpy(&typed_mf, mfp.bytes, sizeof(typed_mf));
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
            fl::memcpy(member_func_storage.bytes, &mf, sizeof(mf));
            // Set the invoker to a function that knows how to cast back and call
            invoker = &invoke_const_member<C>;
        }
        
        template <typename C>
        static R invoke_const_member(const void* obj, const MemberFuncStorage& mfp, Args... args) {
            const C* typed_obj = static_cast<const C*>(obj);
            R (C::*typed_mf)(Args...) const;
            fl::memcpy(&typed_mf, mfp.bytes, sizeof(typed_mf));
            return (typed_obj->*typed_mf)(args...);
        }
        
        R invoke(Args... args) const override {
            return invoker(obj, member_func_storage, args...);
        }
    };

    // variant to store any of our callable types inline (with heap fallback for large lambdas)
    using Storage = variant<fl::shared_ptr<CallableBase>, FreeFunctionCallable, InlinedLambda, NonConstMemberCallable, ConstMemberCallable>;
    Storage mStorage;

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
    
    // Copy constructor - properly handle variant alignment
    function(const function& other) : mStorage(other.mStorage) {}
    
    // Move constructor - properly handle variant alignment  
    function(function&& other) noexcept : mStorage(fl::move(other.mStorage)) {}
    
    // Copy assignment
    function& operator=(const function& other) {
        if (this != &other) {
            mStorage = other.mStorage;
        }
        return *this;
    }
    
    // Move assignment
    function& operator=(function&& other) noexcept {
        if (this != &other) {
            mStorage = fl::move(other.mStorage);
        }
        return *this;
    }
    
    // 1) Free function constructor - stored inline!
    function(R (*fp)(Args...)) {
        mStorage = FreeFunctionCallable(fp);
    }
    
    // 2) Lambda/functor constructor - inline if small, heap if large
    template <typename F, typename = enable_if_t<!is_member_function_pointer<F>::value && !is_function_pointer<F>::value>>
    function(F f) {
        // Use template specialization instead of if constexpr for C++14 compatibility
        construct_lambda_or_functor(fl::move(f), typename conditional<sizeof(F) <= kInlineLambdaSize, true_type, false_type>::type{});
    }
    
    // 3) non‑const member function - stored inline!
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) {
        mStorage = NonConstMemberCallable(obj, mf);
    }
    
    // 4) const member function - stored inline!
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) {
        mStorage = ConstMemberCallable(obj, mf);
    }
    
    R operator()(Args... args) const {
        // Direct dispatch using type checking - efficient and simple
        if (auto* heap_callable = mStorage.template ptr<fl::shared_ptr<CallableBase>>()) {
            return (*heap_callable)->invoke(args...);
        } else if (auto* free_func = mStorage.template ptr<FreeFunctionCallable>()) {
            return free_func->invoke(args...);
        } else if (auto* inlined_lambda = mStorage.template ptr<InlinedLambda>()) {
            return inlined_lambda->invoke(args...);
        } else if (auto* nonconst_member = mStorage.template ptr<NonConstMemberCallable>()) {
            return nonconst_member->invoke(args...);
        } else if (auto* const_member = mStorage.template ptr<ConstMemberCallable>()) {
            return const_member->invoke(args...);
        }
        // This should never happen if the function is properly constructed
        return default_return_helper<R>();
    }
    
    explicit operator bool() const {
        return !mStorage.empty();
    }
    
    void clear() {
        mStorage = Storage{};  // Reset to empty variant
    }
    
    bool operator==(const function& o) const {
        // For simplicity, just check if both are empty or both are non-empty
        // Full equality would require more complex comparison logic
        return mStorage.empty() == o.mStorage.empty();
    }
    
    bool operator!=(const function& o) const {
        return !(*this == o);
    }

private:
    // Helper for small lambdas/functors - inline storage
    template <typename F>
    void construct_lambda_or_functor(F f, true_type /* small */) {
        mStorage = InlinedLambda(fl::move(f));
    }
    
    // Helper for large lambdas/functors - heap storage
    template <typename F>
    void construct_lambda_or_functor(F f, false_type /* large */) {
        mStorage = fl::shared_ptr<CallableBase>(fl::make_shared<Callable<F>>(fl::move(f)));
    }
};


//----------------------------------------------------------------------------
// function_list: Container for managing multiple callbacks with add/remove
//----------------------------------------------------------------------------

// Primary template declaration with static assertion for invalid usage
// Only specializations for function signatures (e.g., function_list<void(Args...)>) are valid
template <typename T>
class function_list {
    static_assert(fl::is_same<T, void>::value && !fl::is_same<T, void>::value,
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

        FunctionEntry() : id(0), priority(0), fn() {}
        FunctionEntry(int idParam, int priorityParam, FunctionType fnParam)
            : id(idParam), priority(priorityParam), fn(fnParam) {}
    };

    using FunctionVector = fl::vector<FunctionEntry>;

    FunctionVector mFunctions;
    int mIdCounter = 0;

    bool mNeedsCompact = false;  // True when functions have been cleared during invocation

  public:
    function_list() = default;
    ~function_list() = default;

    int add(function<void(Args...)> fn, int priority = 0) {
        int id = mIdCounter++;
        mFunctions.push_back(FunctionEntry(id, priority, fn));
        return id;
    }

    void remove(int id) {
        // During invocation: clear the function for deferred removal
        for (size_t i = 0; i < mFunctions.size(); ++i) {
            if (mFunctions[i].id == id) {
                mFunctions[i].fn.clear();
                mNeedsCompact = true;
            }
        }
    }

    void clear() {
        mFunctions.clear();
    }

    // Compact the storage by removing invalid (cleared) callbacks
    // Used internally after invocation if removals occurred
    void compact() {
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
    fl::size size() const {
        fl::size count = 0;
        for (const auto& entry : mFunctions) {
            if (entry.fn) {
                ++count;
            }
        }
        return count;
    }
    bool empty() const { return size() == 0; }

    // Boolean conversion for if (callback) checks
    explicit operator bool() const { return !empty(); }

    void invoke(Args... args) {
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
        fl::sort(priorities.begin(), priorities.end(), [](int a, int b) { return a > b; });

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
    void operator()(Args... args) {
        invoke(args...);
    }
};

// Partial specialization for non-void return types: function_list<R(Args...)> where R != void
// Triggers a compile-time error when attempting to use non-void return types
template <typename R, typename... Args>
class function_list<R(Args...)> {
    static_assert(fl::is_same<R, void>::value,
                  "function_list only supports void return type. "
                  "Use function_list<void(Args...)> instead of function_list<ReturnType(Args...)>.");
};

} // namespace fl

FL_DISABLE_WARNING_POP
