/// @file ilcd_rgb_peripheral.h
/// @brief Virtual interface for LCD RGB peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the LCD RGB driver.
/// It abstracts all ESP-IDF LCD RGB API calls into a clean interface that can be:
/// - Implemented by LcdRgbPeripheralEsp (real hardware delegate on ESP32-P4)
/// - Implemented by LcdRgbPeripheralMock (unit test simulation)
///
/// ## Design Philosophy
///
/// The interface captures the minimal low-level operations against the LCD RGB
/// peripheral hardware. By abstracting at this level, we maximize the amount of
/// driver logic that can be unit tested without real hardware.
///
/// ## Interface Contract
///
/// - All methods return `bool` (true = success, false = error)
/// - Methods mirror ESP-IDF LCD RGB API semantics
/// - No ESP-IDF types leak into interface (opaque handles)
/// - Memory alignment: All DMA buffers MUST be 64-byte aligned
/// - Thread safety: Caller responsible for synchronization

#pragma once

// This interface is platform-agnostic (no ESP32 guard)
// - LcdRgbPeripheralEsp requires ESP32-P4 (real hardware)
// - LcdRgbPeripheralMock works on all platforms (testing)

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/vector.h"

namespace fl {
namespace detail {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief LCD RGB peripheral configuration
///
/// Encapsulates all parameters needed to initialize the LCD RGB hardware.
/// Maps to ESP-IDF's esp_lcd_rgb_panel_config_t structure.
struct LcdRgbPeripheralConfig {
    int pclk_gpio;                       ///< Pixel clock GPIO
    int vsync_gpio;                      ///< VSYNC GPIO (-1 to disable)
    int hsync_gpio;                      ///< HSYNC GPIO (-1 to disable)
    int de_gpio;                         ///< Data enable GPIO (-1 to disable)
    int disp_gpio;                       ///< Display enable GPIO (-1 to disable)
    fl::vector_fixed<int, 16> data_gpios;///< Data lane GPIOs (16 max)
    uint32_t pclk_hz;                    ///< Pixel clock frequency
    size_t num_lanes;                    ///< Active data lanes (1-16)
    size_t h_res;                        ///< Horizontal resolution (pixels per line)
    size_t v_res;                        ///< Vertical resolution (lines per frame)
    size_t vsync_front_porch;            ///< VSYNC front porch (for reset gap)
    bool use_psram;                      ///< Allocate buffers in PSRAM

    /// @brief Default constructor
    LcdRgbPeripheralConfig()
        : pclk_gpio(-1),
          vsync_gpio(-1),
          hsync_gpio(-1),
          de_gpio(-1),
          disp_gpio(-1),
          data_gpios(),
          pclk_hz(0),
          num_lanes(0),
          h_res(0),
          v_res(1),
          vsync_front_porch(0),
          use_psram(true) {}

    /// @brief Constructor with mandatory parameters
    LcdRgbPeripheralConfig(int pclk, uint32_t freq, size_t lanes, size_t hres)
        : pclk_gpio(pclk),
          vsync_gpio(-1),
          hsync_gpio(-1),
          de_gpio(-1),
          disp_gpio(-1),
          data_gpios(),
          pclk_hz(freq),
          num_lanes(lanes),
          h_res(hres),
          v_res(1),
          vsync_front_porch(0),
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

/// @brief Virtual interface for LCD RGB peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF LCD RGB operations.
/// Implementations:
/// - LcdRgbPeripheralEsp: Thin wrapper around ESP-IDF APIs (real hardware)
/// - LcdRgbPeripheralMock: Simulation for host-based unit tests
///
/// ## Usage Pattern
/// ```cpp
/// // Create peripheral (real or mock)
/// ILcdRgbPeripheral* peripheral = new LcdRgbPeripheralMock();
///
/// // Configure
/// LcdRgbPeripheralConfig config = {...};
/// if (!peripheral->initialize(config)) { /* error */ }
///
/// // Register callback
/// peripheral->registerDrawCallback(callback, user_ctx);
///
/// // Allocate buffer and draw
/// uint16_t* buffer = peripheral->allocateFrameBuffer(pixels);
/// // ... encode data into buffer ...
/// peripheral->drawFrame(buffer, pixels);
///
/// // Wait for completion
/// peripheral->waitFrameDone(timeout_ms);
///
/// // Cleanup
/// peripheral->freeFrameBuffer(buffer);
/// delete peripheral;
/// ```
class ILcdRgbPeripheral {
public:
    virtual ~ILcdRgbPeripheral() = default;

    //=========================================================================
    // Lifecycle Methods
    //=========================================================================

    /// @brief Initialize LCD RGB peripheral with configuration
    /// @param config Hardware configuration (pins, clock, resolution)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: esp_lcd_new_rgb_panel() + esp_lcd_panel_init()
    ///
    /// This method:
    /// - Creates the RGB LCD panel
    /// - Configures GPIO pins
    /// - Sets pixel clock frequency
    /// - Allocates hardware resources
    virtual bool initialize(const LcdRgbPeripheralConfig& config) = 0;

    /// @brief Shutdown and release all resources
    ///
    /// Maps to ESP-IDF: esp_lcd_panel_del()
    virtual void deinitialize() = 0;

    /// @brief Check if peripheral is initialized
    /// @return true if initialized, false otherwise
    virtual bool isInitialized() const = 0;

    //=========================================================================
    // Buffer Management
    //=========================================================================

    /// @brief Allocate DMA-capable frame buffer
    /// @param size_bytes Size in bytes (will be rounded up to 64-byte alignment)
    /// @return Pointer to allocated buffer, or nullptr on error
    ///
    /// Maps to ESP-IDF: heap_caps_aligned_alloc(64, size, MALLOC_CAP_DMA)
    ///
    /// The returned buffer:
    /// - Is 64-byte aligned (cache line alignment)
    /// - Is DMA-capable
    /// - Must be freed via freeFrameBuffer()
    virtual uint16_t* allocateFrameBuffer(size_t size_bytes) = 0;

    /// @brief Free frame buffer allocated via allocateFrameBuffer()
    /// @param buffer Buffer pointer (nullptr is safe, no-op)
    ///
    /// Maps to ESP-IDF: heap_caps_free()
    virtual void freeFrameBuffer(uint16_t* buffer) = 0;

    //=========================================================================
    // Transmission Methods
    //=========================================================================

    /// @brief Draw a frame to the LCD panel (DMA transfer)
    /// @param buffer Frame buffer containing pixel data
    /// @param size_bytes Size of data in bytes
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: esp_lcd_panel_draw_bitmap()
    ///
    /// This method queues a DMA transfer of the frame buffer to the LCD panel.
    /// The buffer must remain valid until the transfer completes (callback fires).
    virtual bool drawFrame(const uint16_t* buffer, size_t size_bytes) = 0;

    /// @brief Wait for all pending frame transfers to complete
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if complete, false on timeout
    virtual bool waitFrameDone(uint32_t timeout_ms) = 0;

    /// @brief Check if a transfer is in progress
    /// @return true if busy, false if idle
    virtual bool isBusy() const = 0;

    //=========================================================================
    // Callback Registration
    //=========================================================================

    /// @brief Register callback for frame completion events
    /// @param callback Function pointer (cast to void*)
    /// @param user_ctx User context pointer (passed to callback)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: esp_lcd_rgb_panel_register_event_callbacks()
    ///
    /// Callback signature (cast from void*):
    /// ```cpp
    /// bool callback(void* panel_handle, const void* edata, void* user_ctx);
    /// ```
    ///
    /// The callback:
    /// - Runs in ISR context (MUST be ISR-safe)
    /// - Returns true if high-priority task woken, false otherwise
    virtual bool registerDrawCallback(void* callback, void* user_ctx) = 0;

    //=========================================================================
    // State Inspection
    //=========================================================================

    /// @brief Get current configuration
    /// @return Reference to configuration
    virtual const LcdRgbPeripheralConfig& getConfig() const = 0;

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
