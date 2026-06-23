// IWYU pragma: private

/// @file spi_hw_1.cpp
/// @brief Direct instance injection for 1-lane hardware SPI
///
/// Platform-specific implementations register their instances via lazy initialization
/// on first access to getAll().

#include "platforms/shared/spi_hw_1.h"
#include "fl/stl/compiler_control.h"
#include "platforms/init_spi_hw.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace {
    /// Static registry of all registered instances
    fl::vector<fl::shared_ptr<SpiHw1>>& getRegistrySpiHw1() FL_NOEXCEPT {
        static fl::vector<fl::shared_ptr<SpiHw1>> registry;
        return registry;
    }
}

/// Register a platform-specific instance
/// Called by platform implementations during static initialization
void SpiHw1::registerInstance(fl::shared_ptr<SpiHw1> instance) FL_NOEXCEPT {
    if (instance) {
        getRegistrySpiHw1().push_back(instance);
    }
}

/// Remove a registered instance
bool SpiHw1::removeInstance(const fl::shared_ptr<SpiHw1>& instance) FL_NOEXCEPT {
    auto& registry = getRegistrySpiHw1();
    for (size_t i = 0; i < registry.size(); ++i) {
        if (registry[i] == instance) {
            registry.erase(registry.begin() + i);
            return true;
        }
    }
    return false;
}

/// Clear all registered instances (primarily for testing)
void SpiHw1::clearInstances() FL_NOEXCEPT {
    getRegistrySpiHw1().clear();
}

/// Get all registered instances
/// Implementation moved to cpp to avoid Teensy 3.x __cxa_guard linkage issues
const fl::vector<fl::shared_ptr<SpiHw1>>& SpiHw1::getAll() FL_NOEXCEPT {
    // Lazy initialization of platform-specific SPI instances
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (!sInitialized) {
        sInitialized = true;
        platforms::initSpiHardware();  // Unified initialization (replaces initSpiHw1Instances)
    }
    return getRegistrySpiHw1();
}

}  // namespace fl
