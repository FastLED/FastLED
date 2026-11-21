#include "fl/stl/thread_local.h"
#include "fl/stl/vector.h"
#include "test.h"

#if FASTLED_USE_THREAD_LOCAL
#include "fl/stl/thread.h"
#endif

FL_TEST_FILE(FL_FILEPATH) {

#if FASTLED_USE_THREAD_LOCAL

// Regression test for #5310: when pthread_key_create fails, access() must use
// a shared fallback slot instead of dereferencing null. Failure is injected
// for real by exhausting the process's pthread keys (PTHREAD_KEYS_MAX) only
// while the ThreadLocal is constructed. The window is a few microseconds on
// this thread with no allocation inside the loop (capacity reserved first).
//
// This test lives in its own file because each tests/**/*.cpp runs as its own
// process: while every pthread key is hoarded, any other ThreadLocal first
// constructed in the process would get the shared fallback slot for the rest
// of the run (#5323). Keep it the only test case here and avoid FL_SUBCASE
// (fl_unittest.h lazily constructs a static ThreadLocal on first subcase).
//
// The sibling pthread_setspecific failure branch in createStorage() is
// deliberately not injected (#5322). Its only extra code is freeing the
// storage it just allocated and falling through to fallbackStorage(), the
// same function this test exercises. With a valid key, setspecific fails only
// on ENOMEM, which cannot be forced portably without interposing libc
// symbols (blocked by macOS two-level namespaces, and absent on Windows).
// A test-only key-policy template parameter would widen ThreadLocalReal's
// type for every user and is not worth it for two lines of code.
FL_TEST_CASE("ThreadLocal - key creation failure falls back to a shared slot") {
    const int kMaxKeys = 1 << 16;
    fl::vector<pthread_key_t> hoard;
    hoard.reserve(kMaxKeys);
    bool exhausted = false;
    for (int i = 0; i < kMaxKeys; ++i) {
        pthread_key_t k;
        if (pthread_key_create(&k, nullptr) != 0) {
            exhausted = true;
            break;
        }
        hoard.push_back(k);
    }
    // Constructed with no keys left: key creation fails inside the ctor.
    fl::ThreadLocal<int> with_default(5);
    fl::ThreadLocal<int> no_default;
    for (pthread_key_t k : hoard) {
        pthread_key_delete(k);
    }
    if (!exhausted) {
        FL_MESSAGE("pthread keys could not be exhausted; skipping");
        return;
    }

    FL_CHECK_EQ(with_default.access(), 5);
    FL_CHECK_EQ(no_default.access(), 0);
    with_default.set(7);
    no_default = 8;

    // Every thread shares the one fallback slot (ThreadLocalFake semantics).
    int seen_default = 0;
    int seen_plain = 0;
    fl::thread t([&]() {
        seen_default = with_default.access();
        seen_plain = no_default.access();
        with_default.set(9);
    });
    t.join();
    FL_CHECK_EQ(seen_default, 7);
    FL_CHECK_EQ(seen_plain, 8);
    FL_CHECK_EQ(with_default.access(), 9);
    // Destructors free the fallback slots (LSAN in --debug checks this).
}

#endif  // FASTLED_USE_THREAD_LOCAL

} // FL_TEST_FILE
