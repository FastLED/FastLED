/// @file spi_hw_16.cpp
/// @brief Implementation of 16-lane hardware SPI registry
///
/// This provides the static registry for 16-lane SPI hardware instances.
/// Platform-specific implementations register their instances at static initialization time.

#include "platforms/shared/spi_hw_16.h"
#include "fl/compiler_control.h"

namespace fl {

/// Static storage for registered instances (moved to .cpp to avoid __cxa_guard conflicts)
static fl::vector<fl::shared_ptr<SpiHw16>>& getRegistry() {
    static fl::vector<fl::shared_ptr<SpiHw16>> instances;
    return instances;
}

/// Get all available 16-lane hardware SPI devices on this platform
const fl::vector<fl::shared_ptr<SpiHw16>>& SpiHw16::getAll() {
    return getRegistry();
}

/// Register a platform-specific instance
void SpiHw16::registerInstance(fl::shared_ptr<SpiHw16> instance) {
    if (instance) {
        getRegistry().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw16::removeInstance(const fl::shared_ptr<SpiHw16>& instance) {
    auto& registry = getRegistry();
    for (size_t i = 0; i < registry.size(); ++i) {
        if (registry[i] == instance) {
            registry.erase(registry.begin() + i);
            return true;
        }
    }
    return false;
}

/// Clear all registered instances (primarily for testing)
void SpiHw16::clearInstances() {
    getRegistry().clear();
}

}  // namespace fl
