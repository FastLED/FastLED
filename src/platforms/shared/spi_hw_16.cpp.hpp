/// @file spi_hw_16.cpp
/// @brief Implementation of 16-lane hardware SPI registry
///
/// This provides the static registry for 16-lane SPI hardware instances.
/// Platform-specific implementations register their instances via lazy initialization
/// on first access to getAll().

#include "platforms/shared/spi_hw_16.h"
#include "fl/compiler_control.h"
#include "platforms/init_spi_hw_16.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw16>>& getRegistrySpiHw16() {
        static fl::vector<fl::shared_ptr<SpiHw16>> registry;
        return registry;
    }
}

/// Get all available 16-lane hardware SPI devices on this platform
const fl::vector<fl::shared_ptr<SpiHw16>>& SpiHw16::getAll() {
    // Lazy initialization of platform-specific SPI instances
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (!sInitialized) {
        sInitialized = true;
        platform::initSpiHw16Instances();
    }
    return getRegistrySpiHw16();
}

/// Register a platform-specific instance
void SpiHw16::registerInstance(fl::shared_ptr<SpiHw16> instance) {
    if (instance) {
        getRegistrySpiHw16().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw16::removeInstance(const fl::shared_ptr<SpiHw16>& instance) {
    auto& registry = getRegistrySpiHw16();
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
    getRegistrySpiHw16().clear();
}

}  // namespace fl
