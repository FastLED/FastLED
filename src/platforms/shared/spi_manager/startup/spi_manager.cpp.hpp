// IWYU pragma: private
// ok no header - public declarations remain in platforms/shared/spi_manager.h

#include "platforms/shared/spi_manager.h"

namespace fl {

SPIBusManager::SPIBusManager() FL_NO_EXCEPT : mNumBuses(0), mInitialized(false) {
    for (u8 i = 0; i < MAX_BUSES; i++) {
        mBuses[i] = SPIBusInfo{};
    }
}

SPIBusManager::~SPIBusManager() {
    // Note: Do NOT call reset() here!
    // During static destruction, hardware resources (like SpiHw1Stub static instances)
    // may have already been destroyed, causing crashes when we try to call end() on them.
    // This is especially problematic in test environments where SPIBusManager, SpiHw1Stub,
    // and other static objects are destroyed in undefined order.
    //
    // Device destructors already handle cleanup via unregisterDevice(), so this is safe.
}

SPIBusManager& getSPIBusManager() FL_NO_EXCEPT {
    static SPIBusManager instance;
    return instance;
}

} // namespace fl
