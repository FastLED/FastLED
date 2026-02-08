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
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"  // For vTaskDelay
#include "freertos/task.h"       // For portTICK_PERIOD_MS
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
    // Use usb_serial_jtag_is_driver_installed() to check if driver already exists
    if (usb_serial_jtag_is_driver_installed()) {
        esp_rom_printf("USB-Serial JTAG: Driver already installed (inherited from Arduino or bootloader)\n");
        mBuffered = true;
        mInstalledDriver = false;  // We didn't install it, so don't uninstall it
        return true;  // Success: driver is installed and functional
    }

    // Driver not installed - install it ourselves
    esp_rom_printf("USB-Serial JTAG: Driver not detected, installing...\n");

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

        // Verify installation worked
        if (usb_serial_jtag_is_driver_installed()) {
            esp_rom_printf("USB-Serial JTAG: Verification OK - buffered mode active\n");
        } else {
            esp_rom_printf("WARNING: USB-Serial JTAG verification failed\n");
        }

        return true;
    }

    // Installation failed - will fall back to ROM UART
    esp_rom_printf("ERROR: USB-Serial JTAG driver installation failed (err=0x%x) - FALLING BACK TO ROM UART\n", err);
    return false;

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
        // Use usb_serial_jtag_write_bytes() - copies to TX ring buffer, non-blocking
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        esp_rom_printf("[DEBUG] USB-Serial JTAG buffered write: len=%d, first_chars='%.20s'\n",
                      static_cast<int>(len), str);

        int written = usb_serial_jtag_write_bytes(str, len, 0);  // timeout=0 (non-blocking)
        esp_rom_printf("[DEBUG] USB-Serial JTAG usb_serial_jtag_write_bytes returned: %d\n", written);

        if (written < 0) {
            // USB write failed - log error
            esp_rom_printf("ERROR: USB-Serial JTAG write failed (err=%d, len=%d)\n",
                          written, static_cast<int>(len));
            // Keep mBuffered = true - don't fall back silently
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
        // Use usb_serial_jtag_write_bytes() - handles raw binary data
        int written = usb_serial_jtag_write_bytes(buffer, size, 0);  // timeout=0 (non-blocking)
        return (written < 0) ? 0 : static_cast<size_t>(written);
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
        // Write string and newline
        size_t len = 0;
        const char* p = str;
        while (*p++) len++;

        int written = usb_serial_jtag_write_bytes(str, len, 0);  // timeout=0
        if (written < 0) {
            esp_rom_printf("ERROR: USB-Serial JTAG writeln failed (err=%d, len=%d)\n",
                          written, static_cast<int>(len));
            return;
        }

        written = usb_serial_jtag_write_bytes("\n", 1, 0);
        if (written < 0) {
            esp_rom_printf("ERROR: USB-Serial JTAG newline write failed (err=%d)\n", written);
        }
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

    // Wait for TX buffer to empty (all data transmitted)
    // Convert milliseconds to FreeRTOS ticks
    TickType_t timeout_ticks = timeoutMs / portTICK_PERIOD_MS;
    esp_err_t err = usb_serial_jtag_wait_tx_done(timeout_ticks);

    return (err == ESP_OK);
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
