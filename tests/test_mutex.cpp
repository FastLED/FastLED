#include "test.h"

#include "fl/mutex.h"


TEST_CASE("Mutex reentrant") {
    // Tests that the lock can be acquired multiple times by the same thread.
    {
        fl::mutex m;
        fl::lock_guard<fl::mutex> lock(m);
        {
            // This will deadlock.
            bool acquired_recursively = m.try_lock();
            CHECK_EQ(acquired_recursively, true);
            m.unlock();
        }
    }
}
