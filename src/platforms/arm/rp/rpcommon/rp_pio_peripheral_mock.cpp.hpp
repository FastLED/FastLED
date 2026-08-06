// IWYU pragma: private

/// @file rp_pio_peripheral_mock.cpp.hpp
/// @brief Test-only singleton storage for RP PIO peripheral mocks.

#include "platforms/is_platform.h"

#if defined(FASTLED_STUB_IMPL)

#include "fl/stl/singleton.h"
#include "platforms/arm/rp/rpcommon/rp_pio_peripheral_mock.h"

namespace fl {

RpPioTxPeripheralMock& RpPioTxPeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<RpPioTxPeripheralMock>::instance();
}

RpPioSpiPeripheralMock& RpPioSpiPeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<RpPioSpiPeripheralMock>::instance();
}

}  // namespace fl

#endif  // defined(FASTLED_STUB_IMPL)
