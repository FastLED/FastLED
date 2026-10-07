// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32
#include "platforms/esp/32/semaphore_esp32.h" // ok no header - public declarations remain in platform semaphore header.
#include "platforms/esp/32/semaphore/semaphore_esp32.impl.hpp"

namespace fl { namespace platforms {

// Common counting semaphore values
template class CountingSemaphoreESP32<2>;
template class CountingSemaphoreESP32<5>;
template class CountingSemaphoreESP32<10>;
template class CountingSemaphoreESP32<100>;
template class CountingSemaphoreESP32<1000>;

} } // namespace fl::platforms
#endif // FL_IS_ESP32
