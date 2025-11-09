/// @file parlio_engine.cpp
/// @brief ESP32-P4 PARLIO DMA engine implementation
///
/// Implementation of the ParlioEngine singleton - the powerhouse that manages
/// exclusive access to the ESP32-P4 PARLIO DMA hardware for parallel LED output.

#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_engine.h"
#include "driver/parlio_tx.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fl/log.h"
#include "fl/singleton.h"

namespace fl {

// Helper function to convert ESP error to string
namespace detail {
inline const char* parlio_err_to_str(esp_err_t err) {
    return esp_err_to_name(err);
}
} // namespace detail

// Concrete implementation - all platform-specific types hidden in cpp
class ParlioEngine : public IParlioEngine {
public:
    ParlioEngine()
        : mOwner(nullptr)
        , mBusy(false)
        , mSemaphore(nullptr)
        , mTxUnit(nullptr)
        , mConfigured(false)
        , mBufferSize(0)
        , mWriteBuffer(nullptr)
        , mReadBuffer(nullptr)
    {
        // Create binary semaphore for hardware arbitration
        mSemaphore = xSemaphoreCreateBinary();
        if (!mSemaphore) {
            FL_WARN("PARLIO Engine: Failed to create semaphore!");
            return;
        }

        // Initially give semaphore (hardware available)
        xSemaphoreGive(mSemaphore);

        FL_DBG("PARLIO Engine: Initialized");
    }

    ~ParlioEngine() override {
        shutdown();
        free_buffers();
        if (mSemaphore) {
            vSemaphoreDelete(mSemaphore);
            mSemaphore = nullptr;
        }
        FL_DBG("PARLIO Engine: Destroyed");
    }

    void acquire(void* owner) override;
    void release(void* owner) override;
    void waitForCompletion() override;
    bool isBusy() const override;
    void* getCurrentOwner() const override;
    bool configure(const ParlioHardwareConfig& config) override;
    bool write(const uint8_t* data, size_t bits, void* callback_arg) override;
    bool write_chunked(const uint8_t* data, size_t total_bits, size_t bits_per_component, void* callback_arg) override;
    void wait_write_done() override;
    void shutdown() override;
    bool is_configured() const override;
    bool allocate_buffers(size_t buffer_size) override;
    void free_buffers() override;
    uint8_t* get_write_buffer() override;
    const uint8_t* get_read_buffer() override;
    void swap_buffers() override;
    size_t get_buffer_size() const override;

private:
    void* mOwner;                   ///< Current owner of the hardware (nullptr if idle)
    volatile bool mBusy;            ///< Flag indicating hardware is in use
    SemaphoreHandle_t mSemaphore;   ///< Binary semaphore for hardware arbitration
    parlio_tx_unit_handle_t mTxUnit; ///< ESP-IDF PARLIO hardware handle
    bool mConfigured;               ///< Configuration status flag
    ParlioHardwareConfig mConfig;   ///< Stored configuration
    size_t mBufferSize;             ///< Size of each DMA buffer
    uint8_t* mWriteBuffer;          ///< Write buffer for packing data
    uint8_t* mReadBuffer;           ///< Read buffer for transmission
};

// Static method - returns interface but creates concrete singleton
IParlioEngine& IParlioEngine::getInstance() {
    return fl::Singleton<ParlioEngine>::instance();
}

void ParlioEngine::acquire(void* owner) {
    // Wait for hardware to become available (blocking)
    xSemaphoreTake(mSemaphore, portMAX_DELAY);

    mOwner = owner;
    mBusy = true;

    FL_DBG("PARLIO Engine: Acquired by " << owner);
}

void ParlioEngine::release(void* owner) {
    if (mOwner != owner) {
        FL_WARN("PARLIO Engine: Release called by non-owner "
               << owner << " (current owner: " << mOwner << ")");
        return;
    }

    FL_DBG("PARLIO Engine: Released by " << owner);

    // Mark hardware as available for next acquire
    mOwner = nullptr;
    mBusy = false;

    // Give semaphore to unblock next waiting driver
    xSemaphoreGive(mSemaphore);
}

void ParlioEngine::waitForCompletion() {
    // Try to acquire (will block if busy)
    xSemaphoreTake(mSemaphore, portMAX_DELAY);

    FL_DBG("PARLIO Engine: Wait completed");

    // Immediately release (we just wanted to wait)
    xSemaphoreGive(mSemaphore);
}

bool ParlioEngine::isBusy() const {
    return mBusy;
}

void* ParlioEngine::getCurrentOwner() const {
    return mOwner;
}

bool ParlioEngine::configure(const ParlioHardwareConfig& config) {
    if (mConfigured) {
        FL_WARN("PARLIO Engine: Already configured - shutdown() first");
        return false;
    }

    // Validate max_transfer_size against hardware limit
    constexpr uint32_t MAX_BYTES_PER_CHUNK = 65535;
    if (config.max_transfer_size > MAX_BYTES_PER_CHUNK) {
        FL_WARN("PARLIO Engine: max_transfer_size (" << config.max_transfer_size
                << ") exceeds hardware limit (" << MAX_BYTES_PER_CHUNK << ")");
        return false;
    }

    FL_DBG("PARLIO Engine: Configuring hardware");

    // Configure PARLIO TX unit
    parlio_tx_unit_config_t parlio_config = {};
    parlio_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    parlio_config.clk_in_gpio_num = (gpio_num_t)-1;  // Use internal clock
    parlio_config.input_clk_src_freq_hz = 0;  // Not used when clk_in_gpio_num is -1
    parlio_config.output_clk_freq_hz = config.clock_freq_hz;
    parlio_config.data_width = config.num_lanes;
    parlio_config.clk_out_gpio_num = (gpio_num_t)-1;  // No external clock output
    parlio_config.valid_gpio_num = (gpio_num_t)-1;  // No separate valid signal
    parlio_config.trans_queue_depth = 4;
    parlio_config.max_transfer_size = config.max_transfer_size;
    parlio_config.dma_burst_size = 64;  // Standard DMA burst size
    parlio_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
    parlio_config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
    parlio_config.flags.clk_gate_en = 0;
    parlio_config.flags.io_loop_back = 0;
    parlio_config.flags.allow_pd = 0;

    // Copy GPIO numbers
    for (int i = 0; i < config.num_lanes; i++) {
        parlio_config.data_gpio_nums[i] = (gpio_num_t)config.data_gpios[i];
        FL_DBG("  data_gpio[" << i << "]: " << config.data_gpios[i]);
    }

    // Create PARLIO TX unit
    esp_err_t err = parlio_new_tx_unit(&parlio_config, &mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO Engine: parlio_new_tx_unit() failed: "
                << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
        return false;
    }

    // Register event callbacks
    parlio_tx_event_callbacks_t cbs = {};
    cbs.on_trans_done = reinterpret_cast<parlio_tx_done_callback_t>(config.on_write_done);
    err = parlio_tx_unit_register_event_callbacks(mTxUnit, &cbs, config.callback_arg);
    if (err != ESP_OK) {
        FL_WARN("PARLIO Engine: Failed to register callbacks: "
                << detail::parlio_err_to_str(err));
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
        return false;
    }

    // Enable PARLIO TX unit
    err = parlio_tx_unit_enable(mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO Engine: parlio_tx_unit_enable() failed: "
                << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
        return false;
    }

    // Store configuration
    mConfig = config;
    mConfigured = true;

    FL_DBG("PARLIO Engine: Configuration successful");
    FL_DBG("  Clock frequency: " << config.clock_freq_hz << " Hz");
    FL_DBG("  Data width: " << config.num_lanes << " lanes");
    FL_DBG("  Max transfer: " << config.max_transfer_size << " bytes");

    return true;
}

bool ParlioEngine::write(const uint8_t* data, size_t bits, void* callback_arg) {
    if (!mConfigured) {
        FL_WARN("PARLIO Engine: Not configured - call configure() first");
        return false;
    }

    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x00000000;  // Lines idle low
    tx_config.flags.queue_nonblocking = 0;

    esp_err_t err = parlio_tx_unit_transmit(mTxUnit, data, bits, &tx_config);
    if (err != ESP_OK) {
        FL_WARN("PARLIO Engine: parlio_tx_unit_transmit() failed: "
                << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
        return false;
    }

    FL_DBG("PARLIO Engine: Write started (" << bits << " bits)");
    return true;
}

bool ParlioEngine::write_chunked(
    const uint8_t* data,
    size_t total_bits,
    size_t bits_per_component,
    void* callback_arg
) {
    if (!mConfigured) {
        FL_WARN("PARLIO Engine: Not configured - call configure() first");
        return false;
    }

    // Hardware limit: max 65535 bytes per DMA transfer
    constexpr uint32_t MAX_BYTES_PER_CHUNK = 65535;
    const uint32_t MAX_BITS_PER_CHUNK = MAX_BYTES_PER_CHUNK * 8;

    // Calculate bytes per component (round up for bit-to-byte conversion)
    const uint32_t bytes_per_component = (bits_per_component + 7) / 8;

    // Calculate max components per chunk (must be whole components)
    const uint32_t max_components_per_chunk = MAX_BYTES_PER_CHUNK / bytes_per_component;

    // Calculate total components to write
    const uint32_t total_components = total_bits / bits_per_component;

    FL_DBG("PARLIO Engine: Chunked write starting");
    FL_DBG("  Total bits: " << total_bits);
    FL_DBG("  Bits per component: " << bits_per_component);
    FL_DBG("  Total components: " << total_components);
    FL_DBG("  Max components per chunk: " << max_components_per_chunk);

    // Write in component-aligned chunks
    uint32_t components_remaining = total_components;
    const uint8_t* chunk_ptr = data;

    while (components_remaining > 0) {
        // Determine how many components in this chunk
        uint32_t components_in_chunk = (components_remaining < max_components_per_chunk)
                                        ? components_remaining
                                        : max_components_per_chunk;

        // Calculate chunk size in bits and bytes
        size_t chunk_bits = components_in_chunk * bits_per_component;
        size_t chunk_bytes = components_in_chunk * bytes_per_component;

        FL_DBG("  Chunk: " << components_in_chunk << " components, "
               << chunk_bytes << " bytes, " << chunk_bits << " bits");

        // Write this chunk
        if (!write(chunk_ptr, chunk_bits, callback_arg)) {
            FL_WARN("PARLIO Engine: Chunk write failed");
            return false;
        }

        // Advance to next chunk
        chunk_ptr += chunk_bytes;
        components_remaining -= components_in_chunk;

        // Wait for completion if not the last chunk
        // This ensures DMA gaps occur at component boundaries
        if (components_remaining > 0) {
            wait_write_done();
        }
    }

    FL_DBG("PARLIO Engine: Chunked write complete");
    return true;
}

void ParlioEngine::wait_write_done() {
    // Wait for DMA completion via semaphore
    waitForCompletion();
}

void ParlioEngine::shutdown() {
    if (!mConfigured) {
        return;  // Already shutdown
    }

    FL_DBG("PARLIO Engine: Shutting down hardware");

    if (mTxUnit) {
        parlio_tx_unit_disable(mTxUnit);
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
    }

    mConfigured = false;
    FL_DBG("PARLIO Engine: Shutdown complete");
}

bool ParlioEngine::is_configured() const {
    return mConfigured;
}

bool ParlioEngine::allocate_buffers(size_t buffer_size) {
    // Free existing buffers if any
    free_buffers();

    // Allocate DMA-capable memory for double-buffering
    // ESP32 requires 32-bit alignment for DMA
    mWriteBuffer = static_cast<uint8_t*>(heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_32BIT));
    if (!mWriteBuffer) {
        FL_WARN("PARLIO Engine: Failed to allocate write buffer (" << buffer_size << " bytes)");
        return false;
    }

    mReadBuffer = static_cast<uint8_t*>(heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_32BIT));
    if (!mReadBuffer) {
        FL_WARN("PARLIO Engine: Failed to allocate read buffer (" << buffer_size << " bytes)");
        heap_caps_free(mWriteBuffer);
        mWriteBuffer = nullptr;
        return false;
    }

    mBufferSize = buffer_size;
    FL_DBG("PARLIO Engine: Allocated buffers (" << buffer_size << " bytes each)");
    return true;
}

void ParlioEngine::free_buffers() {
    if (mWriteBuffer) {
        heap_caps_free(mWriteBuffer);
        mWriteBuffer = nullptr;
    }
    if (mReadBuffer) {
        heap_caps_free(mReadBuffer);
        mReadBuffer = nullptr;
    }
    mBufferSize = 0;
    FL_DBG("PARLIO Engine: Freed buffers");
}

uint8_t* ParlioEngine::get_write_buffer() {
    return mWriteBuffer;
}

const uint8_t* ParlioEngine::get_read_buffer() {
    return mReadBuffer;
}

void ParlioEngine::swap_buffers() {
    // Swap pointers for ping-pong buffering
    uint8_t* temp = mWriteBuffer;
    mWriteBuffer = mReadBuffer;
    mReadBuffer = temp;
    FL_DBG("PARLIO Engine: Swapped buffers");
}

size_t ParlioEngine::get_buffer_size() const {
    return mBufferSize;
}

}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
