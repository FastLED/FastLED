#pragma once

// A structure that will execute a cleanup function on scope exit unless
// release() is called.
struct Cleanup {
    typedef void (*void_fn_t)(void*);
    template<typename CleanupFunction>
    Cleanup(CleanupFunction fn, void *arg) : mFn((void_fn_t)fn), mArg(arg) {}
    
    ~Cleanup() {
        maybe_cleanup();
    }

    void release() {
        mFn = nullptr;
    }

    void maybe_cleanup() {
        if (mFn) {
            mFn(mArg);
            mFn = nullptr;
        }
    }
    
    void_fn_t mFn = nullptr;
    void *mArg = nullptr;
};
