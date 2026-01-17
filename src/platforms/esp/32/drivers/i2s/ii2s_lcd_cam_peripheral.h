/// @file ii2s_lcd_cam_peripheral.h
/// @brief Virtual interface for I2S LCD_CAM peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the I2S LCD_CAM driver.
/// It abstracts all ESP-IDF LCD I80 bus API calls into a clean interface that can be:
/// - Implemented by I2sLcdCamPeripheralEsp (real hardware delegate on ESP32-S3)
/// - Implemented by I2sLcdCamPeripheralMock (unit test simulation)
///
/// ## Design Philosophy
///
/// The interface captures the minimal low-level operations against the I2S LCD_CAM
/// peripheral hardware. By abstracting at this level, we maximize the amount of
/// driver logic that can be unit tested without real hardware.
///
/// ## Hardware Background
///
/// ESP32-S3 uses the LCD_CAM peripheral (via esp_lcd_i80_bus API) for parallel LED
/// driving. This is the same peripheral used by Yves Bazin's I2SClocklessLedDriver.
/// The peripheral provides:
/// - 16-bit parallel data output
/// - DMA-driven transmission
/// - Configurable pixel clock (PCLK)
/// - ISR callback on transmission complete
///
/// ## Interface Contract
///
/// - All methods return `bool` (true = success, false = error)
/// - Methods mirror ESP-IDF LCD I80 API semantics
/// - No ESP-IDF types leak into interface (opaque handles)
/// - Memory alignment: All DMA buffers MUST be 64-byte aligned (PSRAM requirement)
/// - Thread safety: Caller responsible for synchronization

#pragma once

// This interface is platform-agnostic (no ESP32 guard)
// - I2sLcdCamPeripheralEsp requires ESP32-S3 (real hardware)
// - I2sLcdCamPeripheralMock works on all platforms (testing)

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/vector.h"

namespace fl {
namespace detail {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief I2S LCD_CAM peripheral configuration
///
/// Encapsulates all parameters needed to initialize the I2S LCD_CAM hardware.
/// Maps to ESP-IDF's esp_lcd_i80_bus_config_t and esp_lcd_panel_io_i80_config_t.
struct I2sLcdCamConfig {
    fl::vector_fixed<int, 16> data_gpios;  ///< Data lane GPIOs (D0-D15)
    int num_lanes;                          ///< Active data lanes (1-16)
    uint32_t pclk_hz;                       ///< Pixel clock frequency
    size_t max_transfer_bytes;              ///< Maximum bytes per transfer
    bool use_psram;                         ///< Allocate buffers in PSRAM

    /// @brief Default constructor
    I2sLcdCamConfig()
        : data_gpios(),
          num_lanes(0),
          pclk_hz(0),
          max_transfer_bytes(0),
          use_psram(true) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;  // Unused by default
        }
    }

    /// @brief Constructor with mandatory parameters
    I2sLcdCamConfig(int lanes, uint32_t freq, size_t max_bytes)
        : data_gpios(),
          num_lanes(lanes),
          pclk_hz(freq),
          max_transfer_bytes(max_bytes),
          use_psram(true) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;  // Unused by default
        }
    }
};

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for I2S LCD_CAM peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF LCD I80 operations.
/// Implementations:
/// - I2sLcdCamPeripheralEsp: Wrapper around ESP-IDF APIs (real hardware)
/// - I2sLcdCamPeripheralMock: Simulation for host-based unit tests
///
/// ## Usage Pattern
/// ```cpp
/// // Create peripheral (real or mock)
/// II2sLcdCamPeripheral* peripheral = new I2sLcdCamPeripheralMock();
///
/// // Configure
/// I2sLcdCamConfig config = {...};
/// if (!peripheral->initialize(config)) { /* error */ }
///
/// // Register callback
/// peripheral->registerTransmitCallback(callback, user_ctx);
///
/// // Allocate buffer and transmit
/// uint16_t* buffer = peripheral->allocateBuffer(size);
/// // ... encode data into buffer ...
/// peripheral->transmit(buffer, size);
///
/// // Wait for completion
/// peripheral->waitTransmitDone(timeout_ms);
///
/// // Cleanup
/// peripheral->freeBuffer(buffer);
/// delete peripheral;
/// ```
class II2sLcdCamPeripheral {
public:
    virtual ~II2sLcdCamPeripheral() = default;

    //=========================================================================
    // Lifecycle Methods
    //=========================================================================

    /// @brief Initialize I2S LCD_CAM peripheral with configuration
    /// @param config Hardware configuration (pins, clock, max transfer size)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: esp_lcd_new_i80_bus() + esp_lcd_new_panel_io_i80()
    ///
    /// This method:
    /// - Creates the I80 bus with configured pins
    /// - Creates panel IO with PCLK frequency
    /// - Registers internal callback
    /// - Allocates hardware resources
    virtual bool initialize(const I2sLcdCamConfig& config) = 0;

    /// @brief Shutdown and release all resources
    ///
    /// Maps to ESP-IDF: esp_lcd_panel_io_del() + esp_lcd_del_i80_bus()
    virtual void deinitialize() = 0;

    /// @brief Check if peripheral is initialized
    /// @return true if initialized, false otherwise
    virtual bool isInitialized() const = 0;

    //=========================================================================
    // Buffer Management
    //=========================================================================

    /// @brief Allocate DMA-capable buffer
    /// @param size_bytes Size in bytes (will be rounded up to 64-byte alignment)
    /// @return Pointer to allocated buffer, or nullptr on error
    ///
    /// Maps to ESP-IDF: heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    ///
    /// The returned buffer:
    /// - Is 64-byte aligned (PSRAM alignment requirement)
    /// - Is DMA-capable and in PSRAM
    /// - Must be freed via freeBuffer()
    virtual uint16_t* allocateBuffer(size_t size_bytes) = 0;

    /// @brief Free buffer allocated via allocateBuffer()
    /// @param buffer Buffer pointer (nullptr is safe, no-op)
    ///
    /// Maps to ESP-IDF: heap_caps_free()
    virtual void freeBuffer(uint16_t* buffer) = 0;

    //=========================================================================
    // Transmission Methods
    //=========================================================================

    /// @brief Transmit data via I2S LCD_CAM DMA
    /// @param buffer Buffer containing encoded pixel data
    /// @param size_bytes Size of data in bytes
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: led_io_handle->tx_color()
    ///
    /// This method queues a DMA transfer of the buffer.
    /// The buffer must remain valid until the transfer completes (callback fires).
    virtual bool transmit(const uint16_t* buffer, size_t size_bytes) = 0;

    /// @brief Wait for all pending transmissions to complete
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if complete, false on timeout
    virtual bool waitTransmitDone(uint32_t timeout_ms) = 0;

    /// @brief Check if a transmission is in progress
    /// @return true if busy, false if idle
    virtual bool isBusy() const = 0;

    //=========================================================================
    // Callback Registration
    //=========================================================================

    /// @brief Register callback for transmission completion events
    /// @param callback Function pointer (cast to void*)
    /// @param user_ctx User context pointer (passed to callback)
    /// @return true on success, false on error
    ///
    /// Callback signature (cast from void*):
    /// ```cpp
    /// bool callback(void* panel_io, const void* edata, void* user_ctx);
    /// ```
    ///
    /// The callback:
    /// - Runs in ISR context (MUST be ISR-safe)
    /// - Returns true if high-priority task woken, false otherwise
    virtual bool registerTransmitCallback(void* callback, void* user_ctx) = 0;

    //=========================================================================
    // State Inspection
    //=========================================================================

    /// @brief Get current configuration
    /// @return Reference to configuration
    virtual const I2sLcdCamConfig& getConfig() const = 0;

    //=========================================================================
    // Platform Utilities
    //=========================================================================

    /// @brief Get current timestamp in microseconds
    /// @return Current timestamp (monotonic clock)
    virtual uint64_t getMicroseconds() = 0;

    /// @brief Portable delay
    /// @param ms Delay duration in milliseconds
    virtual void delay(uint32_t ms) = 0;
};

} // namespace detail
} // namespace fl
