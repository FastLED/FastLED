#include "fl/random.h"

namespace fl {

// Global default random number generator instance
// Use function with static local to avoid global constructor
fl_random& default_random() {
    static fl_random instance;
    return instance;
}

} // namespace fl 
