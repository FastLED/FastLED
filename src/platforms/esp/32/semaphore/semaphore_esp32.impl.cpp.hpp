// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/semaphore_esp32.h" // ok no header - public declaration remains in parent directory.
#include "platforms/esp/32/semaphore/semaphore_esp32.impl.hpp"

#ifdef FL_IS_ESP32
namespace fl { namespace platforms {

// Binary semaphore (max value = 1)
template class CountingSemaphoreESP32<1>;

// Explicit instantiation of try_acquire_for for common duration types
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::milli>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::micro>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::nano>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::ratio<1>>&);  // okay std namespace

// Explicit instantiation of try_acquire_until for common clock types
template bool CountingSemaphoreESP32<1>::try_acquire_until(const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_until(const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} } // namespace fl::platforms
#endif // FL_IS_ESP32
