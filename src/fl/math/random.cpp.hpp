#include "fl/math/random.h"
#include "fl/stl/singleton.h"
#include "fl/stl/mutex.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace {

struct LockedRandom {
    fl::mutex mtx;
    math::random rng;
};

} // namespace

math::random& default_random() FL_NO_EXCEPT {
    return Singleton<LockedRandom>::instance().rng;
}

} // namespace fl
