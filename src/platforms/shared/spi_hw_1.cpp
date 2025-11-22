/// @file spi_hw_1.cpp
/// @brief Direct instance injection for 1-lane hardware SPI
///
/// Platform-specific implementations register their instances directly
/// during static initialization via registerInstance().

#include "platforms/shared/spi_hw_1.h"
#include "fl/compiler_control.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw1>>& getRegistry() {
        static fl::vector<fl::shared_ptr<SpiHw1>> registry;
        return registry;
    }
}

/// Register a platform-specific instance
/// Called by platform implementations during static initialization
void SpiHw1::registerInstance(fl::shared_ptr<SpiHw1> instance) {
    if (instance) {
        getRegistry().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw1::removeInstance(const fl::shared_ptr<SpiHw1>& instance) {
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
void SpiHw1::clearInstances() {
    getRegistry().clear();
}

/// Get all registered instances
/// Implementation moved to cpp to avoid Teensy 3.x __cxa_guard linkage issues
const fl::vector<fl::shared_ptr<SpiHw1>>& SpiHw1::getAll() {
    return getRegistry();
}

}  // namespace fl
