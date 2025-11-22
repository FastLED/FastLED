/// @file spi_hw_8.cpp
/// @brief Direct instance injection for 8-lane hardware SPI
///
/// Platform-specific implementations register their instances directly
/// during static initialization via registerInstance().

#include "platforms/shared/spi_hw_8.h"
#include "fl/compiler_control.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw8>>& getRegistry() {
        static fl::vector<fl::shared_ptr<SpiHw8>> registry;
        return registry;
    }
}

/// Register a platform-specific instance
/// Called by platform implementations during static initialization
void SpiHw8::registerInstance(fl::shared_ptr<SpiHw8> instance) {
    if (instance) {
        getRegistry().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw8::removeInstance(const fl::shared_ptr<SpiHw8>& instance) {
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
void SpiHw8::clearInstances() {
    getRegistry().clear();
}

/// Get all registered instances
/// This is moved out of the header to avoid __cxa_guard conflicts on some platforms
const fl::vector<fl::shared_ptr<SpiHw8>>& SpiHw8::getAll() {
    return getRegistry();
}

}  // namespace fl
