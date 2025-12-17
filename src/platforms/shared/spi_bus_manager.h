#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

// Shared SPI type definitions
#include "platforms/shared/spi_types.h"

// Multi-lane SPI support - unconditional includes (platform detection via weak linkage)
#include "platforms/shared/spi_hw_1.h"  // Single-SPI
#include "platforms/shared/spi_hw_2.h"  // Dual-SPI
#include "platforms/shared/spi_hw_4.h"  // Quad-SPI
#include "platforms/shared/spi_hw_8.h"  // Octal-SPI
#include "platforms/shared/spi_hw_16.h"  // Hexadeca-SPI
#include "platforms/shared/spi_transposer.h"




// Software SPI support - Pin class forward declared here, defined by platform
// Users must include FastLED.h before using SPIBusManager to ensure Pin class is available

namespace fl {

class Pin;  // Forward declaration - actual class provided by platform's fastpin.h

/// SPI bus configuration types
enum class SPIBusType : uint8_t {
    SOFT_SPI,      ///< Software bit-banged SPI (always available)
    SINGLE_SPI,    ///< Hardware SPI, 1 data line (standard SPI)
    DUAL_SPI,      ///< Hardware SPI, 2 data lines (ESP32-C series)
    QUAD_SPI,      ///< Hardware SPI, 4 data lines (ESP32/S/P series)
    OCTO_SPI,      ///< Hardware SPI, 8 data lines (ESP32-P4)
    HEXADECA_SPI,  ///< Hardware SPI, 16 data lines (ESP32 I2S parallel mode)
};

/// Handle returned when registering with the SPI bus manager
struct SPIBusHandle {
    uint8_t bus_id;        ///< Internal bus ID
    uint8_t lane_id;       ///< Lane ID within bus (0 for single SPI, 0-3 for quad, 0-7 for octo)
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
    SPIDeviceInfo devices[16];      ///< Device list (max 16 for I2S parallel mode)
    uint8_t spi_bus_num;            ///< Hardware SPI bus number (2 or 3)
    fl::shared_ptr<SpiHwBase> hw_controller;  ///< Polymorphic hardware controller (SpiHw1/2/4/8/16)
    bool is_initialized;            ///< Whether hardware is initialized
    const char* error_message;      ///< Error message if initialization failed

    // Multi-lane SPI specific buffers
    fl::vector<fl::vector<uint8_t>> lane_buffers;    ///< Per-lane write buffers for multi-lane SPI
    fl::vector<uint8_t> interleaved_buffer;          ///< Transposed output for multi-lane DMA

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
    SPIBusManager();
    ~SPIBusManager();

    /// Register a device (LED strip) with the manager
    /// Called by LED controllers during construction
    /// @param clock_pin Clock pin number
    /// @param data_pin Data pin number
    /// @param requested_speed_hz User-requested SPI speed in Hz (from DATA_RATE_MHZ)
    /// @param controller Pointer to controller instance
    /// @returns Handle to use for transmit operations
    SPIBusHandle registerDevice(uint8_t clock_pin, uint8_t data_pin, uint32_t requested_speed_hz, void* controller);

    /// Unregister a device (LED strip) from the manager
    /// Called by LED controller destructors
    /// @param handle Device handle from registerDevice()
    /// @returns true if successful, false if handle invalid
    bool unregisterDevice(SPIBusHandle handle);

    /// Initialize all buses and resolve conflicts
    /// Called on first FastLED.show()
    /// @returns true if all buses initialized successfully
    bool initialize();

    /// Transmit data for a specific device
    /// @param handle Device handle from registerDevice()
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    void transmit(SPIBusHandle handle, const uint8_t* data, size_t length);

    /// Wait for transmission to complete
    /// @param handle Device handle from registerDevice()
    void waitComplete(SPIBusHandle handle);

    /// Finalize transmission - flush buffered data for Dual-SPI, Quad-SPI and Octo-SPI
    /// This performs the bit-interleaving and DMA transmission
    /// @param handle Device handle from registerDevice()
    void finalizeTransmission(SPIBusHandle handle);

    /// Check if a device is enabled
    /// @param handle Device handle from registerDevice()
    /// @returns true if device is enabled and can transmit
    bool isDeviceEnabled(SPIBusHandle handle) const;

    /// Clear all registrations (for testing)
    void reset();

    /// Get the number of buses currently registered
    uint8_t getNumBuses() const;

    /// Get bus info for testing/debugging
    const SPIBusInfo* getBusInfo(uint8_t bus_id) const;

private:
    /// Find or create bus for a clock pin
    /// @param clock_pin Clock pin number
    /// @returns Pointer to bus, or nullptr if MAX_BUSES exceeded
    SPIBusInfo* getOrCreateBus(uint8_t clock_pin);

    /// Initialize a specific bus (promotes to multi-SPI if needed)
    /// @param bus Bus to initialize
    /// @returns true if successful
    bool initializeBus(SPIBusInfo& bus);

    /// Attempt to promote a bus to multi-line SPI
    /// @param bus Bus to promote
    /// @returns true if promotion succeeded
    bool promoteToMultiSPI(SPIBusInfo& bus);

    /// Create single-line SPI controller
    /// @param bus Bus to initialize
    /// @returns true if successful
    bool createSingleSPI(SPIBusInfo& bus);

    /// Disable conflicting devices (keep first, disable others)
    /// @param bus Bus with conflicts
    void disableConflictingDevices(SPIBusInfo& bus);

    /// Select appropriate SPI clock speed for a bus
    /// Takes the minimum (slowest) requested speed to ensure all devices work
    /// @param bus Bus to select speed for
    /// @returns Clock speed in Hz
    uint32_t selectBusSpeed(const SPIBusInfo& bus);

    /// Get platform-specific default SPI speed
    /// @returns Default speed in Hz
    uint32_t getPlatformDefaultSpeed();

    /// Get platform-specific maximum SPI speed
    /// @returns Maximum safe speed in Hz
    uint32_t getPlatformMaxSpeed();

    /// Release hardware resources for a bus
    /// Called when all devices on a bus are unregistered
    /// @param bus Bus to clean up
    void releaseBusHardware(SPIBusInfo& bus);

    /// Software SPI bit-banging implementation using runtime pins
    /// @param clock_pin Clock pin number
    /// @param data_pin Data pin number
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    ///
    /// @note This function is only available when Pin class is fully defined.
    ///       In normal usage (when FastLED.h is included), Pin will be available.
    ///       In test/stub builds or when Pin is not available, this is a no-op.
    void softwareSPIWrite(uint8_t clock_pin, uint8_t data_pin, const uint8_t* data, size_t length);

    /// Get maximum supported SPI type for this platform
    /// Uses runtime detection via getAll() - platforms provide via weak linkage
    /// @returns Maximum SPI type supported
    SPIBusType getMaxSupportedSPIType() const;
};

// Global instance
SPIBusManager& getSPIBusManager();

} // namespace fl
