// ESP32 UART Driver Implementation - Full Feature Support

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "uart_esp32.h"
#include "fl/stl/assert.h"
#include "fl/singleton.h"
#include "platforms/esp/esp_version.h"

// ESP-IDF headers (only in .cpp.hpp, not in .h)
FL_EXTERN_C_BEGIN
#include "driver/uart.h"
#include "esp_rom_uart.h"
#include "freertos/FreeRTOS.h"  // For vTaskDelay
#include "freertos/task.h"       // For portTICK_PERIOD_MS
FL_EXTERN_C_END

namespace fl {

// ============================================================================
// Helper: Convert fl enum classes to ESP-IDF types
// ============================================================================

static uart_port_t convertPort(UartPort port) {
    return static_cast<uart_port_t>(static_cast<uint8_t>(port));
}

static uart_word_length_t convertDataBits(UartDataBits bits) {
    switch (bits) {
        case UartDataBits::BITS_5: return UART_DATA_5_BITS;
        case UartDataBits::BITS_6: return UART_DATA_6_BITS;
        case UartDataBits::BITS_7: return UART_DATA_7_BITS;
        case UartDataBits::BITS_8: return UART_DATA_8_BITS;
        default: return UART_DATA_8_BITS;
    }
}

static uart_parity_t convertParity(UartParity parity) {
    switch (parity) {
        case UartParity::NONE: return UART_PARITY_DISABLE;
        case UartParity::ODD:  return UART_PARITY_ODD;
        case UartParity::EVEN: return UART_PARITY_EVEN;
        default: return UART_PARITY_DISABLE;
    }
}

static uart_stop_bits_t convertStopBits(UartStopBits bits) {
    switch (bits) {
        case UartStopBits::BITS_1:   return UART_STOP_BITS_1;
        case UartStopBits::BITS_1_5: return UART_STOP_BITS_1_5;
        case UartStopBits::BITS_2:   return UART_STOP_BITS_2;
        default: return UART_STOP_BITS_1;
    }
}

static uart_hw_flowcontrol_t convertFlowControl(UartFlowControl ctrl) {
    switch (ctrl) {
        case UartFlowControl::NONE:    return UART_HW_FLOWCTRL_DISABLE;
        case UartFlowControl::RTS:     return UART_HW_FLOWCTRL_RTS;
        case UartFlowControl::CTS:     return UART_HW_FLOWCTRL_CTS;
        case UartFlowControl::RTS_CTS: return UART_HW_FLOWCTRL_CTS_RTS;
        default: return UART_HW_FLOWCTRL_DISABLE;
    }
}

static int convertIntrFlags(UartIntrPriority priority, UartIntrFlags flags) {
    int esp_flags = 0;

    // Add priority level (LEVEL1-5)
    // ESP-IDF uses ESP_INTR_FLAG_LEVEL1/2/3/4/5
    // Encoded as (1 << level) in lower bits
    int level = static_cast<int>(priority);
    esp_flags |= (1 << level);

    // Add IRAM flag if requested (ESP_INTR_FLAG_IRAM = 0x400)
    bool use_iram = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(UartIntrFlags::IRAM)) != 0;
    if (use_iram) {
        esp_flags |= ESP_INTR_FLAG_IRAM;
    }

    // Add SHARED flag if requested (ESP_INTR_FLAG_SHARED = 0x800)
    bool use_shared = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(UartIntrFlags::SHARED)) != 0;
    if (use_shared) {
        esp_flags |= ESP_INTR_FLAG_SHARED;
    }

    return esp_flags;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
static uart_sclk_t convertClockSource(UartClockSource source) {
    switch (source) {
        case UartClockSource::CLK_DEFAULT:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            return UART_SCLK_DEFAULT;
#else
            return UART_SCLK_APB;
#endif
        case UartClockSource::CLK_APB:      return UART_SCLK_APB;
        case UartClockSource::CLK_REF_TICK:
#if defined(UART_SCLK_REF_TICK)
            return UART_SCLK_REF_TICK;
#else
            return UART_SCLK_APB;  // Fallback if REF_TICK not supported
#endif
        case UartClockSource::CLK_XTAL:
#if defined(UART_SCLK_XTAL)
            return UART_SCLK_XTAL;
#else
            return UART_SCLK_APB;  // Fallback if XTAL not supported
#endif
        default:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            return UART_SCLK_DEFAULT;
#else
            return UART_SCLK_APB;
#endif
    }
}
#endif

// ============================================================================
// UartManager Implementation (Internal)
// ============================================================================

namespace {

/**
 * @brief UART allocation manager (singleton) - INTERNAL IMPLEMENTATION DETAIL
 *
 * Tracks which UART ports are currently allocated to prevent double allocation.
 * FL_ASSERT fires if attempting to allocate an already-allocated UART.
 *
 * THREAD SAFETY: Not thread-safe - allocate UARTs during setup(), not from multiple tasks.
 */
class UartManager {
public:
    static UartManager& instance() {
        return fl::Singleton<UartManager>::instance();
    }

    bool allocateUart(UartPort port) {
        int index = static_cast<int>(port);
        FL_ASSERT(index >= 0 && index < 3, "Invalid UART port");
        FL_ASSERT(!mAllocated[index], "UART port already allocated");

        mAllocated[index] = true;
        return true;
    }

    void deallocateUart(UartPort port) {
        int index = static_cast<int>(port);
        if (index >= 0 && index < 3) {
            mAllocated[index] = false;
        }
    }

    bool isAllocated(UartPort port) const {
        int index = static_cast<int>(port);
        if (index < 0 || index >= 3) {
            return false;
        }
        return mAllocated[index];
    }

private:
    bool mAllocated[3] = {false, false, false};  // UART0, UART1, UART2

    UartManager() = default;
    friend class fl::Singleton<UartManager>;
};

} // anonymous namespace

// ============================================================================
// UartEsp32 Implementation
// ============================================================================

UartEsp32::UartEsp32(const UartConfig& config)
    : mConfig(config)
    , mPortInt(static_cast<int>(convertPort(config.port)))
    , mBuffered(false)
    , mEventQueue(nullptr) {

    // Check allocation status
    UartManager& manager = UartManager::instance();
    FL_ASSERT(!manager.isAllocated(config.port),
              "UART port already allocated - cannot allocate twice");

    // Allocate the UART
    manager.allocateUart(config.port);

    // Initialize driver
    initDriver();
}

UartEsp32::~UartEsp32() {
    // Deallocate UART
    if (mPortInt >= 0) {
        UartManager& manager = UartManager::instance();
        manager.deallocateUart(mConfig.port);
    }
}

UartEsp32::UartEsp32(UartEsp32&& other) noexcept
    : mConfig(other.mConfig)
    , mPortInt(other.mPortInt)
    , mBuffered(other.mBuffered)
    , mEventQueue(other.mEventQueue) {
    // Mark other as invalid (prevent deallocation)
    other.mPortInt = -1;
    other.mEventQueue = nullptr;
}

UartEsp32& UartEsp32::operator=(UartEsp32&& other) noexcept {
    if (this != &other) {
        // Deallocate our current UART
        if (mPortInt >= 0) {
            UartManager& manager = UartManager::instance();
            manager.deallocateUart(mConfig.port);
        }

        // Take ownership of other's UART
        mConfig = other.mConfig;
        mPortInt = other.mPortInt;
        mBuffered = other.mBuffered;
        mEventQueue = other.mEventQueue;

        // Mark other as invalid
        other.mPortInt = -1;
        other.mEventQueue = nullptr;
    }
    return *this;
}

bool UartEsp32::initDriver() {
    uart_port_t port = static_cast<uart_port_t>(mPortInt);

    // ROBUST DRIVER DETECTION:
    // Use uart_get_buffered_data_len() as the definitive test for driver installation.
    // This is more reliable than uart_write_bytes(port, "", 0) which can succeed even
    // without a driver installed (0-byte write is a no-op).
    size_t buffered_len = 0;
    esp_err_t check_err = uart_get_buffered_data_len(port, &buffered_len);

    if (check_err == ESP_OK) {
        // Driver is installed and working - use it
        mBuffered = true;
        return true;  // Success: driver is installed and functional
    }

    // Driver not installed - install it ourselves

    // Configure UART parameters from UartConfig
    uart_config_t uart_config = {};
    uart_config.baud_rate = static_cast<int>(mConfig.baudRate);
    uart_config.data_bits = convertDataBits(mConfig.dataBits);
    uart_config.parity = convertParity(mConfig.parity);
    uart_config.stop_bits = convertStopBits(mConfig.stopBits);
    uart_config.flow_ctrl = convertFlowControl(mConfig.flowControl);
    uart_config.rx_flow_ctrl_thresh = static_cast<uint8_t>(mConfig.rxFlowControlThresh);

    // Set clock source (version-dependent field)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    uart_config.source_clk = convertClockSource(mConfig.clockSource);
#endif
    // IDF 3.x: source_clk field doesn't exist - skip it

    esp_err_t err = uart_param_config(port, &uart_config);
    if (err != ESP_OK) {
        return false;  // Failed - mBuffered remains false, will use ROM UART
    }

    // Configure pins if specified
    if (mConfig.pins.txPin >= 0 || mConfig.pins.rxPin >= 0 ||
        mConfig.pins.rtsPin >= 0 || mConfig.pins.ctsPin >= 0) {
        err = uart_set_pin(
            port,
            mConfig.pins.txPin,   // TX pin
            mConfig.pins.rxPin,   // RX pin
            mConfig.pins.rtsPin,  // RTS pin
            mConfig.pins.ctsPin   // CTS pin
        );
        if (err != ESP_OK) {
            return false;  // Pin configuration failed
        }
    }

    // Configure signal inversion if requested
    if (mConfig.signalInvert.txInvert || mConfig.signalInvert.rxInvert ||
        mConfig.signalInvert.rtsInvert || mConfig.signalInvert.ctsInvert) {
        uint32_t inv_mask = 0;
        if (mConfig.signalInvert.txInvert)  inv_mask |= UART_SIGNAL_TXD_INV;
        if (mConfig.signalInvert.rxInvert)  inv_mask |= UART_SIGNAL_RXD_INV;
        if (mConfig.signalInvert.rtsInvert) inv_mask |= UART_SIGNAL_RTS_INV;
        if (mConfig.signalInvert.ctsInvert) inv_mask |= UART_SIGNAL_CTS_INV;

        err = uart_set_line_inverse(port, inv_mask);
        if (err != ESP_OK) {
            return false;  // Signal inversion failed
        }
    }

    // Convert interrupt priority + flags to ESP-IDF format
    int intr_alloc_flags = convertIntrFlags(mConfig.intrPriority, mConfig.intrFlags);

    // Install driver with TX and RX buffers from config
    // Event queue: 0 = polling mode (we use uart_read_bytes directly)
    //             >0 = event-driven mode (ISR posts uart_event_t to queue)
    // Interrupt flags: Controls ISR priority and IRAM placement
    //   ESP_INTR_FLAG_IRAM (0x400) = ISR runs from IRAM (survives flash cache disable)
    //   ESP_INTR_FLAG_LEVEL1-5 = ISR priority level
    void** queue_handle_ptr = mConfig.eventQueueSize > 0 ? &mEventQueue : nullptr;

    err = uart_driver_install(
        port,
        static_cast<int>(mConfig.rxBufferSize),      // RX buffer size
        static_cast<int>(mConfig.txBufferSize),      // TX buffer size
        static_cast<int>(mConfig.eventQueueSize),    // Event queue size (0 = no queue)
        (QueueHandle_t*)queue_handle_ptr,            // Event queue handle
        intr_alloc_flags                             // ISR priority + flags
    );

    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means already installed - OK
        mBuffered = true;

        // CRITICAL FIX: Add delay after driver installation
        // ESP-IDF UART driver requires a delay after installation before transmitting
        // Without this, uart_write_bytes() will succeed but data won't transmit
        // Reference: https://esp32.com/viewtopic.php?t=2319
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms delay

        // Configure RS485 mode if enabled
        if (mConfig.rs485Config.enabled) {
            // Note: RS485 mode configuration varies by ESP-IDF version
            // For now, we assume RTS pin is already configured above
            // and ESP-IDF will handle RS485 mode automatically if RTS is set
            // Future: Add explicit uart_set_mode(UART_MODE_RS485_HALF_DUPLEX) if available
        }

        return true;
    }

    // Installation failed - will fall back to ROM UART
    return false;
}

void UartEsp32::write(const char* str) {
    if (!str)
        return;

    uart_port_t port = static_cast<uart_port_t>(mPortInt);

    if (mBuffered) {
        // Use uart_write_bytes() - copies to TX ring buffer, non-blocking
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        int written = uart_write_bytes(port, str, len);
        (void)written;  // Suppress unused variable warning in release builds
    } else {
        // Fallback to ROM UART (direct FIFO writes, blocks if full)
        while (*str) {
            esp_rom_output_tx_one_char(*str++);
        }
    }
}

size_t UartEsp32::write(const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0)
        return 0;

    uart_port_t port = static_cast<uart_port_t>(mPortInt);

    if (mBuffered) {
        // Use uart_write_bytes() - handles raw binary data
        // ESP-IDF UART driver handles the buffer internally
        int written = uart_write_bytes(port, buffer, size);
        return (written < 0) ? 0 : static_cast<size_t>(written);
    } else {
        // Fallback to ROM UART (byte-by-byte)
        for (size_t i = 0; i < size; i++) {
            esp_rom_output_tx_one_char(static_cast<char>(buffer[i]));
        }
        return size;
    }
}

void UartEsp32::writeln(const char* str) {
    if (!str)
        return;

    uart_port_t port = static_cast<uart_port_t>(mPortInt);

    if (mBuffered) {
        // Write string and newline
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        uart_write_bytes(port, str, len);
        uart_write_bytes(port, "\n", 1);
    } else {
        // Fallback to ROM UART
        while (*str) {
            esp_rom_output_tx_one_char(*str++);
        }
        esp_rom_output_tx_one_char('\n');
    }
}

int UartEsp32::available() {
    if (!mBuffered) {
        return 0;  // Driver not installed, no data available
    }

    uart_port_t port = static_cast<uart_port_t>(mPortInt);
    size_t available = 0;
    esp_err_t err = uart_get_buffered_data_len(port, &available);
    if (err != ESP_OK) {
        return 0;  // Error querying buffer, report no data
    }

    return static_cast<int>(available);
}

int UartEsp32::read() {
    if (!mBuffered) {
        return -1;  // Driver not installed, cannot read
    }

    uart_port_t port = static_cast<uart_port_t>(mPortInt);
    uint8_t c = 0;
    int len = uart_read_bytes(port, &c, 1, 0);  // timeout=0 (non-blocking)

    if (len == 1) {
        return static_cast<int>(c);  // Success: return byte as unsigned
    } else {
        return -1;  // No data available or error
    }
}

bool UartEsp32::flush(uint32_t timeoutMs) {
    if (!mBuffered) {
        return false;  // Driver not installed, cannot flush
    }

    uart_port_t port = static_cast<uart_port_t>(mPortInt);

    // Wait for TX FIFO to empty (all data transmitted)
    // Convert milliseconds to FreeRTOS ticks
    TickType_t timeout_ticks = timeoutMs / portTICK_PERIOD_MS;
    esp_err_t err = uart_wait_tx_done(port, timeout_ticks);

    return (err == ESP_OK);
}

} // namespace fl

#endif // FL_IS_ESP32
