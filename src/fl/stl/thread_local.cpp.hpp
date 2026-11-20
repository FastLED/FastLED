#include "fl/stl/thread_local.h"

#if FASTLED_USE_THREAD_LOCAL
#include "fl/log/log.h"

namespace fl {
namespace detail {

void thread_local_fallback_warn() FL_NO_EXCEPT {
    FL_WARN_ONCE("ThreadLocal: pthread_key_create/pthread_setspecific failed; "
                 "affected threads share one fallback value");
}

} // namespace detail
} // namespace fl
#endif // FASTLED_USE_THREAD_LOCAL
