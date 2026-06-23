// IWYU pragma: private

/// @file spi_hw_2.cpp
/// @brief Direct instance injection for 2-lane hardware SPI
///
/// Platform-specific implementations register their instances via lazy initialization
/// on first access to getAll().

#include "platforms/shared/spi_hw_2.h"
#include "fl/stl/compiler_control.h"
#include "platforms/init_spi_hw.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw2>>& getRegistrySpiHw2() FL_NOEXCEPT {
        static fl::vector<fl::shared_ptr<SpiHw2>> registry;
        return registry;
    }
}

/// Register a platform-specific instance
/// Called by platform implementations during static initialization
void SpiHw2::registerInstance(fl::shared_ptr<SpiHw2> instance) FL_NOEXCEPT {
    if (instance) {
        getRegistrySpiHw2().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw2::removeInstance(const fl::shared_ptr<SpiHw2>& instance) FL_NOEXCEPT {
    auto& registry = getRegistrySpiHw2();
    for (size_t i = 0; i < registry.size(); ++i) {
        if (registry[i] == instance) {
            registry.erase(registry.begin() + i);
            return true;
        }
    }
    return false;
}

/// Clear all registered instances (primarily for testing)
void SpiHw2::clearInstances() FL_NOEXCEPT {
    getRegistrySpiHw2().clear();
}

/// Get all registered instances
/// Implementation moved to cpp to avoid Teensy 3.x __cxa_guard linkage issues
const fl::vector<fl::shared_ptr<SpiHw2>>& SpiHw2::getAll() FL_NOEXCEPT {
    // Lazy initialization of platform-specific SPI instances
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (!sInitialized) {
        sInitialized = true;
        platforms::initSpiHardware();  // Unified initialization (replaces initSpiHw2Instances)
    }
    return getRegistrySpiHw2();
}

}  // namespace fl
