#include "fl/stl/thread_local.h"

#if FASTLED_USE_THREAD_LOCAL
#include "fl/log/log.h"
#include "fl/stl/atomic.h"

namespace fl {
namespace detail {

void thread_local_fallback_warn() FL_NO_EXCEPT {
    // Atomic once-flag: several ThreadLocal objects may degrade concurrently.
    static fl::atomic<bool> warned(false);
    if (!warned.exchange(true, fl::memory_order_acq_rel)) {
        FL_WARN("ThreadLocal: pthread_key_create/pthread_setspecific failed; "
                "affected threads share one fallback value");
    }
}

} // namespace detail
} // namespace fl
#endif // FASTLED_USE_THREAD_LOCAL
