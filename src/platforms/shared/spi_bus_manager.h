#pragma once

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include <stdint.h>
#include <stddef.h>

// Platform-specific includes for Quad-SPI support
#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4) || defined(FASTLED_TESTING)
#include "platforms/shared/spi_quad.h"
#include "platforms/shared/spi_transposer_quad.h"
#endif

namespace fl {

/// SPI bus configuration types
enum class SPIBusType : uint8_t {
    SOFT_SPI,      ///< Software bit-banged SPI (always available)
    SINGLE_SPI,    ///< Hardware SPI, 1 data line (standard SPI)
    DUAL_SPI,      ///< Hardware SPI, 2 data lines (ESP32-C series)
    QUAD_SPI,      ///< Hardware SPI, 4 data lines (ESP32/S/P series)
};

/// Handle returned when registering with the SPI bus manager
struct SPIBusHandle {
    uint8_t bus_id;        ///< Internal bus ID
    uint8_t lane_id;       ///< Lane ID within bus (0 for single SPI, 0-3 for quad)
    bool is_valid;         ///< Whether this handle is valid

    SPIBusHandle() : bus_id(0xFF), lane_id(0xFF), is_valid(false) {}
    SPIBusHandle(uint8_t bid, uint8_t lid) : bus_id(bid), lane_id(lid), is_valid(true) {}
};

/// Information about a registered device on an SPI bus
struct SPIDeviceInfo {
    uint8_t clock_pin;              ///< Clock pin number
    uint8_t data_pin;               ///< Data pin number
    void* controller;               ///< Pointer to LED controller
    uint8_t lane_id;                ///< Assigned lane (0-7)
    bool is_enabled;                ///< Whether this device is active
    bool is_allocated;              ///< Whether this slot is currently in use

    SPIDeviceInfo()
        : clock_pin(0xFF), data_pin(0xFF), controller(nullptr),
          lane_id(0xFF), is_enabled(false), is_allocated(false) {}
};

/// Information about a managed SPI bus
struct SPIBusInfo {
    uint8_t clock_pin;              ///< Clock pin number
    SPIBusType bus_type;            ///< How this bus is being used
    uint8_t num_devices;            ///< Number of devices on this bus
    SPIDeviceInfo devices[8];       ///< Device list (max 8 for ESP32-P4)
    uint8_t spi_bus_num;            ///< Hardware SPI bus number (2 or 3)
    void* hw_controller;            ///< Pointer to hardware controller (Single/Dual/Quad)
    bool is_initialized;            ///< Whether hardware is initialized
    const char* error_message;      ///< Error message if initialization failed

    // Quad-SPI specific buffers
    fl::vector<fl::vector<uint8_t>> lane_buffers;    ///< Per-lane write buffers for Quad-SPI
    fl::vector<uint8_t> interleaved_buffer;          ///< Transposed output for Quad-SPI DMA

    SPIBusInfo()
        : clock_pin(0xFF), bus_type(SPIBusType::SOFT_SPI), num_devices(0),
          spi_bus_num(0xFF), hw_controller(nullptr), is_initialized(false),
          error_message(nullptr) {}
};

/// SPI Bus Manager - Middleware between LED controllers and hardware
///
/// This class manages all SPI bus allocation, detects clock pin conflicts,
/// and automatically promotes to multi-line SPI when possible.
class SPIBusManager {
private:
    static constexpr uint8_t MAX_BUSES = 8;     // Max number of different clock pins
    SPIBusInfo mBuses[MAX_BUSES];
    uint8_t mNumBuses;
    bool mInitialized;

public:
    SPIBusManager() : mNumBuses(0), mInitialized(false) {
        for (uint8_t i = 0; i < MAX_BUSES; i++) {
            mBuses[i] = SPIBusInfo{};
        }
    }

    /// Destructor - clean up all hardware resources
    ~SPIBusManager() {
        reset();  // Release all hardware controllers
    }

    /// Register a device (LED strip) with the manager
    /// Called by LED controllers during construction
    /// @param clock_pin Clock pin number
    /// @param data_pin Data pin number
    /// @param controller Pointer to controller instance
    /// @returns Handle to use for transmit operations
    SPIBusHandle registerDevice(uint8_t clock_pin, uint8_t data_pin, void* controller) {
        if (!controller) {
            FL_WARN("SPIBusManager: NULL controller pointer");
            return SPIBusHandle();
        }

        // Find or create bus for this clock pin
        SPIBusInfo* bus = getOrCreateBus(clock_pin);
        if (!bus) {
            FL_WARN("SPIBusManager: Too many different clock pins (max " << MAX_BUSES << ")");
            return SPIBusHandle();
        }

        // Check if we can add another device to this bus
        if (bus->num_devices >= 8) {
            FL_WARN("SPIBusManager: Too many devices on clock pin " << clock_pin << " (max 8)");
            return SPIBusHandle();
        }

        // Add device to bus
        uint8_t device_idx = bus->num_devices;
        bus->devices[device_idx].clock_pin = clock_pin;
        bus->devices[device_idx].data_pin = data_pin;
        bus->devices[device_idx].controller = controller;
        bus->devices[device_idx].lane_id = device_idx;  // Tentative assignment
        bus->devices[device_idx].is_enabled = true;  // Enabled by default
        bus->devices[device_idx].is_allocated = true;  // Mark as allocated
        bus->num_devices++;

        // Return handle
        uint8_t bus_id = static_cast<uint8_t>(bus - mBuses);
        return SPIBusHandle(bus_id, device_idx);
    }

    /// Unregister a device (LED strip) from the manager
    /// Called by LED controller destructors
    /// @param handle Device handle from registerDevice()
    /// @returns true if successful, false if handle invalid
    bool unregisterDevice(SPIBusHandle handle) {
        if (!handle.is_valid || handle.bus_id >= mNumBuses) {
            return false;
        }

        SPIBusInfo& bus = mBuses[handle.bus_id];
        if (handle.lane_id >= 8) {
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

    /// Initialize all buses and resolve conflicts
    /// Called on first FastLED.show()
    /// @returns true if all buses initialized successfully
    bool initialize() {
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

    /// Transmit data for a specific device
    /// @param handle Device handle from registerDevice()
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    void transmit(SPIBusHandle handle, const uint8_t* data, size_t length) {
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
                // DualSPIController handles interleaving
                // TODO: Implement dual SPI transmission (future)
                break;
            }

            case SPIBusType::QUAD_SPI: {
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
                // Software SPI fallback
                // TODO: Implement software SPI fallback (future)
                break;
            }
        }
    }

    /// Wait for transmission to complete
    /// @param handle Device handle from registerDevice()
    void waitComplete(SPIBusHandle handle) {
        if (!handle.is_valid || handle.bus_id >= mNumBuses) {
            return;
        }

        SPIBusInfo& bus = mBuses[handle.bus_id];
        if (!bus.is_initialized) {
            return;
        }

        // Route to appropriate backend
        switch (bus.bus_type) {
            case SPIBusType::QUAD_SPI: {
                #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4) || defined(FASTLED_TESTING)
                if (bus.hw_controller) {
                    SPIQuad* quad = static_cast<SPIQuad*>(bus.hw_controller);
                    quad->waitComplete();
                }
                #endif
                break;
            }
            default:
                break;
        }
    }

    /// Finalize transmission - flush buffered data for Quad-SPI
    /// This performs the bit-interleaving and DMA transmission
    /// @param handle Device handle from registerDevice()
    void finalizeTransmission(SPIBusHandle handle) {
        if (!handle.is_valid || handle.bus_id >= mNumBuses) {
            return;
        }

        SPIBusInfo& bus = mBuses[handle.bus_id];
        if (!bus.is_initialized || bus.bus_type != SPIBusType::QUAD_SPI) {
            return;  // Only needed for Quad-SPI
        }

        #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4) || defined(FASTLED_TESTING)
        if (!bus.hw_controller) {
            return;
        }

        SPIQuad* quad = static_cast<SPIQuad*>(bus.hw_controller);

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

        // Prepare lane data for transposer
        fl::optional<SPITransposerQuad::LaneData> lanes[4];
        for (uint8_t i = 0; i < bus.num_devices && i < 4; i++) {
            if (bus.devices[i].is_enabled && i < bus.lane_buffers.size()) {
                // TODO: Add proper padding frame support (chipset-specific black LED patterns)
                // For now, use empty padding (will pad with zeros)
                lanes[i] = SPITransposerQuad::LaneData{
                    fl::span<const uint8_t>(bus.lane_buffers[i].data(), bus.lane_buffers[i].size()),
                    fl::span<const uint8_t>()  // No padding frame yet
                };
            }
        }

        // Allocate interleaved buffer (4× max lane size)
        bus.interleaved_buffer.resize(max_size * 4);

        // Transpose lanes into interleaved format
        const char* error = nullptr;
        if (!SPITransposerQuad::transpose(lanes[0], lanes[1], lanes[2], lanes[3],
                                          fl::span<uint8_t>(bus.interleaved_buffer), &error)) {
            FL_WARN("SPI Bus Manager: Transpose failed - " << (error ? error : "unknown error"));
            // Clear buffers and bail
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }

        // Transmit via Quad-SPI hardware
        if (!quad->transmitAsync(fl::span<const uint8_t>(bus.interleaved_buffer))) {
            FL_WARN("SPI Bus Manager: Quad-SPI transmit failed");
        } else {
            // Wait for completion
            quad->waitComplete();
        }

        // Clear lane buffers for next frame
        for (auto& lane_buffer : bus.lane_buffers) {
            lane_buffer.clear();
        }
        #endif
    }

    /// Check if a device is enabled
    /// @param handle Device handle from registerDevice()
    /// @returns true if device is enabled and can transmit
    bool isDeviceEnabled(SPIBusHandle handle) const {
        if (!handle.is_valid || handle.bus_id >= mNumBuses) {
            return false;
        }

        const SPIBusInfo& bus = mBuses[handle.bus_id];
        if (handle.lane_id >= bus.num_devices) {
            return false;
        }

        return bus.devices[handle.lane_id].is_enabled;
    }

    /// Clear all registrations (for testing)
    void reset() {
        for (uint8_t i = 0; i < MAX_BUSES; i++) {
            // Clean up hardware controllers if allocated
            if (mBuses[i].is_initialized) {
                releaseBusHardware(mBuses[i]);
            }
            mBuses[i] = SPIBusInfo{};
        }
        mNumBuses = 0;
        mInitialized = false;
    }

    /// Get the number of buses currently registered
    uint8_t getNumBuses() const { return mNumBuses; }

    /// Get bus info for testing/debugging
    const SPIBusInfo* getBusInfo(uint8_t bus_id) const {
        if (bus_id >= mNumBuses) {
            return nullptr;
        }
        return &mBuses[bus_id];
    }

private:
    /// Find or create bus for a clock pin
    /// @param clock_pin Clock pin number
    /// @returns Pointer to bus, or nullptr if MAX_BUSES exceeded
    SPIBusInfo* getOrCreateBus(uint8_t clock_pin) {
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

    /// Initialize a specific bus (promotes to multi-SPI if needed)
    /// @param bus Bus to initialize
    /// @returns true if successful
    bool initializeBus(SPIBusInfo& bus) {
        // Single device? Use standard single-line SPI
        if (bus.num_devices == 1) {
            bus.bus_type = SPIBusType::SINGLE_SPI;
            return createSingleSPI(bus);
        }

        // Multiple devices? Try to promote to multi-line SPI
        if (bus.num_devices >= 2 && bus.num_devices <= 8) {
            if (promoteToMultiSPI(bus)) {
                const char* type_name = "Unknown";
                switch (bus.bus_type) {
                    case SPIBusType::DUAL_SPI: type_name = "Dual-SPI"; break;
                    case SPIBusType::QUAD_SPI: type_name = "Quad-SPI"; break;
                    default: break;
                }
                FL_WARN("SPI Manager: Promoted clock pin " << bus.clock_pin << " to " << type_name << " (" << bus.num_devices << " devices)");
                return true;
            } else {
                // Promotion failed - disable conflicting devices
                FL_WARN("SPI Manager: Cannot promote clock pin " << bus.clock_pin << " (platform limitation)");
                disableConflictingDevices(bus);
                return false;
            }
        }

        // Too many devices
        FL_WARN("SPI Manager: Too many devices on clock pin " << bus.clock_pin << " (" << bus.num_devices << " devices)");
        disableConflictingDevices(bus);
        return false;
    }

    /// Attempt to promote a bus to multi-line SPI
    /// @param bus Bus to promote
    /// @returns true if promotion succeeded
    bool promoteToMultiSPI(SPIBusInfo& bus) {
        SPIBusType max_type = getMaxSupportedSPIType();

        // Determine which multi-SPI type to use
        if (bus.num_devices == 2 && static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::DUAL_SPI)) {
            bus.bus_type = SPIBusType::DUAL_SPI;
            // TODO: Create DualSPIController
            // bus.hw_controller = new DualSPIController<...>();
            bus.error_message = "Dual-SPI not yet implemented";
            return false;  // Not implemented yet

        } else if (bus.num_devices >= 3 && bus.num_devices <= 4 &&
                   static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::QUAD_SPI)) {
            bus.bus_type = SPIBusType::QUAD_SPI;

            #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4) || defined(FASTLED_TESTING)
            // Get available Quad-SPI controllers and find one we can use
            const auto& controllers = SPIQuad::getAll();
            if (controllers.empty()) {
                bus.error_message = "No Quad-SPI controllers available on this platform";
                return false;
            }

            // Try each controller until we find one that works
            // (ESP32/S2/S3 have 2 buses, C series have 1)
            SPIQuad* quad_ctrl = nullptr;
            for (auto* ctrl : controllers) {
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
            SPIQuad::Config config;
            config.bus_num = static_cast<uint8_t>(quad_ctrl->getBusId());
            config.clock_speed_hz = 20000000;  // 20 MHz default (conservative)
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

            // Store controller pointer and SPI bus number
            bus.hw_controller = quad_ctrl;
            bus.spi_bus_num = config.bus_num;

            // Initialize lane buffers
            bus.lane_buffers.resize(bus.num_devices);

            bus.is_initialized = true;
            return true;
            #else
            bus.error_message = "Quad-SPI not supported on this platform";
            return false;
            #endif

        } else if (bus.num_devices >= 5 && bus.num_devices <= 8 &&
                   max_type == SPIBusType::QUAD_SPI) {
            // ESP32-P4 supports 8 lanes (octal SPI)
            // TODO: Implement OctalSPIController
            bus.error_message = "Octal-SPI not yet implemented";
            return false;
        }

        bus.error_message = "Multi-SPI not supported on this platform";
        return false;
    }

    /// Create single-line SPI controller
    /// @param bus Bus to initialize
    /// @returns true if successful
    bool createSingleSPI(SPIBusInfo& bus) {
        // Single SPI is the standard path - just mark as initialized
        // The existing SPI controller code will handle it
        bus.is_initialized = true;
        return true;
    }

    /// Disable conflicting devices (keep first, disable others)
    /// @param bus Bus with conflicts
    void disableConflictingDevices(SPIBusInfo& bus) {
        // Keep first device enabled, disable all others
        for (uint8_t i = 1; i < bus.num_devices; i++) {
            bus.devices[i].is_enabled = false;
            FL_WARN("SPI Manager: Disabled device " << i << " on clock pin " << bus.clock_pin << " (conflict)");
        }

        // Initialize first device as single SPI
        if (bus.num_devices > 0) {
            bus.num_devices = 1;  // Treat as single device
            bus.bus_type = SPIBusType::SINGLE_SPI;
            bus.is_initialized = true;
        }
    }

    /// Release hardware resources for a bus
    /// Called when all devices on a bus are unregistered
    /// @param bus Bus to clean up
    void releaseBusHardware(SPIBusInfo& bus) {
        if (!bus.is_initialized) {
            return;  // Nothing to release
        }

        #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4) || defined(FASTLED_TESTING)
        // Release Quad-SPI controller
        if (bus.bus_type == SPIBusType::QUAD_SPI && bus.hw_controller) {
            SPIQuad* quad = static_cast<SPIQuad*>(bus.hw_controller);
            quad->end();  // Shutdown SPI peripheral
            bus.hw_controller = nullptr;
        }
        #endif

        // Clear lane buffers
        bus.lane_buffers.clear();
        bus.interleaved_buffer.clear();

        // Reset bus state
        bus.is_initialized = false;
        bus.bus_type = SPIBusType::SOFT_SPI;
    }

    /// Get maximum supported SPI type for this platform
    /// @returns Maximum SPI type supported
    SPIBusType getMaxSupportedSPIType() const {
        #if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32P4) || defined(FASTLED_TESTING)
            // ESP32 main series and test platform support Quad-SPI
            return SPIBusType::QUAD_SPI;
        #elif defined(ESP32C2) || defined(ESP32C3) || defined(ESP32C6) || defined(ESP32H2)
            // ESP32-C series supports Dual-SPI
            return SPIBusType::DUAL_SPI;
        #else
            // Other platforms: single SPI only
            return SPIBusType::SINGLE_SPI;
        #endif
    }
};

// Global instance (defined in implementation file or as inline)
// For header-only implementation, we use inline to avoid multiple definition errors
inline SPIBusManager& getSPIBusManager() {
    static SPIBusManager instance;
    return instance;
}

} // namespace fl
