#pragma once
#include "fl/template_magic.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

namespace fl {

// Add is_void trait that's missing from type_traits.h
template <typename T> struct is_void {
    static constexpr bool value = false;
};

template <> struct is_void<void> {
    static constexpr bool value = true;
};

//----------------------------------------------------------------------------
// Type-erased function implementation with small object optimization
// function<R(Args...)>: avoids heap allocation for small callables
// Supports free functions, lambdas/functors, member functions (const & non‑const)
//----------------------------------------------------------------------------
template <typename> class function;

template <typename R, typename... Args> class function<R(Args...)> {
  private:
    // Aligned storage for small object optimization (similar to InlinedMemoryBlock in vector.h)
    typedef uintptr_t MemoryType;
    static constexpr size_t STORAGE_CAPACITY = 3;  // Store up to 3 pointer-sized objects
    static constexpr size_t TOTAL_BYTES = STORAGE_CAPACITY * sizeof(MemoryType);
    static constexpr size_t STORAGE_ALIGN = sizeof(MemoryType);
    
    // Properly aligned storage block
    alignas(STORAGE_ALIGN) MemoryType storage_[STORAGE_CAPACITY];
    
    // Type-erased function pointers for operations
    R (*invoker_)(const void* storage, Args... args);
    void (*destructor_)(void* storage);
    void (*copy_constructor_)(void* dest, const void* src);
    void (*move_constructor_)(void* dest, void* src);
    
    bool uses_heap_;
    
    // Get properly aligned memory pointer
    void* get_storage() {
        return static_cast<void*>(storage_);
    }
    
    const void* get_storage() const {
        return static_cast<const void*>(storage_);
    }
    
    // Helper to determine if a type fits in our storage
    template<typename T>
    static constexpr bool fits_in_storage() {
        return sizeof(T) <= TOTAL_BYTES && alignof(T) <= STORAGE_ALIGN;
    }
    
    // Helper templates for void return type handling
    template<typename Func, typename RetType>
    struct invoke_small_helper {
        static RetType call(const void* storage, Args... args) {
            const Func* f = static_cast<const Func*>(storage);
            return (*f)(args...);
        }
    };
    
    template<typename Func>
    struct invoke_small_helper<Func, void> {
        static void call(const void* storage, Args... args) {
            const Func* f = static_cast<const Func*>(storage);
            (*f)(args...);
        }
    };
    
    template<typename Func, typename RetType>
    struct invoke_heap_helper {
        static RetType call(const void* storage, Args... args) {
            const Func* const* f_ptr = static_cast<const Func* const*>(storage);
            return (**f_ptr)(args...);
        }
    };
    
    template<typename Func>
    struct invoke_heap_helper<Func, void> {
        static void call(const void* storage, Args... args) {
            const Func* const* f_ptr = static_cast<const Func* const*>(storage);
            (**f_ptr)(args...);
        }
    };
    
    // Type-erased operations for small objects stored inline
    template<typename Func>
    static R invoke_small(const void* storage, Args... args) {
        return invoke_small_helper<Func, R>::call(storage, args...);
    }
    
    template<typename Func>
    static void destruct_small(void* storage) {
        static_cast<Func*>(storage)->~Func();
    }
    
    template<typename Func>
    static void copy_construct_small(void* dest, const void* src) {
        new (dest) Func(*static_cast<const Func*>(src));
    }
    
    template<typename Func>
    static void move_construct_small(void* dest, void* src) {
        new (dest) Func(fl::move(*static_cast<Func*>(src)));
    }
    
    // Type-erased operations for heap-allocated objects
    template<typename Func>
    static R invoke_heap(const void* storage, Args... args) {
        return invoke_heap_helper<Func, R>::call(storage, args...);
    }
    
    template<typename Func>
    static void destruct_heap(void* storage) {
        Func** f_ptr = static_cast<Func**>(storage);
        delete *f_ptr;
    }
    
    template<typename Func>
    static void copy_construct_heap(void* dest, const void* src) {
        const Func* const* f_ptr = static_cast<const Func* const*>(src);
        Func** dest_ptr = static_cast<Func**>(dest);
        *dest_ptr = new Func(**f_ptr);
    }
    
    template<typename Func>
    static void move_construct_heap(void* dest, void* src) {
        Func** src_ptr = static_cast<Func**>(src);
        Func** dest_ptr = static_cast<Func**>(dest);
        *dest_ptr = *src_ptr;
        *src_ptr = nullptr;
    }

  public:
    function() 
        : invoker_(nullptr), destructor_(nullptr), 
          copy_constructor_(nullptr), move_constructor_(nullptr), uses_heap_(false) {
        // Initialize storage to zero
        for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
            storage_[i] = 0;
        }
    }
    
    ~function() {
        if (destructor_) {
            destructor_(get_storage());
        }
    }

    function(const function& other) 
        : invoker_(other.invoker_), destructor_(other.destructor_),
          copy_constructor_(other.copy_constructor_), move_constructor_(other.move_constructor_),
          uses_heap_(other.uses_heap_) {
        // Initialize storage to zero
        for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
            storage_[i] = 0;
        }
        if (copy_constructor_) {
            copy_constructor_(get_storage(), other.get_storage());
        }
    }

    function(function&& other) noexcept
        : invoker_(other.invoker_), destructor_(other.destructor_),
          copy_constructor_(other.copy_constructor_), move_constructor_(other.move_constructor_),
          uses_heap_(other.uses_heap_) {
        // Initialize storage to zero
        for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
            storage_[i] = 0;
        }
        if (move_constructor_) {
            move_constructor_(get_storage(), other.get_storage());
        }
        other.invoker_ = nullptr;
        other.destructor_ = nullptr;
        other.copy_constructor_ = nullptr;
        other.move_constructor_ = nullptr;
    }

    function& operator=(const function& other) {
        if (this != &other) {
            // Destroy current content
            if (destructor_) {
                destructor_(get_storage());
            }
            
            // Copy from other
            invoker_ = other.invoker_;
            destructor_ = other.destructor_;
            copy_constructor_ = other.copy_constructor_;
            move_constructor_ = other.move_constructor_;
            uses_heap_ = other.uses_heap_;
            
            // Initialize storage to zero
            for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
                storage_[i] = 0;
            }
            
            if (copy_constructor_) {
                copy_constructor_(get_storage(), other.get_storage());
            }
        }
        return *this;
    }

    function& operator=(function&& other) noexcept {
        if (this != &other) {
            // Destroy current content
            if (destructor_) {
                destructor_(get_storage());
            }
            
            // Move from other
            invoker_ = other.invoker_;
            destructor_ = other.destructor_;
            copy_constructor_ = other.copy_constructor_;
            move_constructor_ = other.move_constructor_;
            uses_heap_ = other.uses_heap_;
            
            // Initialize storage to zero
            for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
                storage_[i] = 0;
            }
            
            if (move_constructor_) {
                move_constructor_(get_storage(), other.get_storage());
            }
            
            other.invoker_ = nullptr;
            other.destructor_ = nullptr;
            other.copy_constructor_ = nullptr;
            other.move_constructor_ = nullptr;
        }
        return *this;
    }

    // Generic constructor for lambdas, free functions, functors
    template <typename Func,
              typename = enable_if_t<!is_member_function_pointer<Func>::value &&
                                    !is_same<decay_t<Func>, function>::value>>
    function(Func&& f) {
        using DecayedFunc = decay_t<Func>;
        
        // Initialize storage to zero
        for (size_t i = 0; i < STORAGE_CAPACITY; ++i) {
            storage_[i] = 0;
        }
        
        if (fits_in_storage<DecayedFunc>()) {
            // Store inline
            new (get_storage()) DecayedFunc(fl::forward<Func>(f));
            invoker_ = &invoke_small<DecayedFunc>;
            destructor_ = &destruct_small<DecayedFunc>;
            copy_constructor_ = &copy_construct_small<DecayedFunc>;
            move_constructor_ = &move_construct_small<DecayedFunc>;
            uses_heap_ = false;
        } else {
            // Store on heap
            DecayedFunc** f_ptr = reinterpret_cast<DecayedFunc**>(get_storage());
            *f_ptr = new DecayedFunc(fl::forward<Func>(f));
            invoker_ = &invoke_heap<DecayedFunc>;
            destructor_ = &destruct_heap<DecayedFunc>;
            copy_constructor_ = &copy_construct_heap<DecayedFunc>;
            move_constructor_ = &move_construct_heap<DecayedFunc>;
            uses_heap_ = true;
        }
    }

    // Helper for member function calls
    template<typename C, typename RetType>
    struct mem_call_helper {
        static RetType call(C* obj, R (C::*mf)(Args...), Args... args) {
            return (obj->*mf)(args...);
        }
    };
    
    template<typename C>
    struct mem_call_helper<C, void> {
        static void call(C* obj, R (C::*mf)(Args...), Args... args) {
            (obj->*mf)(args...);
        }
    };
    
    template<typename C, typename RetType>
    struct const_mem_call_helper {
        static RetType call(const C* obj, R (C::*mf)(Args...) const, Args... args) {
            return (obj->*mf)(args...);
        }
    };
    
    template<typename C>
    struct const_mem_call_helper<C, void> {
        static void call(const C* obj, R (C::*mf)(Args...) const, Args... args) {
            (obj->*mf)(args...);
        }
    };

    // Non-const member function
    template <typename C>
    function(R (C::*mf)(Args...), C* obj) {
        using MemberFunc = R (C::*)(Args...);
        struct MemCallable {
            C* obj;
            MemberFunc mf;
            R operator()(Args... args) const {
                return mem_call_helper<C, R>::call(obj, mf, args...);
            }
        };
        
        *this = function(MemCallable{obj, mf});
    }

    // Const member function
    template <typename C>
    function(R (C::*mf)(Args...) const, const C* obj) {
        using MemberFunc = R (C::*)(Args...) const;
        struct ConstMemCallable {
            const C* obj;
            MemberFunc mf;
            R operator()(Args... args) const {
                return const_mem_call_helper<C, R>::call(obj, mf, args...);
            }
        };
        
        *this = function(ConstMemCallable{obj, mf});
    }

    // Invocation
    R operator()(Args... args) const { 
        return invoker_(get_storage(), args...); 
    }

    explicit operator bool() const { 
        return invoker_ != nullptr; 
    }

    bool operator==(const function& other) const {
        // Simple pointer comparison for now - could be enhanced
        return invoker_ == other.invoker_ && 
               (invoker_ == nullptr || 
                (uses_heap_ == other.uses_heap_ && 
                 (uses_heap_ ? 
                  *reinterpret_cast<const void* const*>(get_storage()) == *reinterpret_cast<const void* const*>(other.get_storage()) :
                  get_storage() == other.get_storage())));
    }

    bool operator!=(const function& other) const {
        return !(*this == other);
    }
};

} // namespace fl

#pragma GCC diagnostic pop
