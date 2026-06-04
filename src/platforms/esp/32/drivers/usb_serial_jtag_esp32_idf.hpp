// IWYU pragma: private

// ESP32 USB-Serial JTAG Driver Implementation - Full Feature Support

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "usb_serial_jtag_esp32.h"
#include "fl/log/log.h"  // FL_WARN
#include "fl/stl/assert.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/esp_version.h"

// ESP-IDF headers (only in .cpp.hpp, not in .h)
FL_EXTERN_C_BEGIN
#include "esp_rom_uart.h"

// USB-Serial JTAG driver is only available on ESP32-S3, C3, C6, H2 with IDF 4.4+
#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FL_HAS_USB_SERIAL_JTAG 1
// IWYU pragma: begin_keep
#include "driver/usb_serial_jtag.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"  // For vTaskDelay
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"       // For portTICK_PERIOD_MS
// IWYU pragma: end_keep
#endif
#endif

FL_EXTERN_C_END

// Compatibility: esp_rom_output_tx_one_char was added in ESP-IDF 5.3.
// Older IDF versions only have esp_rom_uart_tx_one_char.
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#define esp_rom_output_tx_one_char(c) esp_rom_uart_tx_one_char(c)
#endif

namespace fl {

// ============================================================================
// UsbSerialJtagEsp32 Implementation
// ============================================================================

UsbSerialJtagEsp32::UsbSerialJtagEsp32(const UsbSerialJtagConfig& config)
    : mConfig(config)
    , mBuffered(false)
    , mInstalledDriver(false)
    , mHasPeek(false)
    , mPeekByte(0)
    , mInitOutcome(InitOutcome::kNotInitialized)
    , mInitError(0)
    , mRxCacheRead(0)
    , mRxCacheCount(0) {

    // Initialize driver
    initDriver();
}

UsbSerialJtagEsp32::~UsbSerialJtagEsp32() {
#ifdef FL_HAS_USB_SERIAL_JTAG
    // Only uninstall if WE installed it (don't uninstall Arduino's driver)
    if (mInstalledDriver) {
        usb_serial_jtag_driver_uninstall();
    }
#endif
}

UsbSerialJtagEsp32::UsbSerialJtagEsp32(UsbSerialJtagEsp32&& other) FL_NOEXCEPT
    : mConfig(other.mConfig)
    , mBuffered(other.mBuffered)
    , mInstalledDriver(other.mInstalledDriver)
    , mHasPeek(other.mHasPeek)
    , mPeekByte(other.mPeekByte)
    , mInitOutcome(other.mInitOutcome)
    , mInitError(other.mInitError)
    , mRxCacheRead(other.mRxCacheRead)
    , mRxCacheCount(other.mRxCacheCount) {
    for (size_t i = 0; i < kRxCacheSize; ++i) {
        mRxCache[i] = other.mRxCache[i];
    }
    // Mark other as invalid (prevent double-uninstall)
    other.mBuffered = false;
    other.mInstalledDriver = false;
    other.mHasPeek = false;
    other.mPeekByte = 0;
    other.mRxCacheRead = 0;
    other.mRxCacheCount = 0;
}

UsbSerialJtagEsp32& UsbSerialJtagEsp32::operator=(UsbSerialJtagEsp32&& other) FL_NOEXCEPT {
    if (this != &other) {
#ifdef FL_HAS_USB_SERIAL_JTAG
        // Uninstall our current driver if we own it
        if (mInstalledDriver) {
            usb_serial_jtag_driver_uninstall();
        }
#endif

        // Take ownership of other's driver
        mConfig = other.mConfig;
        mBuffered = other.mBuffered;
        mInstalledDriver = other.mInstalledDriver;
        mHasPeek = other.mHasPeek;
        mPeekByte = other.mPeekByte;
        mInitOutcome = other.mInitOutcome;
        mInitError = other.mInitError;
        mRxCacheRead = other.mRxCacheRead;
        mRxCacheCount = other.mRxCacheCount;
        for (size_t i = 0; i < kRxCacheSize; ++i) {
            mRxCache[i] = other.mRxCache[i];
        }

        // Mark other as invalid
        other.mBuffered = false;
        other.mInstalledDriver = false;
        other.mHasPeek = false;
        other.mPeekByte = 0;
        other.mRxCacheRead = 0;
        other.mRxCacheCount = 0;
    }
    return *this;
}

bool UsbSerialJtagEsp32::initDriver() FL_NOEXCEPT {
    // NOTE: this runs from the UsbSerialJtagEsp32 / EspIO singleton
    // constructor — FL_PRINT would re-enter the in-progress singleton. All
    // outcome state is recorded in mInitOutcome / mInitError; EspIO surfaces
    // a single human-readable diagnostic once the normal logging sink is up.
#ifdef FL_HAS_USB_SERIAL_JTAG
    // ROBUST DRIVER DETECTION:
    // usb_serial_jtag_is_driver_installed() was added in ESP-IDF 5.4.0
    // For earlier versions, we attempt to install and handle errors
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Use usb_serial_jtag_is_driver_installed() to check if driver already exists
    if (usb_serial_jtag_is_driver_installed()) {
        mBuffered = true;
        mInstalledDriver = false;  // We didn't install it, so don't uninstall it
        mInitOutcome = InitOutcome::kPreInstalled;
        return true;  // Success: driver is installed and functional
    }
#endif

    // Configure USB-Serial JTAG with buffer sizes
    usb_serial_jtag_driver_config_t usb_config = {};
    usb_config.tx_buffer_size = static_cast<u32>(mConfig.txBufferSize);
    usb_config.rx_buffer_size = static_cast<u32>(mConfig.rxBufferSize);

    esp_err_t err = usb_serial_jtag_driver_install(&usb_config);

    if (err == ESP_OK) {
        mBuffered = true;
        mInstalledDriver = true;  // We installed it, so we'll uninstall it later

        // Add small delay after driver installation (similar to UART)
        vTaskDelay(50 / portTICK_PERIOD_MS);  // 50ms delay

        // Verify installation worked (only if function is available)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        if (usb_serial_jtag_is_driver_installed()) {
            mInitOutcome = InitOutcome::kInstalledOk;
        } else {
            mInitOutcome = InitOutcome::kVerificationFailed;
        }
#else
        mInitOutcome = InitOutcome::kInstalledUnverified;
#endif

        return true;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Installation failed - will fall back to ROM UART
    mInitOutcome = InitOutcome::kInstallFailed;
    mInitError = static_cast<i32>(err);
    return false;
#else
    // ESP-IDF < 5.4.0: If installation failed with ESP_ERR_INVALID_STATE, driver is already installed by Arduino
    if (err == ESP_ERR_INVALID_STATE) {
        mBuffered = true;
        mInstalledDriver = false;  // We didn't install it, so don't uninstall it
        mInitOutcome = InitOutcome::kPreInstalled;
        return true;
    }

    // Other errors - fall back to ROM UART
    mInitOutcome = InitOutcome::kInstallFailed;
    mInitError = static_cast<i32>(err);
    return false;
#endif

#else
    // USB-Serial JTAG not available on this chip - fall back to ROM UART
    mInitOutcome = InitOutcome::kNotAvailable;
    return false;
#endif
}

void UsbSerialJtagEsp32::write(const char* str) FL_NOEXCEPT {
    if (!str)
        return;

#ifdef FL_HAS_USB_SERIAL_JTAG
    if (mBuffered) {
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        // Write in chunks to handle data larger than the TX ring buffer.
        // timeout=0 non-blocking fails silently for large payloads; use a small
        // per-chunk timeout so the driver can drain between writes.
        size_t remaining = len;
        const char* src = str;
        while (remaining > 0) {
            size_t chunk = remaining < 512 ? remaining : 512;
            int written = usb_serial_jtag_write_bytes(src, chunk, pdMS_TO_TICKS(100));
            if (written > 0) {
                src += (size_t)written;
                remaining -= (size_t)written;
            } else if (written < 0) {
                FL_WARN("USB-Serial JTAG write failed: err=" << written);
                break;
            }
            // written == 0: buffer full, will retry after timeout
        }
        return;
    }
#endif

    // Fallback to ROM UART (direct FIFO writes, blocks if full)
    while (*str) {
        esp_rom_output_tx_one_char(*str++);
    }
}

size_t UsbSerialJtagEsp32::write(const u8* buffer, size_t size) FL_NOEXCEPT {
    if (!buffer || size == 0)
        return 0;

#ifdef FL_HAS_USB_SERIAL_JTAG
    if (mBuffered) {
        // Write in chunks to handle data larger than the TX ring buffer.
        size_t total_written = 0;
        const u8* src = buffer;
        size_t remaining = size;
        while (remaining > 0) {
            size_t chunk = remaining < 512 ? remaining : 512;
            int written = usb_serial_jtag_write_bytes(src, chunk, pdMS_TO_TICKS(100));
            if (written > 0) {
                src += (size_t)written;
                remaining -= (size_t)written;
                total_written += (size_t)written;
            } else if (written < 0) {
                break;  // Error
            }
            // written == 0: retry after timeout
        }
        return total_written;
    }
#endif

    // Fallback to ROM UART (byte-by-byte)
    for (size_t i = 0; i < size; i++) {
        esp_rom_output_tx_one_char(static_cast<char>(buffer[i]));
    }
    return size;
}

void UsbSerialJtagEsp32::writeln(const char* str) FL_NOEXCEPT {
    if (!str)
        return;

#ifdef FL_HAS_USB_SERIAL_JTAG
    if (mBuffered) {
        // Write all bytes in chunks to handle payloads larger than the TX ring buffer.
        // USB JTAG TX buffer defaults to 4096 bytes but JSON responses can be larger.
        // Using timeout=0 (non-blocking) fails silently for large payloads; a small
        // per-chunk timeout allows the driver to drain data between writes.
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        size_t remaining = len;
        const char* src = str;
        while (remaining > 0) {
            size_t chunk = remaining < 512 ? remaining : 512;
            int written = usb_serial_jtag_write_bytes(src, chunk, pdMS_TO_TICKS(100));
            if (written > 0) {
                src += (size_t)written;
                remaining -= (size_t)written;
            } else if (written < 0) {
                FL_WARN("USB-Serial JTAG writeln failed: err=" << written);
                break;
            }
            // written == 0: buffer full after timeout, retry
        }

        // Write newline (with timeout)
        usb_serial_jtag_write_bytes("\n", 1, pdMS_TO_TICKS(100));
        return;
    }
#endif

    // Fallback to ROM UART
    while (*str) {
        esp_rom_output_tx_one_char(*str++);
    }
    esp_rom_output_tx_one_char('\n');
}

int UsbSerialJtagEsp32::available() FL_NOEXCEPT {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return 0;  // Driver not installed, no data available
    }

    fillRxCache();
    return static_cast<int>(mRxCacheCount + (mHasPeek ? 1 : 0));
#else
    return 0;
#endif
}

int UsbSerialJtagEsp32::read() FL_NOEXCEPT {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return -1;  // Driver not installed, cannot read
    }

    if (mHasPeek) {
        mHasPeek = false;
        return static_cast<int>(mPeekByte);
    }

    if (mRxCacheCount > 0) {
        u8 c = mRxCache[mRxCacheRead];
        mRxCacheRead = (mRxCacheRead + 1) % kRxCacheSize;
        mRxCacheCount--;
        return static_cast<int>(c);
    }

    u8 c = 0;
    int len = usb_serial_jtag_read_bytes(&c, 1, 0);  // timeout=0 (non-blocking)

    if (len == 1) {
        return static_cast<int>(c);  // Success: return byte as unsigned
    } else {
        return -1;  // No data available or error
    }
#else
    return -1;
#endif
}

void UsbSerialJtagEsp32::fillRxCache() FL_NOEXCEPT {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return;
    }

    while (mRxCacheCount < kRxCacheSize) {
        size_t write_idx = (mRxCacheRead + mRxCacheCount) % kRxCacheSize;
        size_t contiguous = kRxCacheSize - write_idx;
        size_t free_count = kRxCacheSize - mRxCacheCount;
        if (contiguous > free_count) {
            contiguous = free_count;
        }

        int len = usb_serial_jtag_read_bytes(
            &mRxCache[write_idx],
            static_cast<u32>(contiguous),
            0);
        if (len <= 0) {
            return;
        }

        mRxCacheCount += static_cast<size_t>(len);
        if (static_cast<size_t>(len) < contiguous) {
            return;
        }
    }
#endif
}

bool UsbSerialJtagEsp32::flush(u32 timeoutMs) FL_NOEXCEPT {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return false;  // Driver not installed, cannot flush
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Wait for TX buffer to empty (all data transmitted)
    // Convert milliseconds to FreeRTOS ticks
    TickType_t timeout_ticks = timeoutMs / portTICK_PERIOD_MS;
    esp_err_t err = usb_serial_jtag_wait_tx_done(timeout_ticks);

    return (err == ESP_OK);
#else
    // ESP-IDF < 5.4.0: usb_serial_jtag_wait_tx_done() not available
    // Just add a delay to allow transmission to complete
    (void)timeoutMs;  // Unused in this fallback
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms delay for transmission
    return true;
#endif
#else
    return false;
#endif
}

bool UsbSerialJtagEsp32::isConnected() const FL_NOEXCEPT {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return false;  // Driver not installed, cannot check connection
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    return usb_serial_jtag_is_connected();
#else
    return true;  // Fallback for older IDF versions
#endif
#else
    return false;
#endif
}

} // namespace fl

#endif // FL_IS_ESP32
