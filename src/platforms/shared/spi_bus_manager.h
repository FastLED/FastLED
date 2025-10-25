#pragma once

#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/log.h"
#include "fl/stdint.h"

// Shared SPI type definitions
#include "platforms/shared/spi_types.h"

// Multi-lane SPI support - unconditional includes (platform detection via weak linkage)
#include "platforms/shared/spi_hw_2.h"  // Dual-SPI
#include "platforms/shared/spi_hw_4.h"  // Quad-SPI
#include "platforms/shared/spi_hw_8.h"  // Octal-SPI
#include "platforms/shared/spi_transposer.h"

// Software SPI support - Pin class forward declared here, defined by platform
// Users must include FastLED.h before using SPIBusManager to ensure Pin class is available
namespace fl {
    class Pin;  // Forward declaration - actual class provided by platform's fastpin.h
}

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
    uint32_t requested_speed_hz;    ///< User-requested SPI speed (from DATA_RATE_MHZ)
    bool is_enabled;                ///< Whether this device is active
    bool is_allocated;              ///< Whether this slot is currently in use

    SPIDeviceInfo()
        : clock_pin(0xFF), data_pin(0xFF), controller(nullptr),
          lane_id(0xFF), requested_speed_hz(0), is_enabled(false), is_allocated(false) {}
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
    /// @param requested_speed_hz User-requested SPI speed in Hz (from DATA_RATE_MHZ)
    /// @param controller Pointer to controller instance
    /// @returns Handle to use for transmit operations
    SPIBusHandle registerDevice(uint8_t clock_pin, uint8_t data_pin, uint32_t requested_speed_hz, void* controller) {
        if (!controller) {
            FL_WARN("SPIBusManager: nullptr controller pointer");
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
        bus->devices[device_idx].requested_speed_hz = requested_speed_hz;
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
            case SPIBusType::DUAL_SPI: {
                if (bus.hw_controller) {
                    // 2-lane (Dual-SPI)
                    SpiHw2* dual = static_cast<SpiHw2*>(bus.hw_controller);
                    dual->waitComplete();
                }
                break;
            }
            case SPIBusType::QUAD_SPI: {
                if (bus.hw_controller) {
                    // Determine if this is 4-lane or 8-lane based on device count
                    if (bus.num_devices > 4) {
                        // 8-lane (Octal-SPI)
                        SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller);
                        octal->waitComplete();
                    } else {
                        // 4-lane (Quad-SPI)
                        SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller);
                        quad->waitComplete();
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    /// Finalize transmission - flush buffered data for Dual-SPI and Quad-SPI
    /// This performs the bit-interleaving and DMA transmission
    /// @param handle Device handle from registerDevice()
    void finalizeTransmission(SPIBusHandle handle) {
        if (!handle.is_valid || handle.bus_id >= mNumBuses) {
            return;
        }

        SPIBusInfo& bus = mBuses[handle.bus_id];
        if (!bus.is_initialized) {
            return;  // Bus not initialized
        }

        // Only needed for multi-SPI modes (Dual-SPI, Quad-SPI, Octal-SPI)
        if (bus.bus_type != SPIBusType::DUAL_SPI && bus.bus_type != SPIBusType::QUAD_SPI) {
            return;
        }

        // Handle Dual-SPI (runtime detection via getAll())
        if (bus.bus_type == SPIBusType::DUAL_SPI) {
            if (!bus.hw_controller) {
                return;
            }

            SpiHw2* dual = static_cast<SpiHw2*>(bus.hw_controller);

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

            // Acquire DMA buffer (zero-copy API)
            DMABuffer result = dual->acquireDMABuffer(max_size);
            if (!result.ok()) {
                FL_WARN("SPI Bus Manager: Failed to acquire DMA buffer for Dual-SPI: " << static_cast<int>(result.error()));
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
                FL_WARN("SPI Bus Manager: Dual transpose failed - " << (error ? error : "unknown error"));
                // Clear buffers and bail
                for (auto& lane_buffer : bus.lane_buffers) {
                    lane_buffer.clear();
                }
                return;
            }

            // Transmit via Dual-SPI hardware
            bool transmit_ok = dual->transmit(TransmitMode::ASYNC);
            if (transmit_ok) {
                dual->waitComplete();
            } else {
                FL_WARN("SPI Bus Manager: Dual-SPI transmit failed");
            }

            // Clear lane buffers for next frame
            for (auto& lane_buffer : bus.lane_buffers) {
                lane_buffer.clear();
            }
            return;
        }

        // Handle Quad-SPI and Octal-SPI (runtime detection)
        if (!bus.hw_controller) {
            return;
        }

        // Determine if this is 4-lane or 8-lane based on device count
        bool is_octal_mode = (bus.num_devices > 4);

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

        // Determine if we're using 8-lane (octal) or 4-lane (quad) mode
        bool is_octal = (bus.num_devices > 4 && bus.num_devices <= 8);

        // Acquire DMA buffer (zero-copy API)
        DMABuffer result;
        fl::span<uint8_t> dma_buf;
        if (is_octal) {
            SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller);
            result = octal->acquireDMABuffer(max_size);
            if (!result.ok()) {
                FL_WARN("SPI Bus Manager: Failed to acquire DMA buffer for Octal-SPI: " << static_cast<int>(result.error()));
                // Clear buffers and bail
                for (auto& lane_buffer : bus.lane_buffers) {
                    lane_buffer.clear();
                }
                return;
            }
            dma_buf = result.data();
        } else {
            SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller);
            result = quad->acquireDMABuffer(max_size);
            if (!result.ok()) {
                FL_WARN("SPI Bus Manager: Failed to acquire DMA buffer for Quad-SPI: " << static_cast<int>(result.error()));
                // Clear buffers and bail
                for (auto& lane_buffer : bus.lane_buffers) {
                    lane_buffer.clear();
                }
                return;
            }
            dma_buf = result.data();
        }

        const char* error = nullptr;
        if (is_octal) {
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
                FL_WARN("SPI Bus Manager: Octal transpose failed - " << (error ? error : "unknown error"));
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
                FL_WARN("SPI Bus Manager: Quad transpose failed - " << (error ? error : "unknown error"));
                // Clear buffers and bail
                for (auto& lane_buffer : bus.lane_buffers) {
                    lane_buffer.clear();
                }
                return;
            }
        }

        // Transmit via Quad-SPI or Octal-SPI hardware
        bool transmit_ok = false;
        if (is_octal_mode) {
            // 8-lane (Octal-SPI)
            SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller);
            transmit_ok = octal->transmit(TransmitMode::ASYNC);
            if (transmit_ok) {
                octal->waitComplete();
            }
        } else {
            // 4-lane (Quad-SPI)
            SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller);
            transmit_ok = quad->transmit(TransmitMode::ASYNC);
            if (transmit_ok) {
                quad->waitComplete();
            }
        }

        if (!transmit_ok) {
            FL_WARN("SPI Bus Manager: " << (is_octal_mode ? "Octal" : "Quad") << "-SPI transmit failed");
        }

        // Clear lane buffers for next frame
        for (auto& lane_buffer : bus.lane_buffers) {
            lane_buffer.clear();
        }
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
                const char* type_name;
                switch (bus.bus_type) {
                    case SPIBusType::DUAL_SPI: type_name = "Dual-SPI"; break;
                    case SPIBusType::QUAD_SPI: type_name = "Quad-SPI"; break;
                    default: type_name = "Unknown"; break;
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

            // Get available Dual-SPI controllers and find one we can use (runtime detection)
            const auto& controllers = SpiHw2::getAll();
            if (controllers.empty()) {
                bus.error_message = "No Dual-SPI controllers available on this platform";
                return false;
            }

            // Try each controller until we find one that works
            SpiHw2* dual_ctrl = nullptr;
            for (auto* ctrl : controllers) {
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
            SpiHw4* quad_ctrl = nullptr;
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
                   static_cast<uint8_t>(max_type) >= static_cast<uint8_t>(SPIBusType::QUAD_SPI)) {
            // Octal-SPI: 8 lanes using SpiHw8 interface
            bus.bus_type = SPIBusType::QUAD_SPI;  // Reuses enum value, but with 8-lane controller

            // Get available Octal-SPI controllers (8-lane) - runtime detection
            const auto& controllers = SpiHw8::getAll();
            if (controllers.empty()) {
                bus.error_message = "No Octal-SPI (8-lane) controllers available on this platform";
                return false;
            }

            // Try each controller until we find one that works
            SpiHw8* octal_ctrl = nullptr;
            for (auto* ctrl : controllers) {
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
        FL_DBG("SPI: Using standard single-lane SPI (bus manager passthrough mode)");
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

    /// Select appropriate SPI clock speed for a bus
    /// Takes the minimum (slowest) requested speed to ensure all devices work
    /// @param bus Bus to select speed for
    /// @returns Clock speed in Hz
    uint32_t selectBusSpeed(const SPIBusInfo& bus) {
        // Find the minimum (slowest) requested speed from all devices on this bus
        // This ensures all devices can handle the speed
        uint32_t min_speed = UINT32_MAX;
        uint32_t devices_with_speed = 0;

        for (uint8_t i = 0; i < bus.num_devices; i++) {
            if (bus.devices[i].is_allocated && bus.devices[i].requested_speed_hz > 0) {
                min_speed = fl::fl_min(min_speed, bus.devices[i].requested_speed_hz);
                devices_with_speed++;
            }
        }

        // If no devices specified a speed (or all specified 0), use platform default
        if (devices_with_speed == 0 || min_speed == UINT32_MAX) {
            min_speed = getPlatformDefaultSpeed();
        }

        // Clamp to platform-specific maximum
        uint32_t platform_max = getPlatformMaxSpeed();
        if (min_speed > platform_max) {
            FL_WARN("SPI: Requested speed " << min_speed << " Hz exceeds platform max "
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

    /// Get platform-specific default SPI speed
    /// @returns Default speed in Hz
    uint32_t getPlatformDefaultSpeed() {
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

    /// Get platform-specific maximum SPI speed
    /// @returns Maximum safe speed in Hz
    uint32_t getPlatformMaxSpeed() {
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

    /// Release hardware resources for a bus
    /// Called when all devices on a bus are unregistered
    /// @param bus Bus to clean up
    void releaseBusHardware(SPIBusInfo& bus) {
        if (!bus.is_initialized) {
            return;  // Nothing to release
        }

        // Release Dual-SPI controller (runtime detection)
        if (bus.bus_type == SPIBusType::DUAL_SPI && bus.hw_controller) {
            SpiHw2* dual = static_cast<SpiHw2*>(bus.hw_controller);
            dual->end();  // Shutdown SPI/SERCOM peripheral
            bus.hw_controller = nullptr;
        }

        // Release Quad-SPI or Octal-SPI controller (runtime detection)
        if (bus.bus_type == SPIBusType::QUAD_SPI && bus.hw_controller) {
            // Determine if this is 4-lane or 8-lane based on device count
            if (bus.num_devices > 4) {
                // 8-lane (Octal-SPI)
                SpiHw8* octal = static_cast<SpiHw8*>(bus.hw_controller);
                octal->end();  // Shutdown SPI peripheral
            } else {
                // 4-lane (Quad-SPI)
                SpiHw4* quad = static_cast<SpiHw4*>(bus.hw_controller);
                quad->end();  // Shutdown SPI peripheral
            }
            bus.hw_controller = nullptr;
        }

        // Clear lane buffers
        bus.lane_buffers.clear();
        bus.interleaved_buffer.clear();

        // Reset bus state
        bus.is_initialized = false;
        bus.bus_type = SPIBusType::SOFT_SPI;
    }

    /// Software SPI bit-banging implementation using runtime pins
    /// @param clock_pin Clock pin number
    /// @param data_pin Data pin number
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    ///
    /// @note This function is only available when Pin class is fully defined.
    ///       In normal usage (when FastLED.h is included), Pin will be available.
    ///       In test/stub builds or when Pin is not available, this is a no-op.
    void softwareSPIWrite(uint8_t clock_pin, uint8_t data_pin, const uint8_t* data, size_t length);  // Declaration only

    /// Get maximum supported SPI type for this platform
    /// Uses runtime detection via getAll() - platforms provide via weak linkage
    /// @returns Maximum SPI type supported
    SPIBusType getMaxSupportedSPIType() const {
        // Check at runtime using getAll() - platforms provide via weak linkage
        if (!SpiHw8::getAll().empty()) {
            return SPIBusType::QUAD_SPI;  // Octal uses QUAD_SPI enum
        }
        if (!SpiHw4::getAll().empty()) {
            return SPIBusType::QUAD_SPI;
        }
        if (!SpiHw2::getAll().empty()) {
            return SPIBusType::DUAL_SPI;
        }
        return SPIBusType::SINGLE_SPI;
    }
};

// Global instance (defined in implementation file or as inline)
// For header-only implementation, we use inline to avoid multiple definition errors
inline SPIBusManager& getSPIBusManager() {
    static SPIBusManager instance;
    return instance;
}

// ============================================================================
// Software SPI Implementation (out-of-line to avoid Pin class dependency issues)
// ============================================================================

/// Software SPI bit-banging implementation - defined out-of-line to handle Pin class dependencies
/// This implementation is only active when Pin class is fully defined (i.e., when FastLED.h is included)
inline void SPIBusManager::softwareSPIWrite(uint8_t clock_pin, uint8_t data_pin, const uint8_t* data, size_t length) {
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

} // namespace fl
