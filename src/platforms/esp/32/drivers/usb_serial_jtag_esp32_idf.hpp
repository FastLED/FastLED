// IWYU pragma: private

// ESP32 USB-Serial JTAG Driver Implementation - Full Feature Support

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "usb_serial_jtag_esp32.h"
#include "fl/stl/assert.h"
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

namespace fl {

// ============================================================================
// UsbSerialJtagEsp32 Implementation
// ============================================================================

UsbSerialJtagEsp32::UsbSerialJtagEsp32(const UsbSerialJtagConfig& config)
    : mConfig(config)
    , mBuffered(false)
    , mInstalledDriver(false) {

    // Initialize driver
    initDriver();
}

UsbSerialJtagEsp32::~UsbSerialJtagEsp32() {
#ifdef FL_HAS_USB_SERIAL_JTAG
    // Only uninstall if WE installed it (don't uninstall Arduino's driver)
    if (mInstalledDriver) {
        esp_rom_printf("USB-Serial JTAG: Uninstalling driver\n");
        usb_serial_jtag_driver_uninstall();
    }
#endif
}

UsbSerialJtagEsp32::UsbSerialJtagEsp32(UsbSerialJtagEsp32&& other) noexcept
    : mConfig(other.mConfig)
    , mBuffered(other.mBuffered)
    , mInstalledDriver(other.mInstalledDriver) {
    // Mark other as invalid (prevent double-uninstall)
    other.mBuffered = false;
    other.mInstalledDriver = false;
}

UsbSerialJtagEsp32& UsbSerialJtagEsp32::operator=(UsbSerialJtagEsp32&& other) noexcept {
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

        // Mark other as invalid
        other.mBuffered = false;
        other.mInstalledDriver = false;
    }
    return *this;
}

bool UsbSerialJtagEsp32::initDriver() {
#ifdef FL_HAS_USB_SERIAL_JTAG
    esp_rom_printf("\n=== USB-Serial JTAG Driver Init ===\n");

    // ROBUST DRIVER DETECTION:
    // usb_serial_jtag_is_driver_installed() was added in ESP-IDF 5.4.0
    // For earlier versions, we attempt to install and handle errors
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Use usb_serial_jtag_is_driver_installed() to check if driver already exists
    if (usb_serial_jtag_is_driver_installed()) {
        esp_rom_printf("USB-Serial JTAG: Driver already installed (inherited from Arduino or bootloader)\n");
        mBuffered = true;
        mInstalledDriver = false;  // We didn't install it, so don't uninstall it
        return true;  // Success: driver is installed and functional
    }

    // Driver not installed - install it ourselves
    esp_rom_printf("USB-Serial JTAG: Driver not detected, installing...\n");
#else
    // ESP-IDF < 5.4.0: No is_driver_installed() function available
    // Attempt to install driver - if it fails with ESP_ERR_INVALID_STATE, driver already installed
    esp_rom_printf("USB-Serial JTAG: Attempting driver installation...\n");
#endif

    // Configure USB-Serial JTAG with buffer sizes
    usb_serial_jtag_driver_config_t usb_config = {};
    usb_config.tx_buffer_size = static_cast<u32>(mConfig.txBufferSize);
    usb_config.rx_buffer_size = static_cast<u32>(mConfig.rxBufferSize);

    esp_rom_printf("USB-Serial JTAG: Installing driver (rx_buf=%d, tx_buf=%d)...\n",
                  static_cast<int>(mConfig.rxBufferSize),
                  static_cast<int>(mConfig.txBufferSize));

    esp_err_t err = usb_serial_jtag_driver_install(&usb_config);

    if (err == ESP_OK) {
        esp_rom_printf("USB-Serial JTAG: Driver installed successfully!\n");
        mBuffered = true;
        mInstalledDriver = true;  // We installed it, so we'll uninstall it later

        // Add small delay after driver installation (similar to UART)
        vTaskDelay(50 / portTICK_PERIOD_MS);  // 50ms delay
        esp_rom_printf("USB-Serial JTAG: Post-install delay complete\n");

        // Verify installation worked (only if function is available)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        if (usb_serial_jtag_is_driver_installed()) {
            esp_rom_printf("USB-Serial JTAG: Verification OK - buffered mode active\n");
        } else {
            esp_rom_printf("WARNING: USB-Serial JTAG verification failed\n");
        }
#else
        esp_rom_printf("USB-Serial JTAG: Driver installed (verification not available in IDF < 5.4)\n");
#endif

        return true;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    // Installation failed - will fall back to ROM UART
    esp_rom_printf("ERROR: USB-Serial JTAG driver installation failed (err=0x%x) - FALLING BACK TO ROM UART\n", err);
    return false;
#else
    // ESP-IDF < 5.4.0: If installation failed with ESP_ERR_INVALID_STATE, driver is already installed by Arduino
    if (err == ESP_ERR_INVALID_STATE) {
        esp_rom_printf("USB-Serial JTAG: Driver already installed by Arduino/bootloader\n");
        mBuffered = true;
        mInstalledDriver = false;  // We didn't install it, so don't uninstall it
        return true;
    }

    // Other errors - fall back to ROM UART
    esp_rom_printf("ERROR: USB-Serial JTAG driver installation failed (err=0x%x) - FALLING BACK TO ROM UART\n", err);
    return false;
#endif

#else
    // USB-Serial JTAG not available on this chip - fall back to ROM UART
    esp_rom_printf("USB-Serial JTAG: Not available on this chip - using ROM UART\n");
    return false;
#endif
}

void UsbSerialJtagEsp32::write(const char* str) {
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
                esp_rom_printf("ERROR: USB-Serial JTAG write failed (err=%d)\n", written);
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

size_t UsbSerialJtagEsp32::write(const u8* buffer, size_t size) {
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

void UsbSerialJtagEsp32::writeln(const char* str) {
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
                esp_rom_printf("ERROR: USB-Serial JTAG writeln failed (err=%d)\n", written);
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

int UsbSerialJtagEsp32::available() {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return 0;  // Driver not installed, no data available
    }

    // USB-Serial JTAG doesn't have a direct "get buffered data length" API
    // We can try a non-blocking read with len=0 to check, but that's not standard
    // For now, return 0 (not implemented)
    // TODO: Check if there's a better way to query RX buffer status
    return 0;
#else
    return 0;
#endif
}

int UsbSerialJtagEsp32::read() {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return -1;  // Driver not installed, cannot read
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

bool UsbSerialJtagEsp32::flush(u32 timeoutMs) {
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

bool UsbSerialJtagEsp32::isConnected() const {
#ifdef FL_HAS_USB_SERIAL_JTAG
    if (!mBuffered) {
        return false;  // Driver not installed, cannot check connection
    }

    return usb_serial_jtag_is_connected();
#else
    return false;
#endif
}

} // namespace fl

#endif // FL_IS_ESP32
