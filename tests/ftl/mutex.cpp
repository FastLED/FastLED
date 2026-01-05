#include "test.h"

#include "fl/stl/mutex.h"


TEST_CASE("Mutex reentrant") {
    // Tests that the lock can be acquired multiple times by the same thread.
    {
        fl::recursive_mutex m;
        fl::unique_lock<fl::recursive_mutex> lock(m);
        {
            // This should succeed with recursive_mutex
            bool acquired_recursively = m.try_lock();
            CHECK_EQ(acquired_recursively, true);
            m.unlock();
        }
    }
}
