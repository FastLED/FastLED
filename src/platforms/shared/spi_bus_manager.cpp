#include "spi_bus_manager.h"

#include "fastled_config.h"  // For FASTLED_FORCE_SOFTWARE_SPI
#include "fl/stl/vector.h"
#include "fl/warn.h"
#include "fl/stl/strstream.h"  // Required for FL_WARN_FMT
#include "fl/dbg.h"
#include "fl/log.h"

#include "fl/stl/limits.h"
#include "fl/fastpin.h"
#include "fl/stl/math.h"

// Arduino defines max/min as macros, which conflicts with fl::numeric_limits<T>::max()
// Undefine them here since we use the FastLED fl:: equivalents (fl_min/fl_max)
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace fl {

// ============================================================================
// SPIBusManager Implementation
// ============================================================================

SPIBusManager::SPIBusManager() : mNumBuses(0), mInitialized(false) {
    for (uint8_t i = 0; i < MAX_BUSES; i++) {
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

SPIBusHandle SPIBusManager::registerDevice(uint8_t clock_pin, uint8_t data_pin, uint32_t requested_speed_hz, void* controller) {
    if (!controller) {
        FL_WARN("SPIBusManager: nullptr controller pointer");
        return SPIBusHandle();
    }

    // Find or create bus for this clock pin
    SPIBusInfo* bus = getOrCreateBus(clock_pin);
    if (!bus) {
        FL_WARN_FMT("SPIBusManager: Too many different clock pins (max " << MAX_BUSES << ")");
        return SPIBusHandle();
    }

    // Check if we can add another device to this bus
    if (bus->num_devices >= 16) {
        FL_WARN_FMT("SPIBusManager: Too many devices on clock pin " << clock_pin << " (max 16)");
        return SPIBusHandle();
    }

    // Add device to bus
    uint8_t device_idx = bus->num_devices;
    bus->devices[device_idx].clock_pin = clock_pin;
    bus->devices[device_idx].data_pin = data_pin;
    bus->devices[device_idx].controller = controller;
    bus->devices[device_idx].lane_id = device_idx;  // Tentative assignment
    bus->devices[device_idx].requested_speed_hz = requested_speed_hz;
    bus->devices[device_idx].is_enabled = true;  // Enabled by default
    bus->devices[device_idx].is_allocated = true;  // Mark as allocated
    bus->num_devices++;

    // Return handle
    uint8_t bus_id = static_cast<uint8_t>(bus - mBuses);
    return SPIBusHandle(bus_id, device_idx);
}

bool SPIBusManager::unregisterDevice(SPIBusHandle handle) {
    if (!handle.is_valid || handle.bus_id >= mNumBuses) {
        return false;
    }

    SPIBusInfo& bus = mBuses[handle.bus_id];
    if (handle.lane_id >= 16) {
        return false;
    }

    SPIDeviceInfo& device = bus.devices[handle.lane_id];
    if (!device.is_allocated) {
        return false;  // Already deallocated
    }

    // Mark device as deallocated
    device.is_allocated = false;
    device.is_enabled = false;
    device.controller = nullptr;

    // Count remaining allocated devices
    uint8_t allocated_count = 0;
    for (uint8_t i = 0; i < bus.num_devices; i++) {
        if (bus.devices[i].is_allocated) {
            allocated_count++;
        }
    }

    // If no devices remain, release Quad-SPI hardware
    if (allocated_count == 0) {
        releaseBusHardware(bus);
    }

    return true;
}

bool SPIBusManager::initialize() {
    bool all_ok = true;

    // Initialize each bus that hasn't been initialized yet
    for (uint8_t i = 0; i < mNumBuses; i++) {
        if (!mBuses[i].is_initialized) {
            if (!initializeBus(mBuses[i])) {
                all_ok = false;
            }
        }
    }

    mInitialized = true;
    return all_ok;
}

void SPIBusManager::transmit(SPIBusHandle handle, const uint8_t* data, size_t length) {
    if (!handle.is_valid || handle.bus_id >= mNumBuses) {
        return;
    }

    SPIBusInfo& bus = mBuses[handle.bus_id];
    if (!bus.is_initialized) {
        return;
    }

    // Check if device is enabled
    if (handle.lane_id >= bus.num_devices || !bus.devices[handle.lane_id].is_enabled) {
        return;  // Silently skip disabled devices
    }

    // Route to appropriate backend
    switch (bus.bus_type) {
        case SPIBusType::SINGLE_SPI: {
            // Single SPI uses direct hardware writes (handled by proxy)
            // No buffering needed - proxy writes directly to ESP32SPIOutput
            break;
        }

        case SPIBusType::DUAL_SPI: {
            // Buffer data for this lane - will be interleaved in finalizeTransmission()
            // Ensure lane_buffers is sized correctly
            if (bus.lane_buffers.size() <= handle.lane_id) {
                bus.lane_buffers.resize(handle.lane_id + 1);
            }

            // Append data to this lane's buffer
            fl::vector<uint8_t>& lane_buffer = bus.lane_buffers[handle.lane_id];
            for (size_t i = 0; i < length; i++) {
                lane_buffer.push_back(data[i]);
            }
            break;
        }

        case SPIBusType::QUAD_SPI:
        case SPIBusType::OCTO_SPI:
        case SPIBusType::HEXADECA_SPI: {
            // Buffer data for this lane - will be interleaved in finalizeTransmission()
            // Ensure lane_buffers is sized correctly
            if (bus.lane_buffers.size() <= handle.lane_id) {
                bus.lane_buffers.resize(handle.lane_id + 1);
            }

            // Append data to this lane's buffer
            fl::vector<uint8_t>& lane_buffer = bus.lane_buffers[handle.lane_id];
            for (size_t i = 0; i < length; i++) {
                lane_buffer.push_back(data[i]);
            }
            break;
        }

        case SPIBusType::SOFT_SPI: {
            // Software SPI fallback implementation
            // Note: This requires Pin class to be available, which depends on platform-specific
            // headers being included before spi_bus_manager.h. In practice, when users include
            // FastLED.h, all necessary platform types are defined, so software SPI will work.
            // For testing/stub builds, this is a no-op (see softwareSPIWrite implementation).
            if (handle.lane_id < bus.num_devices) {
                const SPIDeviceInfo& device = bus.devices[handle.lane_id];
                softwareSPIWrite(device.clock_pin, device.data_pin, data, length);
            }
            break;
        }
    }
}

void SPIBusManager::waitComplete(SPIBusHandle handle) {
    if (!handle.is_valid || handle.bus_id >= mNumBuses) {
        return;
    }

    SPIBusInfo& bus = mBuses[handle.bus_id];
    if (!bus.is_initialized) {
        return;
    }

    // Use polymorphic interface - works for all SPI types
    if (bus.hw_controller) {
        bus.hw_controller->waitComplete();
    }
}

void SPIBusManager::finalizeTransmission(SPIBusHandle handle) {
    if (!handle.is_valid || handle.bus_id >= mNumBuses) {
        return;
    }

    SPIBusInfo& bus = mBuses[handle.bus_id];
    if (!bus.is_initialized) {
        return;  // Bus not initialized
    }

    // Only needed for multi-SPI modes (Dual-SPI, Quad-SPI, Octal-SPI, Hexadeca-SPI)
    if (bus.bus_type != SPIBusType::DUAL_SPI &&
        bus.bus_type != SPIBusType::QUAD_SPI &&
        bus.bus_type != SPIBusType::OCTO_SPI &&
        bus.bus_type != SPIBusType::HEXADECA_SPI) {
        return;
    }

    // Handle Dual-SPI (runtime detection via getAll())
    if (bus.bus_type == SPIBusType::DUAL_SPI) {
        if (!bus.hw_controller) {
            return;
        }

        // Find maximum lane size
        size_t max_size = 0;
        for (uint8_t i = 0; i < bus.num_devices && i < 2; i++) {
            if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
                max_size = fl::fl_max(max_size, bus.lane_buffers[i].size());
            }
        }

        if (max_size == 0) {
            return;  // No data to transmit
        }

        // Acquire DMA buffer (zero-copy API) - use polymorphic interface
        DMABuffer result = bus.hw_controller->acquireDMABuffer(max_size);
        if (!result.ok()) {
            FL_WARN_FMT("SPI Bus Manager: Failed to acquire DMA buffer for Dual-SPI: " << static_cast<int>(result.error()));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
        fl::span<uint8_t> dma_buf = result.data();

        // Prepare 2-lane data for transposer
        fl::optional<SPITransposer::LaneData> lane0, lane1;
        if (bus.num_devices > 0 && bus.devices[0].is_enabled && 0 < bus.lane_buffers.size()) {
            lane0 = SPITransposer::LaneData{
                fl::span<const uint8_t>(bus.lane_buffers[0].data(), bus.lane_buffers[0].size()),
                fl::span<const uint8_t>()  // No padding frame yet
            };
        }
        if (bus.num_devices > 1 && bus.devices[1].is_enabled && 1 < bus.lane_buffers.size()) {
            lane1 = SPITransposer::LaneData{
                fl::span<const uint8_t>(bus.lane_buffers[1].data(), bus.lane_buffers[1].size()),
                fl::span<const uint8_t>()  // No padding frame yet
            };
        }

        // Transpose lanes directly into DMA buffer (zero-copy!)
        const char* error = nullptr;
        if (!SPITransposer::transpose2(lane0, lane1, dma_buf, &error)) {
            FL_WARN_FMT("SPI Bus Manager: Dual transpose failed - " << (error ? error : "unknown error"));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }

        // Transmit via Dual-SPI hardware - use polymorphic interface
        bool transmit_ok = bus.hw_controller->transmit(TransmitMode::ASYNC);
        if (transmit_ok) {
            bus.hw_controller->waitComplete();
        } else {
            FL_WARN("SPI Bus Manager: Dual-SPI transmit failed");
        }

        // Clear lane buffers for next frame
        for (auto& lane_buffer : bus.lane_buffers) {
            lane_buffer.clear();
        }
        return;
    }

    // Handle Quad-SPI, Octal-SPI, and Hexadeca-SPI
    if (!bus.hw_controller) {
        return;
    }

    // Determine lane count based on bus type
    bool is_hexadeca_mode = (bus.bus_type == SPIBusType::HEXADECA_SPI);
    bool is_octal_mode = (bus.bus_type == SPIBusType::OCTO_SPI);

    // Find maximum lane size
    size_t max_size = 0;
    for (uint8_t i = 0; i < bus.num_devices; i++) {
        if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
            max_size = fl::fl_max(max_size, bus.lane_buffers[i].size());
        }
    }

    if (max_size == 0) {
        return;  // No data to transmit
    }

    // Determine lane count for buffer acquisition
    bool is_hexadeca = (bus.num_devices > 8 && bus.num_devices <= 16);
    bool is_octal = (bus.num_devices > 4 && bus.num_devices <= 8);

    // Acquire DMA buffer (zero-copy API)
    DMABuffer result;
    fl::span<uint8_t> dma_buf;
    if (is_hexadeca) {
        SpiHw16* hexadeca = static_cast<SpiHw16*>(bus.hw_controller.get());
        result = hexadeca->acquireDMABuffer(max_size);
        if (!result.ok()) {
            FL_WARN_FMT("SPI Bus Manager: Failed to acquire DMA buffer for Hexadeca-SPI: " << static_cast<int>(result.error()));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
        dma_buf = result.data();
    } else if (is_octal) {
        SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller.get());
        result = octal->acquireDMABuffer(max_size);
        if (!result.ok()) {
            FL_WARN_FMT("SPI Bus Manager: Failed to acquire DMA buffer for Octal-SPI: " << static_cast<int>(result.error()));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
        dma_buf = result.data();
    } else {
        SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller.get());
        result = quad->acquireDMABuffer(max_size);
        if (!result.ok()) {
            FL_WARN_FMT("SPI Bus Manager: Failed to acquire DMA buffer for Quad-SPI: " << static_cast<int>(result.error()));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
        dma_buf = result.data();
    }

    const char* error = nullptr;
    if (is_hexadeca) {
        // Prepare 16-lane data for transposer
        fl::optional<SPITransposer::LaneData> lanes[16];
        for (uint8_t i = 0; i < bus.num_devices && i < 16; i++) {
            if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
                lanes[i] = SPITransposer::LaneData{
                    fl::span<const uint8_t>(bus.lane_buffers[i].data(), bus.lane_buffers[i].size()),
                    fl::span<const uint8_t>()  // Zero-padding (universal fallback)
                };
            }
        }

        // Transpose lanes directly into DMA buffer (zero-copy!)
        if (!SPITransposer::transpose16(lanes, dma_buf, &error)) {
            FL_WARN_FMT("SPI Bus Manager: Hexadeca transpose failed - " << (error ? error : "unknown error"));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
    } else if (is_octal) {
        // Prepare 8-lane data for transposer
        fl::optional<SPITransposer::LaneData> lanes[8];
        for (uint8_t i = 0; i < bus.num_devices && i < 8; i++) {
            if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
                // Padding frame support: Currently uses zero-padding for synchronized latching.
                // Zero-padding ensures all lanes finish transmitting simultaneously, which is
                // the primary requirement for visual synchronization across parallel strips.
                //
                // Chipset-specific black LED patterns (e.g., APA102's {0xE0,0x00,0x00,0x00})
                // would be preferable but require type information not available in this
                // type-erased context (void* controller pointer). Zero-padding works universally
                // across all chipsets, though it may cause brief LED flashes on mismatched lengths.
                //
                // Design Note: To support chipset-specific padding, would need either:
                // 1. Virtual getPaddingFrame() method on base controller class (API change)
                // 2. Padding frame passed during device registration (requires user management)
                // 3. Ensure all strips in parallel group have identical LED counts (current best practice)
                lanes[i] = SPITransposer::LaneData{
                    fl::span<const uint8_t>(bus.lane_buffers[i].data(), bus.lane_buffers[i].size()),
                    fl::span<const uint8_t>()  // Zero-padding (universal fallback)
                };
            }
        }

        // Transpose lanes directly into DMA buffer (zero-copy!)
        if (!SPITransposer::transpose8(lanes, dma_buf, &error)) {
            FL_WARN_FMT("SPI Bus Manager: Octal transpose failed - " << (error ? error : "unknown error"));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
    } else {
        // Prepare 4-lane data for transposer
        fl::optional<SPITransposer::LaneData> lanes[4];
        for (uint8_t i = 0; i < bus.num_devices && i < 4; i++) {
            if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
                // Padding frame support: Currently uses zero-padding for synchronized latching.
                // Zero-padding ensures all lanes finish transmitting simultaneously, which is
                // the primary requirement for visual synchronization across parallel strips.
                //
                // Chipset-specific black LED patterns (e.g., APA102's {0xE0,0x00,0x00,0x00})
                // would be preferable but require type information not available in this
                // type-erased context (void* controller pointer). Zero-padding works universally
                // across all chipsets, though it may cause brief LED flashes on mismatched lengths.
                //
                // Design Note: To support chipset-specific padding, would need either:
                // 1. Virtual getPaddingFrame() method on base controller class (API change)
                // 2. Padding frame passed during device registration (requires user management)
                // 3. Ensure all strips in parallel group have identical LED counts (current best practice)
                lanes[i] = SPITransposer::LaneData{
                    fl::span<const uint8_t>(bus.lane_buffers[i].data(), bus.lane_buffers[i].size()),
                    fl::span<const uint8_t>()  // Zero-padding (universal fallback)
                };
            }
        }

        // Transpose lanes directly into DMA buffer (zero-copy!)
        if (!SPITransposer::transpose4(lanes[0], lanes[1], lanes[2], lanes[3], dma_buf, &error)) {
            FL_WARN_FMT("SPI Bus Manager: Quad transpose failed - " << (error ? error : "unknown error"));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }
    }

    // Transmit via Quad-SPI, Octal-SPI, or Hexadeca-SPI hardware
    bool transmit_ok = false;
    if (is_hexadeca_mode) {
        // 16-lane (Hexadeca-SPI)
        SpiHw16* hexadeca = static_cast<SpiHw16*>(bus.hw_controller.get());
        transmit_ok = hexadeca->transmit(TransmitMode::ASYNC);
        if (transmit_ok) {
            hexadeca->waitComplete();
        }
    } else if (is_octal_mode) {
        // 8-lane (Octal-SPI)
        SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller.get());
        transmit_ok = octal->transmit(TransmitMode::ASYNC);
        if (transmit_ok) {
            octal->waitComplete();
        }
    } else {
        // 4-lane (Quad-SPI)
        SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller.get());
        transmit_ok = quad->transmit(TransmitMode::ASYNC);
        if (transmit_ok) {
            quad->waitComplete();
        }
    }

    if (!transmit_ok) {
        FL_WARN_FMT("SPI Bus Manager: " << (is_hexadeca_mode ? "Hexadeca" : (is_octal_mode ? "Octal" : "Quad")) << "-SPI transmit failed");
    }

    // Clear lane buffers for next frame
    for (auto& lane_buffer : bus.lane_buffers) {
        lane_buffer.clear();
    }
}

bool SPIBusManager::isDeviceEnabled(SPIBusHandle handle) const {
    if (!handle.is_valid || handle.bus_id >= mNumBuses) {
        return false;
    }

    const SPIBusInfo& bus = mBuses[handle.bus_id];
    if (handle.lane_id >= bus.num_devices) {
        return false;
    }

    return bus.devices[handle.lane_id].is_enabled;
}

void SPIBusManager::reset() {
    FL_DBG("SPIBusManager: reset() called");
    // Save current mNumBuses before clearing
    uint8_t num_buses_to_clear = mNumBuses;
    // Set mNumBuses to 0 first to prevent re-entry issues
    mNumBuses = 0;
    mInitialized = false;

    // Only iterate through buses that were actually used
    for (uint8_t i = 0; i < num_buses_to_clear; i++) {
        FL_DBG("SPIBusManager: reset() checking bus " << static_cast<int>(i));
        // Clean up hardware controllers if allocated
        if (mBuses[i].is_initialized) {
            FL_DBG("SPIBusManager: reset() releasing bus " << static_cast<int>(i));
            releaseBusHardware(mBuses[i]);
            FL_DBG("SPIBusManager: reset() released bus " << static_cast<int>(i));
        }
        FL_DBG("SPIBusManager: reset() resetting bus info for bus " << static_cast<int>(i));
        // Explicitly clear vectors to avoid crashes during destruction
        FL_DBG("SPIBusManager: reset() clearing lane_buffers");
        mBuses[i].lane_buffers.clear();
        FL_DBG("SPIBusManager: reset() clearing interleaved_buffer");
        mBuses[i].interleaved_buffer.clear();
        FL_DBG("SPIBusManager: reset() resetting scalar fields");
        // Reset scalar fields
        mBuses[i].clock_pin = 0xFF;
        mBuses[i].bus_type = SPIBusType::SOFT_SPI;
        mBuses[i].num_devices = 0;
        mBuses[i].spi_bus_num = 0xFF;
        mBuses[i].hw_controller = nullptr;
        mBuses[i].is_initialized = false;
        mBuses[i].error_message = nullptr;
        // Clear device slots
        for (uint8_t j = 0; j < 16; j++) {
            mBuses[i].devices[j] = SPIDeviceInfo();
        }
        FL_DBG("SPIBusManager: reset() done with bus " << static_cast<int>(i));
    }
    FL_DBG("SPIBusManager: reset() complete");
}

uint8_t SPIBusManager::getNumBuses() const {
    return mNumBuses;
}

const SPIBusInfo* SPIBusManager::getBusInfo(uint8_t bus_id) const {
    if (bus_id >= mNumBuses) {
        return nullptr;
    }
    return &mBuses[bus_id];
}

// ============================================================================
// Private Methods
// ============================================================================

SPIBusInfo* SPIBusManager::getOrCreateBus(uint8_t clock_pin) {
    // Search for existing bus with this clock pin
    for (uint8_t i = 0; i < mNumBuses; i++) {
        if (mBuses[i].clock_pin == clock_pin) {
            return &mBuses[i];
        }
    }

    // Create new bus
    if (mNumBuses >= MAX_BUSES) {
        return nullptr;
    }

    mBuses[mNumBuses].clock_pin = clock_pin;
    mBuses[mNumBuses].num_devices = 0;
    mBuses[mNumBuses].bus_type = SPIBusType::SOFT_SPI;
    return &mBuses[mNumBuses++];
}

bool SPIBusManager::initializeBus(SPIBusInfo& bus) {
    // No devices? Skip initialization (bus was released)
    if (bus.num_devices == 0) {
        return true;  // Not an error, just nothing to initialize
    }

    // Check for forced software SPI mode
#if defined(FASTLED_FORCE_SOFTWARE_SPI)
    // User explicitly requested software SPI - skip hardware attempts
    bus.bus_type = SPIBusType::SOFT_SPI;
    bus.is_initialized = true;
    FL_DBG("SPI: Forcing software SPI (FASTLED_FORCE_SOFTWARE_SPI defined)");
    return true;
#endif

    // Single device? Use standard single-line SPI
    if (bus.num_devices == 1) {
        bus.bus_type = SPIBusType::SINGLE_SPI;
        return createSingleSPI(bus);
    }

    // Multiple devices? Try to promote to multi-line SPI
    if (bus.num_devices >= 2 && bus.num_devices <= 16) {
        if (promoteToMultiSPI(bus)) {
            const char* type_name = "Unknown";
            switch (bus.bus_type) {
                case SPIBusType::DUAL_SPI: type_name = "Dual-SPI"; break;
                case SPIBusType::QUAD_SPI: type_name = "Quad-SPI"; break;
                case SPIBusType::OCTO_SPI: type_name = "Octo-SPI"; break;
                case SPIBusType::HEXADECA_SPI: type_name = "Hexadeca-SPI"; break;
                default: break;
            }
            (void)type_name;  // Suppress unused variable warning when FL_WARN is a no-op
            FL_WARN_FMT("SPI Manager: Promoted clock pin " << bus.clock_pin << " to " << type_name << " (" << bus.num_devices << " devices)");
            return true;
        } else {
            // Promotion failed - disable conflicting devices
            FL_WARN_FMT("SPI Manager: Cannot promote clock pin " << bus.clock_pin << " (platform limitation)");
            disableConflictingDevices(bus);
            return false;
        }
    }

    // Too many devices (>16)
    FL_WARN_FMT("SPI Manager: Too many devices on clock pin " << bus.clock_pin << " (" << bus.num_devices << " devices, max 16)");
    disableConflictingDevices(bus);
    return false;
}

bool SPIBusManager::promoteToMultiSPI(SPIBusInfo& bus) {
    SPIBusType max_type = getMaxSupportedSPIType();

    // Determine which multi-SPI type to use
    if (bus.num_devices == 2 && static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::DUAL_SPI)) {
        bus.bus_type = SPIBusType::DUAL_SPI;

        // Get available Dual-SPI controllers and find one we can use (runtime detection)
        const auto& controllers = SpiHw2::getAll();
        if (controllers.empty()) {
            bus.error_message = "No Dual-SPI controllers available on this platform";
            return false;
        }

        // Try each controller until we find one that works
        fl::shared_ptr<SpiHw2> dual_ctrl;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                dual_ctrl = ctrl;
                break;
            }
        }

        if (!dual_ctrl) {
            bus.error_message = "All Dual-SPI controllers already in use";
            return false;
        }

        // Configure Dual-SPI
        SpiHw2::Config config;
        config.bus_num = static_cast<uint8_t>(dual_ctrl->getBusId());
        config.clock_speed_hz = selectBusSpeed(bus);  // Use slowest requested speed
        config.clock_pin = bus.clock_pin;
        config.data0_pin = bus.devices[0].data_pin;
        config.data1_pin = (bus.num_devices > 1) ? bus.devices[1].data_pin : -1;

        // Initialize the controller
        if (!dual_ctrl->begin(config)) {
            bus.error_message = "Failed to initialize Dual-SPI controller";
            return false;
        }

        FL_DBG("SPI: Initialized Dual-SPI controller '" << dual_ctrl->getName()
               << "' (bus " << config.bus_num << ") at " << config.clock_speed_hz << " Hz");

        // Store controller pointer and SPI bus number
        bus.hw_controller = dual_ctrl;
        bus.spi_bus_num = config.bus_num;

        // Initialize lane buffers
        bus.lane_buffers.resize(bus.num_devices);

        bus.is_initialized = true;
        return true;

    } else if (bus.num_devices >= 3 && bus.num_devices <= 4 &&
               static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::QUAD_SPI)) {
        bus.bus_type = SPIBusType::QUAD_SPI;

        // Get available Quad-SPI controllers and find one we can use (runtime detection)
        const auto& controllers = SpiHw4::getAll();
        if (controllers.empty()) {
            bus.error_message = "No Quad-SPI controllers available on this platform";
            return false;
        }

        // Try each controller until we find one that works
        fl::shared_ptr<SpiHw4> quad_ctrl;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                quad_ctrl = ctrl;
                break;
            }
        }

        if (!quad_ctrl) {
            bus.error_message = "All Quad-SPI controllers already in use";
            return false;
        }

        // Configure Quad-SPI
        SpiHw4::Config config;
        config.bus_num = static_cast<uint8_t>(quad_ctrl->getBusId());
        config.clock_speed_hz = selectBusSpeed(bus);  // Use slowest requested speed
        config.clock_pin = bus.clock_pin;
        config.data0_pin = bus.devices[0].data_pin;
        config.data1_pin = (bus.num_devices > 1) ? bus.devices[1].data_pin : -1;
        config.data2_pin = (bus.num_devices > 2) ? bus.devices[2].data_pin : -1;
        config.data3_pin = (bus.num_devices > 3) ? bus.devices[3].data_pin : -1;
        config.max_transfer_sz = 65536;  // 64KB default

        // Initialize the controller
        if (!quad_ctrl->begin(config)) {
            bus.error_message = "Failed to initialize Quad-SPI controller";
            return false;
        }

        FL_DBG("SPI: Initialized Quad-SPI controller '" << quad_ctrl->getName()
               << "' (bus " << config.bus_num << ") at " << config.clock_speed_hz << " Hz");

        // Store controller pointer and SPI bus number
        bus.hw_controller = quad_ctrl;
        bus.spi_bus_num = config.bus_num;

        // Initialize lane buffers
        bus.lane_buffers.resize(bus.num_devices);

        bus.is_initialized = true;
        return true;

    } else if (bus.num_devices >= 5 && bus.num_devices <= 8 &&
               static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::OCTO_SPI)) {
        // Octal-SPI: 8 lanes using SpiHw8 interface
        bus.bus_type = SPIBusType::OCTO_SPI;

        // Get available Octal-SPI controllers (8-lane) - runtime detection
        const auto& controllers = SpiHw8::getAll();
        if (controllers.empty()) {
            bus.error_message = "No Octal-SPI (8-lane) controllers available on this platform";
            return false;
        }

        // Try each controller until we find one that works
        fl::shared_ptr<SpiHw8> octal_ctrl;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                octal_ctrl = ctrl;
                break;
            }
        }

        if (!octal_ctrl) {
            bus.error_message = "All Octal-SPI (8-lane) controllers already in use";
            return false;
        }

        // Configure Octal-SPI (8 data lines)
        SpiHw8::Config config;
        config.bus_num = static_cast<uint8_t>(octal_ctrl->getBusId());
        config.clock_speed_hz = selectBusSpeed(bus);  // Use slowest requested speed
        config.clock_pin = bus.clock_pin;
        config.data0_pin = bus.devices[0].data_pin;
        config.data1_pin = (bus.num_devices > 1) ? bus.devices[1].data_pin : -1;
        config.data2_pin = (bus.num_devices > 2) ? bus.devices[2].data_pin : -1;
        config.data3_pin = (bus.num_devices > 3) ? bus.devices[3].data_pin : -1;
        config.data4_pin = (bus.num_devices > 4) ? bus.devices[4].data_pin : -1;
        config.data5_pin = (bus.num_devices > 5) ? bus.devices[5].data_pin : -1;
        config.data6_pin = (bus.num_devices > 6) ? bus.devices[6].data_pin : -1;
        config.data7_pin = (bus.num_devices > 7) ? bus.devices[7].data_pin : -1;
        config.max_transfer_sz = 65536;  // 64KB default

        // Initialize the controller
        if (!octal_ctrl->begin(config)) {
            bus.error_message = "Failed to initialize Octal-SPI (8-lane) controller";
            return false;
        }

        FL_DBG("SPI: Initialized Octal-SPI controller '" << octal_ctrl->getName()
               << "' (bus " << config.bus_num << ") at " << config.clock_speed_hz << " Hz");

        // Store controller pointer and SPI bus number
        bus.hw_controller = octal_ctrl;
        bus.spi_bus_num = config.bus_num;

        // Initialize lane buffers
        bus.lane_buffers.resize(bus.num_devices);

        bus.is_initialized = true;
        return true;

    } else if (bus.num_devices >= 9 && bus.num_devices <= 16 &&
               static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::HEXADECA_SPI)) {
        // Hexadeca-SPI: 16 lanes using SpiHw16 interface
        bus.bus_type = SPIBusType::HEXADECA_SPI;

        // Get available Hexadeca-SPI controllers (16-lane) - runtime detection
        const auto& controllers = SpiHw16::getAll();
        if (controllers.empty()) {
            bus.error_message = "No Hexadeca-SPI (16-lane) controllers available on this platform";
            return false;
        }

        // Try each controller until we find one that works
        fl::shared_ptr<SpiHw16> hexadeca_ctrl;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                hexadeca_ctrl = ctrl;
                break;
            }
        }

        if (!hexadeca_ctrl) {
            bus.error_message = "All Hexadeca-SPI (16-lane) controllers already in use";
            return false;
        }

        // Configure Hexadeca-SPI (16 data lines)
        SpiHw16::Config config;
        config.bus_num = static_cast<uint8_t>(hexadeca_ctrl->getBusId());
        config.clock_speed_hz = selectBusSpeed(bus);  // Use slowest requested speed
        config.clock_pin = bus.clock_pin;
        config.data0_pin = bus.devices[0].data_pin;
        config.data1_pin = (bus.num_devices > 1) ? bus.devices[1].data_pin : -1;
        config.data2_pin = (bus.num_devices > 2) ? bus.devices[2].data_pin : -1;
        config.data3_pin = (bus.num_devices > 3) ? bus.devices[3].data_pin : -1;
        config.data4_pin = (bus.num_devices > 4) ? bus.devices[4].data_pin : -1;
        config.data5_pin = (bus.num_devices > 5) ? bus.devices[5].data_pin : -1;
        config.data6_pin = (bus.num_devices > 6) ? bus.devices[6].data_pin : -1;
        config.data7_pin = (bus.num_devices > 7) ? bus.devices[7].data_pin : -1;
        config.data8_pin = (bus.num_devices > 8) ? bus.devices[8].data_pin : -1;
        config.data9_pin = (bus.num_devices > 9) ? bus.devices[9].data_pin : -1;
        config.data10_pin = (bus.num_devices > 10) ? bus.devices[10].data_pin : -1;
        config.data11_pin = (bus.num_devices > 11) ? bus.devices[11].data_pin : -1;
        config.data12_pin = (bus.num_devices > 12) ? bus.devices[12].data_pin : -1;
        config.data13_pin = (bus.num_devices > 13) ? bus.devices[13].data_pin : -1;
        config.data14_pin = (bus.num_devices > 14) ? bus.devices[14].data_pin : -1;
        config.data15_pin = (bus.num_devices > 15) ? bus.devices[15].data_pin : -1;
        config.max_transfer_sz = 65536;  // 64KB default

        // Initialize the controller
        if (!hexadeca_ctrl->begin(config)) {
            bus.error_message = "Failed to initialize Hexadeca-SPI (16-lane) controller";
            return false;
        }

        FL_DBG("SPI: Initialized Hexadeca-SPI controller '" << hexadeca_ctrl->getName()
               << "' (bus " << config.bus_num << ") at " << config.clock_speed_hz << " Hz");

        // Store controller pointer and SPI bus number
        bus.hw_controller = hexadeca_ctrl;
        bus.spi_bus_num = config.bus_num;

        // Initialize lane buffers
        bus.lane_buffers.resize(bus.num_devices);

        bus.is_initialized = true;
        return true;
    }

    bus.error_message = "Multi-SPI not supported on this platform";
    return false;
}

bool SPIBusManager::createSingleSPI(SPIBusInfo& bus) {
    // Single SPI is the standard path - just mark as initialized
    // The existing SPI controller code will handle it
    FL_DBG("SPI: Using standard single-lane SPI (bus manager passthrough mode)");
    bus.is_initialized = true;
    return true;
}

void SPIBusManager::disableConflictingDevices(SPIBusInfo& bus) {
    // Keep first device enabled, disable all others
    for (uint8_t i = 1; i < bus.num_devices; i++) {
        bus.devices[i].is_enabled = false;
        FL_WARN_FMT("SPI Manager: Disabled device " << i << " on clock pin " << bus.clock_pin << " (conflict)");
    }

    // Initialize first device as single SPI
    if (bus.num_devices > 0) {
        bus.num_devices = 1;  // Treat as single device
        bus.bus_type = SPIBusType::SINGLE_SPI;
        bus.is_initialized = true;
    }
}

uint32_t SPIBusManager::selectBusSpeed(const SPIBusInfo& bus) {
    // Find the minimum (slowest) requested speed from all devices on this bus
    // This ensures all devices can handle the speed
    uint32_t min_speed = fl::numeric_limits<uint32_t>::max();
    uint32_t devices_with_speed = 0;

    for (uint8_t i = 0; i < bus.num_devices; i++) {
        if (bus.devices[i].is_allocated && bus.devices[i].requested_speed_hz > 0) {
            min_speed = fl::fl_min(min_speed, bus.devices[i].requested_speed_hz);
            devices_with_speed++;
        }
    }

    // If no devices specified a speed (or all specified 0), use platform default
    if (devices_with_speed == 0 || min_speed == fl::numeric_limits<uint32_t>::max()) {
        min_speed = getPlatformDefaultSpeed();
    }

    // Clamp to platform-specific maximum
    uint32_t platform_max = getPlatformMaxSpeed();
    if (min_speed > platform_max) {
        FL_WARN_FMT("SPI: Requested speed " << min_speed << " Hz exceeds platform max "
                << platform_max << " Hz, clamping to " << platform_max);
        min_speed = platform_max;
    }

    // Log selected speed in MHz with one decimal place
    uint32_t mhz_whole = min_speed / 1000000;
    uint32_t mhz_tenth = (min_speed / 100000) % 10;
    FL_LOG_SPI("SPI: Selected bus speed " << mhz_whole << "." << mhz_tenth
               << " MHz for clock pin " << static_cast<int>(bus.clock_pin));

    return min_speed;
}

uint32_t SPIBusManager::getPlatformDefaultSpeed() {
    // Platform-specific defaults based on hardware capabilities
    #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)
        return 40000000;  // ESP32: 40 MHz default (can do up to 80 MHz)
    #elif defined(__SAMD51__)
        return 40000000;  // SAMD51: 40 MHz (can do 60 MHz)
    #elif defined(NRF52) || defined(NRF52832) || defined(NRF52840)
        return 8000000;   // NRF52: 8 MHz maximum
    #elif defined(__SAMD21G18A__) || defined(__SAMD21__)
        return 12000000;  // SAMD21: 12 MHz safe default (max 24 MHz)
    #elif defined(__IMXRT1062__) || defined(TEENSY40) || defined(TEENSY41)
        return 30000000;  // Teensy 4.x: 30 MHz default
    #else
        return 12000000;  // Conservative default for unknown platforms
    #endif
}

uint32_t SPIBusManager::getPlatformMaxSpeed() {
    // Platform-specific maximums based on hardware datasheets
    #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)
        return 80000000;  // ESP32: 80 MHz maximum with IO_MUX pins
    #elif defined(__SAMD51__)
        return 60000000;  // SAMD51: 60 MHz maximum
    #elif defined(NRF52) || defined(NRF52832) || defined(NRF52840)
        return 8000000;   // NRF52: 8 MHz maximum (hardware limitation)
    #elif defined(__SAMD21G18A__) || defined(__SAMD21__)
        return 24000000;  // SAMD21: 24 MHz maximum (F_CPU/2)
    #elif defined(__IMXRT1062__) || defined(TEENSY40) || defined(TEENSY41)
        return 50000000;  // Teensy 4.x: 50 MHz safe maximum
    #else
        return 25000000;  // Conservative maximum for unknown platforms
    #endif
}

void SPIBusManager::releaseBusHardware(SPIBusInfo& bus) {
    FL_DBG("SPIBusManager: releaseBusHardware() called, is_initialized=" << (bus.is_initialized ? "true" : "false"));
    if (!bus.is_initialized) {
        FL_DBG("SPIBusManager: releaseBusHardware() bus not initialized, returning");
        return;  // Nothing to release
    }

    FL_DBG("SPIBusManager: releaseBusHardware() bus_type=" << static_cast<int>(bus.bus_type));
    // Release Single-SPI controller (runtime detection)
    if (bus.bus_type == SPIBusType::SINGLE_SPI && bus.hw_controller) {
        FL_DBG("SPIBusManager: releaseBusHardware() releasing SINGLE_SPI controller");
        SpiHw1* single = static_cast<SpiHw1*>(bus.hw_controller.get());
        FL_DBG("SPIBusManager: releaseBusHardware() calling single->end()");
        single->end();  // Shutdown SPI peripheral
        FL_DBG("SPIBusManager: releaseBusHardware() nulling hw_controller");
        bus.hw_controller = nullptr;
    }

    // Release Dual-SPI controller (runtime detection)
    if (bus.bus_type == SPIBusType::DUAL_SPI && bus.hw_controller) {
        SpiHw2* dual = static_cast<SpiHw2*>(bus.hw_controller.get());
        dual->end();  // Shutdown SPI/SERCOM peripheral
        bus.hw_controller = nullptr;
    }

    // Release Quad-SPI controller
    if (bus.bus_type == SPIBusType::QUAD_SPI && bus.hw_controller) {
        SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller.get());
        quad->end();  // Shutdown SPI peripheral
        bus.hw_controller = nullptr;
    }

    // Release Octal-SPI controller
    if (bus.bus_type == SPIBusType::OCTO_SPI && bus.hw_controller) {
        SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller.get());
        octal->end();  // Shutdown SPI peripheral
        bus.hw_controller = nullptr;
    }

    // Release Hexadeca-SPI controller
    if (bus.bus_type == SPIBusType::HEXADECA_SPI && bus.hw_controller) {
        SpiHw16* hexadeca = static_cast<SpiHw16*>(bus.hw_controller.get());
        hexadeca->end();  // Shutdown SPI/I2S peripheral
        bus.hw_controller = nullptr;
    }

    // Clear lane buffers
    bus.lane_buffers.clear();
    bus.interleaved_buffer.clear();

    // Reset bus state
    bus.is_initialized = false;
    bus.bus_type = SPIBusType::SOFT_SPI;
    bus.num_devices = 0;  // Reset device count to prevent stale state
}

void SPIBusManager::softwareSPIWrite(uint8_t clock_pin, uint8_t data_pin, const uint8_t* data, size_t length) {
#if !defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)
    // Real hardware implementation using Pin class for bit-banging
    // At this point in the header (after class definition), Pin should be available
    // if the user included FastLED.h before using SPIBusManager.

    // Create runtime Pin objects
    Pin clock(clock_pin);
    Pin dataOut(data_pin);

    // Set pins to output
    clock.setOutput();
    dataOut.setOutput();

    // Initial state: clock low
    clock.lo();

    // Bit-bang each byte
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];

        // Send 8 bits, MSB first
        for (uint8_t bit = 0; bit < 8; bit++) {
            // Set data line based on MSB
            if (byte & 0x80) {
                dataOut.hi();
            } else {
                dataOut.lo();
            }

            // Clock high (data is latched on rising edge)
            clock.hi();

            // Clock low (prepare for next bit)
            clock.lo();

            // Shift to next bit
            byte <<= 1;
        }
    }
#else
    // Stub/WASM platforms: no-op (software SPI not available in test/simulation environment)
    FL_UNUSED(clock_pin);
    FL_UNUSED(data_pin);
    FL_UNUSED(data);
    FL_UNUSED(length);
#endif
}

SPIBusType SPIBusManager::getMaxSupportedSPIType() const {
    // Check at runtime using getAll() - platforms provide via weak linkage
    if (!SpiHw16::getAll().empty()) {
        return SPIBusType::HEXADECA_SPI;  // 16-lane SPI
    }
    if (!SpiHw8::getAll().empty()) {
        return SPIBusType::OCTO_SPI;  // 8-lane SPI
    }
    if (!SpiHw4::getAll().empty()) {
        return SPIBusType::QUAD_SPI;  // 4-lane SPI
    }
    if (!SpiHw2::getAll().empty()) {
        return SPIBusType::DUAL_SPI;  // 2-lane SPI
    }
    return SPIBusType::SINGLE_SPI;
}

// ============================================================================
// Global instance
// ============================================================================

SPIBusManager& getSPIBusManager() {
    static SPIBusManager instance;
    return instance;
}

} // namespace fl
