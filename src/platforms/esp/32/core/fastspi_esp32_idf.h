#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

// ESP32 Hardware SPI implementation using native ESP-IDF driver
// This file uses driver/spi_master.h for pure ESP-IDF builds without Arduino framework

#include "crgb.h"
#include "fastspi_types.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"  // For fl::memset
#include "fl/warn.h"  // For FL_WARN macro

FL_EXTERN_C_BEGIN
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
FL_EXTERN_C_END

namespace fl {

// Determine default SPI host based on chip variant
#if defined(FL_IS_ESP_32S2) || defined(FL_IS_ESP_32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
    #define FASTLED_ESP32_DEFAULT_SPI_HOST SPI2_HOST
#else
    // ESP32 classic - use SPI2_HOST (aka VSPI)
    #define FASTLED_ESP32_DEFAULT_SPI_HOST SPI2_HOST
#endif

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_SPEED>
class ESP32SPIOutput {
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;
    Selectable* mPSelect;
    bool mInitialized;
    bool mInTransaction;
    spi_transaction_t mTransaction;

public:
    // Verify that the pins are valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid data pin specified");
    static_assert(FastPin<CLOCK_PIN>::validpin(), "Invalid clock pin specified");

    ESP32SPIOutput()
        : mSPIHandle(nullptr)
        , mHost(FASTLED_ESP32_DEFAULT_SPI_HOST)
        , mPSelect(nullptr)
        , mInitialized(false)
        , mInTransaction(false)
    {
        fl::memset(&mTransaction, 0, sizeof(mTransaction));
    }

    ESP32SPIOutput(Selectable* pSelect)
        : mSPIHandle(nullptr)
        , mHost(FASTLED_ESP32_DEFAULT_SPI_HOST)
        , mPSelect(pSelect)
        , mInitialized(false)
        , mInTransaction(false)
    {
        fl::memset(&mTransaction, 0, sizeof(mTransaction));
    }

    ~ESP32SPIOutput() {
        if (mInitialized) {
            if (mSPIHandle) {
                spi_bus_remove_device(mSPIHandle);
                mSPIHandle = nullptr;
            }
            spi_bus_free(mHost);
            mInitialized = false;
        }
    }

    void setSelect(Selectable* pSelect) {
        mPSelect = pSelect;
    }

    void init() {
        if (mInitialized) {
            return;
        }

        // Configure SPI bus
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DATA_PIN;
        bus_config.miso_io_num = -1;  // Not used for LED strips
        bus_config.sclk_io_num = CLOCK_PIN;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = 4096;  // 4KB default buffer
        bus_config.flags = SPICOMMON_BUSFLAG_MASTER;

        // Initialize bus with auto DMA
        esp_err_t ret = spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            FL_WARN("SPI bus init failed: " << ret);
            return;
        }

        // Configure device
        spi_device_interface_config_t dev_config = {};
        dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
        dev_config.clock_speed_hz = SPI_SPEED;
        dev_config.spics_io_num = -1;  // No CS pin for LED strips
        dev_config.queue_size = 1;
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only

        // Add device to bus
        ret = spi_bus_add_device(mHost, &dev_config, &mSPIHandle);
        if (ret != ESP_OK) {
            FL_WARN("SPI add device failed: " << ret);
            spi_bus_free(mHost);
            return;
        }

        mInitialized = true;
    }

    static void stop() {}
    static void wait() {}
    static void waitFully() {}

    void writeByteNoWait(u8 b) __attribute__((always_inline)) {
        writeByte(b);
    }

    void writeBytePostWait(u8 b) __attribute__((always_inline)) {
        writeByte(b);
        wait();
    }

    void writeWord(u16 w) __attribute__((always_inline)) {
        writeByte(static_cast<u8>(w >> 8));
        writeByte(static_cast<u8>(w & 0xFF));
    }

    void writeByte(u8 b) {
        if (!mInitialized || !mSPIHandle) {
            return;
        }

        spi_transaction_t t = {};
        t.length = 8;  // 8 bits = 1 byte
        t.tx_data[0] = b;
        t.flags = SPI_TRANS_USE_TXDATA;  // Use inline tx_data buffer

        spi_device_polling_transmit(mSPIHandle, &t);
    }

    void writePixelsBulk(const CRGB* pixels, size_t n) {
        if (!mInitialized || !mSPIHandle || n == 0) {
            return;
        }

        const u8* data = fl::bit_cast<const u8*>(pixels);
        size_t n_bytes = n * 3;

        spi_transaction_t t = {};
        t.length = n_bytes * 8;  // Convert to bits
        t.tx_buffer = data;

        spi_device_polling_transmit(mSPIHandle, &t);
    }

    void select() {
        if (!mInitialized) {
            return;
        }

        // Acquire SPI bus for exclusive access
        spi_device_acquire_bus(mSPIHandle, portMAX_DELAY);
        mInTransaction = true;

        if (mPSelect != nullptr) {
            mPSelect->select();
        }
    }

    void release() {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        if (mPSelect != nullptr) {
            mPSelect->release();
        }

        // Release SPI bus
        spi_device_release_bus(mSPIHandle);
        mInTransaction = false;
    }

    void endTransaction() {
        waitFully();
        release();
    }

    void writeBytesValue(u8 value, int len) {
        select();
        writeBytesValueRaw(value, len);
        release();
    }

    void writeBytesValueRaw(u8 value, int len) {
        while (len--) {
            writeByte(value);
        }
    }

    template <class D>
    void writeBytes(FASTLED_REGISTER u8* data, int len) {
        select();
        u8* end = data + len;
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len, mSPIHandle);
        release();
    }

    void writeBytes(FASTLED_REGISTER u8* data, int len) {
        writeBytes<DATA_NOP>(data, len);
    }

    static void finalizeTransmission() {}

    template <u8 BIT>
    inline void writeBit(u8 b) {
        // Test bit BIT in value b, send 0xFF if set, 0x00 if clear
        writeByte((b & (1 << BIT)) ? 0xFF : 0x00);
    }

    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    __attribute__((noinline)) void writePixels(PixelController<RGB_ORDER> pixels, void* context) {
        #if FASTLED_ESP32_SPI_BULK_TRANSFER
        select();
        int len = pixels.mLen;
        CRGB data_block[FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE] = {0};
        size_t data_block_index = 0;

        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            if (data_block_index >= FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE) {
                writePixelsBulk(data_block, data_block_index);
                data_block_index = 0;
            }
            CRGB rgb(
                D::adjust(pixels.loadAndScale0()),
                D::adjust(pixels.loadAndScale1()),
                D::adjust(pixels.loadAndScale2())
            );
            data_block[data_block_index++] = rgb;
            pixels.advanceData();
            pixels.stepDithering();
        }
        if (data_block_index > 0) {
            writePixelsBulk(data_block, data_block_index);
            data_block_index = 0;
        }
        D::postBlock(len, context);
        release();
        #else
        select();
        int len = pixels.mLen;
        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            writeByte(D::adjust(pixels.loadAndScale0()));
            writeByte(D::adjust(pixels.loadAndScale1()));
            writeByte(D::adjust(pixels.loadAndScale2()));
            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len, context);
        release();
        #endif
    }
};

}  // namespace fl

FL_DISABLE_WARNING_POP
