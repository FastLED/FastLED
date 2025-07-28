#include "fl/random.h"
#include "fl/thread_local.h"

namespace fl {

// Global default random number generator instance
// Use function with static local to avoid global constructor
fl_random& default_random() {
    static ThreadLocal<fl_random> instance;
    return instance.access();
}

} // namespace fl 
