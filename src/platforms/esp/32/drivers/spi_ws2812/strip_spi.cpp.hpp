#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "platforms/esp/32/core/esp_log_control.h"  // Control ESP logging before including esp_log.h
#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "strip_spi.h"
#include "rgbw.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"
#include "fl/stl/vector.h"

namespace fl {
static const char *STRIP_SPI_TAG = "strip_spi";

/// @brief Encode a single LED color byte to SPI bits (WS2812)
/// @param data LED color byte (0-255)
/// @param buf Output buffer (must be zeroed, 3 bytes)
///
/// Each LED bit → 3 SPI bits at 2.5MHz:
/// - Low bit (0): 100 (binary) → ~400ns high, ~800ns low
/// - High bit (1): 110 (binary) → ~800ns high, ~400ns low
///
/// Ported directly from Espressif led_strip_spi_dev.cpp
static void encodeLedByte(uint8_t data, uint8_t* buf) {
    // WS2812 timing via SPI at 2.5MHz (400ns per bit):
    // Each LED bit needs 3 SPI bits:
    //   LED '0' → 100 (1×400ns high + 2×400ns low = 400ns + 800ns)
    //   LED '1' → 110 (2×400ns high + 1×400ns low = 800ns + 400ns)
    //
    // Process byte from MSB to LSB:
    // Byte 0bABCDEFGH → SPI bits: AAA|BBB|CCC|DDD|EEE|FFF|GGG|HHH (24 bits)
    //
    // Output format (3 bytes):
    //   buf[0] = AAABBBCC (bits 7-2 of output)
    //   buf[1] = CDDDEEEF (bits 15-10 of output)
    //   buf[2] = FFGGGHHH (bits 23-18 of output)

    // Buffer is zero-initialized, so we only need to set high bits
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;

    // Process each bit from MSB (bit 7) to LSB (bit 0)
    for (int bit = 7; bit >= 0; bit--) {
        uint8_t pattern = (data & (1 << bit)) ? 0b110 : 0b100;  // High bit: 110, Low bit: 100

        // Map LED bit position to SPI byte/bit positions
        int spi_bit_offset = (7 - bit) * 3;  // Each LED bit → 3 SPI bits
        int byte_idx = spi_bit_offset / 8;
        int bit_offset = spi_bit_offset % 8;

        // Write 3-bit pattern into output buffer
        if (bit_offset <= 5) {
            // Pattern fits entirely in one byte
            buf[byte_idx] |= (pattern << (5 - bit_offset));
        } else {
            // Pattern spans two bytes
            buf[byte_idx] |= (pattern >> (bit_offset - 5));
            buf[byte_idx + 1] |= (pattern << (13 - bit_offset));
        }
    }
}

struct SpiHostUsed {
    spi_host_device_t spi_host;
    bool used;
};

static SpiHostUsed gSpiHostUsed[] = {
    {SPI2_HOST, false},  // in order of preference
#if SOC_SPI_PERIPH_NUM > 2
    {SPI3_HOST, false},
#endif
    {SPI1_HOST, false},
};

static spi_host_device_t getNextAvailableSpiHost()
{
    for (int i = 0; i < sizeof(gSpiHostUsed) / sizeof(gSpiHostUsed[0]); i++)
    {
        if (!gSpiHostUsed[i].used)
        {
            gSpiHostUsed[i].used = true;
            return gSpiHostUsed[i].spi_host;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    return SPI_HOST_MAX;
}


static void releaseSpiHost(spi_host_device_t spi_host)
{
    for (int i = 0; i < sizeof(gSpiHostUsed) / sizeof(gSpiHostUsed[0]); i++)
    {
        if (gSpiHostUsed[i].spi_host == spi_host)
        {
            gSpiHostUsed[i].used = false;
            return;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
}


class SpiStripWs2812 : public ISpiStripWs2812 {
public:
    SpiStripWs2812(int pin, uint32_t led_count, ISpiStripWs2812::SpiHostMode spi_bus_mode, ISpiStripWs2812::DmaMode dma_mode = DMA_AUTO)
        : mLedCount(led_count)
        , mDrawIssued(false)
    {
        // Determine SPI host
        switch (spi_bus_mode) {
            case ISpiStripWs2812::SPI_HOST_MODE_AUTO:
                mSpiHost = getNextAvailableSpiHost();
                break;
            case ISpiStripWs2812::SPI_HOST_MODE_1:
                mSpiHost = SPI1_HOST;
                break;
            case ISpiStripWs2812::SPI_HOST_MODE_2:
                mSpiHost = SPI2_HOST;
                break;
#if SOC_SPI_PERIPH_NUM > 2
            case ISpiStripWs2812::SPI_HOST_MODE_3:
                mSpiHost = SPI3_HOST;
                break;
#endif
            default:
                ESP_ERROR_CHECK(ESP_ERR_NOT_SUPPORTED);
        }

        // Determine DMA mode
        bool with_dma = (dma_mode == ISpiStripWs2812::DMA_ENABLED) ||
                       (dma_mode == ISpiStripWs2812::DMA_AUTO);

        // Allocate LED pixel buffer (RGB: 3 bytes per pixel)
        mLedBuffer.resize(led_count * 3);
        fl::memset(mLedBuffer.data(), 0, mLedBuffer.size());

        // Allocate SPI buffer (3x LED data for WS2812 encoding)
        mSpiBuffer.resize(led_count * 3 * 3);  // Each LED byte → 3 SPI bytes
        fl::memset(mSpiBuffer.data(), 0, mSpiBuffer.size());

        // Initialize SPI bus
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = pin;
        bus_config.miso_io_num = -1;  // Not used
        bus_config.sclk_io_num = -1;  // Not used (data-only SPI)
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = mSpiBuffer.size();
        bus_config.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

        int dma_chan = with_dma ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED;
        esp_err_t ret = spi_bus_initialize(mSpiHost, &bus_config, dma_chan);
        if (ret != ESP_OK) {
            FL_WARN("SPI bus initialize failed: " << esp_err_to_name(ret));
            ESP_ERROR_CHECK(ret);
        }
        FL_DBG("SPI bus initialized on host " << mSpiHost);

        // Configure SPI device
        spi_device_interface_config_t dev_config = {};
        dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
        dev_config.clock_speed_hz = 2500000;  // 2.5MHz for WS2812
        dev_config.spics_io_num = -1;  // No CS pin
        dev_config.queue_size = 1;  // Single transaction at a time
        dev_config.flags = SPI_DEVICE_NO_DUMMY;

        ret = spi_bus_add_device(mSpiHost, &dev_config, &mSpiDevice);
        if (ret != ESP_OK) {
            FL_WARN("SPI device add failed: " << esp_err_to_name(ret));
            spi_bus_free(mSpiHost);
            ESP_ERROR_CHECK(ret);
        }
        FL_DBG("SPI device created for " << led_count << " LEDs on pin " << pin);
    }

    ~SpiStripWs2812() override
    {
        waitDone();

        if (mSpiDevice) {
            spi_bus_remove_device(mSpiDevice);
            mSpiDevice = nullptr;
        }

        spi_bus_free(mSpiHost);
        releaseSpiHost(mSpiHost);

        FL_DBG("SPI device destroyed");
    }

    void setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) override
    {
        if (index >= mLedCount) {
            FL_WARN("setPixel index out of range: " << index << " >= " << mLedCount);
            return;
        }

        // Store in RGB order (WS2812 expects GRB, but we'll handle that in encoding)
        // Actually, let's store in GRB order directly to match WS2812 protocol
        uint32_t offset = index * 3;
        mLedBuffer[offset + 0] = green;
        mLedBuffer[offset + 1] = red;
        mLedBuffer[offset + 2] = blue;
    }

    void drawAsync() override
    {
        if (mDrawIssued)
        {
            waitDone();
        }

        // Encode LED buffer to SPI buffer
        for (size_t i = 0; i < mLedBuffer.size(); i++) {
            encodeLedByte(mLedBuffer[i], &mSpiBuffer[i * 3]);
        }

        // Prepare SPI transaction
        fl::memset(&mTransaction, 0, sizeof(mTransaction));
        mTransaction.length = mSpiBuffer.size() * 8;  // Length in bits
        mTransaction.tx_buffer = mSpiBuffer.data();
        mTransaction.rx_buffer = nullptr;

        // Queue transaction (non-blocking)
        esp_err_t ret = spi_device_queue_trans(mSpiDevice, &mTransaction, portMAX_DELAY);
        if (ret != ESP_OK) {
            FL_WARN("SPI transaction queue failed: " << esp_err_to_name(ret));
            ESP_ERROR_CHECK(ret);
        }

        mDrawIssued = true;
        FL_DBG("SPI transaction queued (" << mSpiBuffer.size() << " bytes)");
    }

    void waitDone() override
    {
        if (!mDrawIssued)
        {
            return;
        }

        // Wait for transaction to complete
        spi_transaction_t* trans_ptr = nullptr;
        esp_err_t ret = spi_device_get_trans_result(mSpiDevice, &trans_ptr, portMAX_DELAY);
        if (ret != ESP_OK) {
            FL_WARN("SPI transaction wait failed: " << esp_err_to_name(ret));
            ESP_ERROR_CHECK(ret);
        }

        mDrawIssued = false;
        FL_DBG("SPI transaction complete");
    }

    bool isDrawing() override
    {
        return mDrawIssued;
    }

    void fill(uint8_t red, uint8_t green, uint8_t blue) {
        for (uint32_t i = 0; i < mLedCount; i++)
        {
            setPixel(i, red, green, blue);
        }
    }

    OutputIterator outputIterator() override
    {
        return OutputIterator(this, mLedCount);
    }

    u32 numPixels() override
    {
        return mLedCount;
    }

private:
    spi_host_device_t mSpiHost;
    spi_device_handle_t mSpiDevice;
    u32 mLedCount;
    bool mDrawIssued;

    fl::vector<uint8_t> mLedBuffer;   // LED pixel data (3 bytes per LED: GRB)
    fl::vector<uint8_t> mSpiBuffer;   // Encoded SPI data (9 bytes per LED)
    spi_transaction_t mTransaction;   // SPI transaction descriptor
};



ISpiStripWs2812::OutputIterator::OutputIterator(ISpiStripWs2812 *strip, uint32_t num_leds)
    : mStrip(strip), mNumLeds(num_leds) {}

ISpiStripWs2812::OutputIterator::~OutputIterator() {
    if (mWritten) {
        FASTLED_WARN("finish() was not called on OutputIterator before destruction.");
        finish();
    }
}

void ISpiStripWs2812::OutputIterator::flush() {
    mStrip->setPixel(mPosition, mRed, mGreen, mBlue);
    mRed = mGreen = mBlue = 0;
}

void ISpiStripWs2812::OutputIterator::operator()(uint8_t value) {
    switch (mWritten) {
    case 0:
        mRed = value;
        break;
    case 1:
        mGreen = value;
        break;
    case 2:
        mBlue = value;
        break;
    }
    mWritten++;
    if (mWritten == 3) {
        flush();
        mWritten = 0;
        mPosition++;
    }
}

void ISpiStripWs2812::OutputIterator::finish() {
    if (mWritten) {
        flush();
    }
}

ISpiStripWs2812* ISpiStripWs2812::Create(int pin, uint32_t led_count, bool is_rgbw, ISpiStripWs2812::SpiHostMode spi_bus, ISpiStripWs2812::DmaMode dma_mode) {
    if (!is_rgbw) {
        return new SpiStripWs2812(pin, led_count, spi_bus, dma_mode);
    }
    // Emulate RGBW mode by pretending the RGBW pixels are RGB pixels.
    uint32_t size_as_rgb = Rgbw::size_as_rgb(led_count);
    return new SpiStripWs2812(pin, size_as_rgb, spi_bus, dma_mode);
}
}  // namespace fl
#endif  // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif  // ESP32
