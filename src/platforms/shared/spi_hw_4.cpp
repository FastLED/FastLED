/// @file spi_hw_4.cpp
/// @brief Direct instance injection for 4-lane hardware SPI
///
/// Platform-specific implementations register their instances directly
/// during static initialization via registerInstance().

#include "platforms/shared/spi_hw_4.h"
#include "fl/compiler_control.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw4>>& getRegistry() {
        static fl::vector<fl::shared_ptr<SpiHw4>> registry;
        return registry;
    }
}

/// Register a platform-specific instance
/// Called by platform implementations during static initialization
void SpiHw4::registerInstance(fl::shared_ptr<SpiHw4> instance) {
    if (instance) {
        getRegistry().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw4::removeInstance(const fl::shared_ptr<SpiHw4>& instance) {
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
void SpiHw4::clearInstances() {
    getRegistry().clear();
}

/// Get all registered instances
/// This is moved out of the header to avoid __cxa_guard conflicts on some platforms
const fl::vector<fl::shared_ptr<SpiHw4>>& SpiHw4::getAll() {
    return getRegistry();
}

}  // namespace fl
