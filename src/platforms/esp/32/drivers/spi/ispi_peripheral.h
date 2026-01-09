/// @file ispi_peripheral.h
/// @brief Virtual interface for SPI peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the SPI channel engine.
/// It abstracts all ESP-IDF SPI Master API calls into a clean interface that can be:
/// - Implemented by SpiPeripheralESP (real hardware delegate)
/// - Implemented by SpiPeripheralMock (unit test simulation)

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {
namespace detail {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief SPI bus configuration
///
/// Encapsulates all parameters needed to initialize the SPI bus.
/// Maps directly to ESP-IDF's spi_bus_config_t structure.
struct SpiBusConfig {
    int miso_pin;          ///< MISO pin (GPIO number, -1 if unused)
    int mosi_pin;          ///< MOSI pin (GPIO number, -1 for quad mode)
    int sclk_pin;          ///< SCLK pin (GPIO number)
    int data2_pin;         ///< Data2/WP pin for quad mode (-1 if unused)
    int data3_pin;         ///< Data3/HD pin for quad mode (-1 if unused)
    int max_transfer_sz;   ///< Max transfer size in bytes (0 = default 4094)
    uint32_t flags;        ///< Bus flags (e.g., SPICOMMON_BUSFLAG_MASTER)

    /// @brief Default constructor (single-lane mode)
    SpiBusConfig()
        : miso_pin(-1)
        , mosi_pin(-1)
        , sclk_pin(-1)
        , data2_pin(-1)
        , data3_pin(-1)
        , max_transfer_sz(0)
        , flags(0)
    {}

    /// @brief Constructor for single-lane mode
    /// @param mosi MOSI pin (data output)
    /// @param sclk SCLK pin (clock output)
    /// @param max_sz Max transfer size (0 = default)
    SpiBusConfig(int mosi, int sclk, int max_sz = 0)
        : miso_pin(-1)
        , mosi_pin(mosi)
        , sclk_pin(sclk)
        , data2_pin(-1)
        , data3_pin(-1)
        , max_transfer_sz(max_sz)
        , flags(0)
    {}

    /// @brief Constructor for dual-lane mode
    /// @param data0 Data0/MOSI pin
    /// @param data1 Data1/MISO pin
    /// @param sclk SCLK pin
    /// @param max_sz Max transfer size
    SpiBusConfig(int data0, int data1, int sclk, int max_sz)
        : miso_pin(data1)  // Data1 uses MISO line
        , mosi_pin(data0)  // Data0 uses MOSI line
        , sclk_pin(sclk)
        , data2_pin(-1)
        , data3_pin(-1)
        , max_transfer_sz(max_sz)
        , flags(0)
    {}

    /// @brief Constructor for quad-lane mode
    /// @param data0 Data0/MOSI pin
    /// @param data1 Data1/MISO pin
    /// @param data2 Data2/WP pin
    /// @param data3 Data3/HD pin
    /// @param sclk SCLK pin
    /// @param max_sz Max transfer size
    SpiBusConfig(int data0, int data1, int data2, int data3, int sclk, int max_sz)
        : miso_pin(data1)
        , mosi_pin(data0)
        , sclk_pin(sclk)
        , data2_pin(data2)
        , data3_pin(data3)
        , max_transfer_sz(max_sz)
        , flags(0)
    {}
};

/// @brief SPI device configuration
///
/// Encapsulates device-specific parameters (clock speed, mode, queue depth).
/// Maps directly to ESP-IDF's spi_device_interface_config_t structure.
struct SpiDeviceConfig {
    uint8_t mode;          ///< SPI mode (0-3), typically 0 for WS2812
    int clock_speed_hz;    ///< Clock frequency in Hz (e.g., 2500000 for WS2812)
    int queue_size;        ///< Transaction queue depth (typically 2-4)
    uint32_t flags;        ///< Device flags (e.g., SPI_DEVICE_NO_DUMMY)
    int spics_io_num;      ///< Chip select pin (-1 if unused)

    /// @brief Default constructor
    SpiDeviceConfig()
        : mode(0)
        , clock_speed_hz(0)
        , queue_size(0)
        , flags(0)
        , spics_io_num(-1)
    {}

    /// @brief Constructor with common parameters
    /// @param clock_hz Clock frequency in Hz
    /// @param queue_depth Transaction queue size
    /// @param spi_mode SPI mode (0-3)
    SpiDeviceConfig(int clock_hz, int queue_depth, uint8_t spi_mode = 0)
        : mode(spi_mode)
        , clock_speed_hz(clock_hz)
        , queue_size(queue_depth)
        , flags(0)
        , spics_io_num(-1)
    {}
};

/// @brief SPI transaction descriptor
///
/// Encapsulates a single DMA transaction (buffer + metadata).
/// Maps to ESP-IDF's spi_transaction_t structure (simplified).
struct SpiTransaction {
    const uint8_t* tx_buffer;  ///< Transmit buffer (DMA-capable)
    size_t length_bits;        ///< Transaction length in bits (buffer size × 8)
    uint32_t flags;            ///< Transaction flags (e.g., SPI_TRANS_USE_TXDATA)
    void* user;                ///< User context pointer (optional)

    /// @brief Default constructor
    SpiTransaction()
        : tx_buffer(nullptr)
        , length_bits(0)
        , flags(0)
        , user(nullptr)
    {}

    /// @brief Constructor with buffer and size
    /// @param buffer Transmit buffer pointer
    /// @param size_bytes Buffer size in bytes
    SpiTransaction(const uint8_t* buffer, size_t size_bytes)
        : tx_buffer(buffer)
        , length_bits(size_bytes * 8)
        , flags(0)
        , user(nullptr)
    {}

    /// @brief Constructor with buffer, size, and user context
    /// @param buffer Transmit buffer pointer
    /// @param size_bytes Buffer size in bytes
    /// @param user_ctx User context pointer
    SpiTransaction(const uint8_t* buffer, size_t size_bytes, void* user_ctx)
        : tx_buffer(buffer)
        , length_bits(size_bytes * 8)
        , flags(0)
        , user(user_ctx)
    {}
};

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for SPI peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF SPI Master operations.
/// Implementations:
/// - SpiPeripheralESP: Thin wrapper around ESP-IDF APIs (real hardware)
/// - SpiPeripheralMock: Simulation for host-based unit tests
///
/// ## Thread Safety
/// - initializeBus() NOT thread-safe (call once during setup)
/// - queueTransaction() can be called from ISR context (ISR-safe)
/// - Other methods NOT thread-safe (caller synchronizes)
///
/// ## Error Handling
/// All methods return bool for success/failure:
/// - true: Operation succeeded
/// - false: Operation failed (see logs for details)
class ISpiPeripheral {
public:
    virtual ~ISpiPeripheral() = default;

    //=========================================================================
    // Bus Lifecycle
    //=========================================================================

    /// @brief Initialize SPI bus with configuration
    /// @param config Bus configuration (MISO/MOSI pins, DMA channel)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_bus_initialize()
    ///
    /// This method:
    /// - Initializes the SPI bus (SPI2 or SPI3)
    /// - Allocates DMA channel
    /// - Configures GPIO pins
    /// - Must be called before addDevice()
    virtual bool initializeBus(const SpiBusConfig& config) = 0;

    /// @brief Add a device to the initialized bus
    /// @param config Device configuration (clock speed, mode, queue size)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_bus_add_device()
    ///
    /// This method:
    /// - Attaches a device to the bus
    /// - Allocates transaction queue
    /// - Returns a device handle (stored internally)
    /// - Must be called after initializeBus()
    virtual bool addDevice(const SpiDeviceConfig& config) = 0;

    /// @brief Remove device from bus
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_bus_remove_device()
    ///
    /// Must be called before freeBus().
    virtual bool removeDevice() = 0;

    /// @brief Free SPI bus resources
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_bus_free()
    ///
    /// Must be called after removeDevice().
    virtual bool freeBus() = 0;

    /// @brief Check if bus is initialized
    /// @return true if initialized, false otherwise
    virtual bool isInitialized() const = 0;

    //=========================================================================
    // Transaction API
    //=========================================================================

    /// @brief Queue a transaction for asynchronous DMA transmission
    /// @param trans Transaction descriptor (buffer, size, flags)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_device_queue_trans()
    ///
    /// This method:
    /// - Queues the transaction in the DMA queue
    /// - Returns immediately (non-blocking)
    /// - Transmission happens in background via DMA
    /// - Completion triggers post-transaction callback (if registered)
    ///
    /// The buffer MUST remain valid until pollTransaction() returns
    /// or the post-transaction callback fires.
    virtual bool queueTransaction(const SpiTransaction& trans) = 0;

    /// @brief Poll for transaction completion
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if transaction completed, false on timeout or error
    ///
    /// Maps to ESP-IDF: spi_device_get_trans_result()
    ///
    /// Blocks until:
    /// - Oldest queued transaction completes (FIFO order)
    /// - Timeout occurs
    ///
    /// Returns true if transaction completes successfully.
    /// Returns false on timeout or hardware error.
    virtual bool pollTransaction(uint32_t timeout_ms) = 0;

    /// @brief Register post-transaction callback
    /// @param callback Function pointer (cast to void*)
    /// @param user_ctx User context pointer (passed to callback)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: spi_post_trans_cb in device config
    ///
    /// Callback signature (cast from void*):
    /// ```cpp
    /// void callback(spi_transaction_t* trans);
    /// ```
    ///
    /// The callback:
    /// - Runs AFTER each transaction completes
    /// - Runs in RTOS task context (NOT ISR context on ESP32)
    /// - Can perform logging, buffer management, etc.
    /// - Should be fast (<100µs ideal)
    ///
    /// ⚠️  CALLBACK SAFETY:
    /// - CAN log (FL_DBG, FL_WARN, printf)
    /// - CAN access shared state with mutexes
    /// - SHOULD minimize execution time
    /// - AVOID blocking operations (delay, long loops)
    virtual bool registerCallback(void* callback, void* user_ctx) = 0;

    //=========================================================================
    // DMA Memory Management
    //=========================================================================

    /// @brief Allocate DMA-capable buffer with 32-bit alignment
    /// @param size Size in bytes (will be rounded up to 4-byte multiple)
    /// @return Pointer to allocated buffer, or nullptr on error
    ///
    /// Maps to ESP-IDF: heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    ///
    /// The returned buffer:
    /// - Is 32-bit (4-byte) aligned
    /// - Is DMA-capable (non-cacheable SRAM)
    /// - Must be freed via freeDma() when done
    ///
    /// Size is automatically rounded up to 4-byte multiple to meet
    /// ESP32 DMA alignment requirements.
    virtual uint8_t* allocateDma(size_t size) = 0;

    /// @brief Free DMA buffer allocated via allocateDma()
    /// @param buffer Buffer pointer (must be from allocateDma())
    ///
    /// Maps to ESP-IDF: heap_caps_free()
    ///
    /// Safe to call with nullptr (no-op).
    virtual void freeDma(uint8_t* buffer) = 0;

    //=========================================================================
    // Platform Utilities
    //=========================================================================

    /// @brief Portable task delay (platform-independent sleep)
    /// @param ms Delay duration in milliseconds
    ///
    /// Maps to platform-specific delay:
    /// - ESP32/FreeRTOS: vTaskDelay(pdMS_TO_TICKS(ms))
    /// - Host/Mock: std::this_thread::sleep_for() or usleep()
    virtual void delay(uint32_t ms) = 0;

    /// @brief Get current timestamp in microseconds
    /// @return Current timestamp in microseconds (monotonic clock)
    ///
    /// Maps to:
    /// - ESP32: esp_timer_get_time()
    /// - Mock: std::chrono::high_resolution_clock or simulated time
    virtual uint64_t getMicroseconds() = 0;
};

} // namespace detail
} // namespace fl
