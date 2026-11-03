// IWYU pragma: private

/// @file spi_single_esp32.cpp
/// @brief ESP32 implementation of Single-SPI
///
/// This file provides the SPISingleESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.
///
/// This implementation uses true async DMA via ESP-IDF's spi_device_queue_trans()
/// and spi_device_get_trans_result() functions for non-blocking transmission.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "platforms/shared/spi_dma_stream.h"
#include "fl/channels/data.h"
#include "platforms/esp/32/drivers/spi/spi_hw_base.h"  // Common ESP32 SPI definitions
#include "platforms/esp/32/drivers/spi/spi_provider.hpp"
#include "fl/stl/cstring.h"
#include "fl/stl/limits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/unique_ptr.h"
// IWYU pragma: begin_keep
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
// IWYU pragma: end_keep

namespace fl {

namespace {
// Cached internal SRAM needs cache maintenance after every ISR refill. Keep
// its established block path until that platform's ISR-safe sync is verified.
#if defined(SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE) && SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
constexpr bool kSpiStreamCacheSafe = false;
#else
constexpr bool kSpiStreamCacheSafe = true;
#endif

struct SpiDmaStorage {
    struct Deleter {
        void operator()(u8* ptr) const FL_NO_EXCEPT { heap_caps_free(ptr); }
    };

    explicit SpiDmaStorage(size_t bytes) FL_NO_EXCEPT
        : storage(static_cast<u8*>(heap_caps_malloc(bytes,
              MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA))) {}

    fl::unique_ptr<u8, Deleter> storage;
};
} // namespace

// ============================================================================
// SPISingleESP32 Class Definition
// ============================================================================

/// ESP32 hardware for Single-SPI transmission
/// Implements SPISingle interface for ESP-IDF SPI peripheral
///
/// This implementation uses async DMA via spi_device_queue_trans()
class SPISingleESP32 : public SpiHw1 {
public:
    explicit SPISingleESP32(int bus_id = -1, const char* name = "Unknown") FL_NO_EXCEPT;
    ~SPISingleESP32();

    bool begin(const SpiHw1::Config& config) FL_NO_EXCEPT override;
    void end() FL_NO_EXCEPT override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) FL_NO_EXCEPT override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) FL_NO_EXCEPT override;
    bool supportsStreaming() const FL_NO_EXCEPT override;
    bool transmitStream(const ChannelDataPtr& source) FL_NO_EXCEPT override;
    void setStreamCompletionCallback(void (*callback)(void*), void* context) FL_NO_EXCEPT override;
    bool waitComplete(u32 timeout_ms = fl::numeric_limits<u32>::max()) FL_NO_EXCEPT override;
    bool isBusy() const FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;
    int getBusId() const FL_NO_EXCEPT override;
    const char* getName() const FL_NO_EXCEPT override;

private:
    void cleanup() FL_NO_EXCEPT;
    bool ensureQueueCapacity(size_t count) FL_NO_EXCEPT;
    void releaseStream() FL_NO_EXCEPT;
    static void streamPostCallback(spi_transaction_t* trans) FL_NO_EXCEPT;

    int mBusId;
    const char* mName;
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;
    bool mInitialized;
    bool mTransactionActive;
    bool mBusInitializedByUs = false;

    // DMA buffer management
    DMABuffer mDMABuffer;            // Current DMA buffer
    bool mBufferAcquired;

    // Transaction structure for async DMA
    spi_transaction_t mTransaction;

    spi_device_interface_config_t mDeviceConfig = {};
    size_t mQueueCapacity = 1;
    ChannelDataPtr mStreamSource;
    fl::shared_ptr<u8> mStreamBuffer;
    fl::vector<spi_transaction_t> mStreamTransactions;
    size_t mStreamQueued = 0;
    size_t mStreamReaped = 0;
    void (*mStreamCompletion)(void*) = nullptr;
    void* mStreamCompletionContext = nullptr;

    SPISingleESP32(const SPISingleESP32&) = delete;
    SPISingleESP32& operator=(const SPISingleESP32&) = delete;
};

// ============================================================================
// SPISingleESP32 Implementation
// ============================================================================

SPISingleESP32::SPISingleESP32(int bus_id, const char* name) FL_NO_EXCEPT
    : mBusId(bus_id)
    , mName(name)
    , mSPIHandle(nullptr)
    , mHost(SPI2_HOST)
    , mInitialized(false)
    , mTransactionActive(false)
    , mBusInitializedByUs(false)
    , mDMABuffer()
    , mBufferAcquired(false) {
}

SPISingleESP32::~SPISingleESP32() {
    cleanup();
    if (mTransactionActive) {
        // The SDK still owns this descriptor and its DMA storage. Destruction
        // cannot return safely after a failed final wait, even in release builds.
        __builtin_abort();
    }
}

bool SPISingleESP32::begin(const SpiHw1::Config& config) FL_NO_EXCEPT {
    if (mInitialized) {
        // A queue-resize allocation failure can also defeat rollback. Keep
        // bus ownership, but retry device creation rather than use a null handle.
        return mSPIHandle != nullptr ||
            spi_bus_add_device(mHost, &mDeviceConfig, &mSPIHandle) == ESP_OK;
    }

    mBusInitializedByUs = false;

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<u8>(mBusId)) {
        return false;  // Mismatch: driver is for bus X but config requests bus Y
    }

    // Use config.bus_num to determine SPI host
    u8 bus_num = (mBusId != -1) ? static_cast<u8>(mBusId) : config.bus_num;

    // Convert platform-agnostic bus_num to ESP32 SPI host
    switch (bus_num) {
    case 2:
        mHost = SPI2_HOST;
        break;
#if SOC_SPI_PERIPH_NUM > 2
    case 3:
        mHost = SPI3_HOST;
        break;
#endif
    default:
        return false;  // Invalid bus number
    }

    // Configure SPI bus for standard single-lane mode
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config.data_pin;
    bus_config.miso_io_num = -1;  // Not used for LED strips
    bus_config.sclk_io_num = config.clock_pin;
    bus_config.quadwp_io_num = -1;  // Not used
    bus_config.quadhd_io_num = -1;  // Not used
    bus_config.max_transfer_sz = config.max_transfer_sz;

    // Standard SPI mode (no dual/quad flags)
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;

    // Initialize bus with auto DMA channel selection
    esp_err_t ret = spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_INVALID_STATE) {
            // Genuine initialization failure — bail out
            return false;
        }
        mBusInitializedByUs = false;
        // ESP_ERR_INVALID_STATE means the bus is already initialized
        // by another driver. This is the canonical ESP-IDF pattern for
        // sharing an SPI bus: skip bus init and proceed to
        // spi_bus_add_device.
    } else {
        mBusInitializedByUs = true;
    }

    // Configure SPI device
    mDeviceConfig = {};
    mDeviceConfig.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
    mDeviceConfig.clock_speed_hz = config.clock_speed_hz;
    mDeviceConfig.spics_io_num = -1;  // No CS pin for LED strips
    mDeviceConfig.queue_size = 1;
    mQueueCapacity = 1;
    mDeviceConfig.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only mode
    if (mBusInitializedByUs && kSpiStreamCacheSafe) {
        // intr_flags is deliberately zero. This non-IRAM interrupt is masked
        // while cache is off, so post_cb can read a PSRAM-backed source.
        // An inherited bus may have IRAM interrupts; never install our refill
        // callback there because its cache-safety contract is unknown.
        mDeviceConfig.post_cb = &streamPostCallback;
    }

    // Add device to bus
    ret = spi_bus_add_device(mHost, &mDeviceConfig, &mSPIHandle);
    if (ret != ESP_OK) {
        if (mBusInitializedByUs) {
            spi_bus_free(mHost);
            mBusInitializedByUs = false;
        }
        return false;
    }

    mInitialized = true;

    return true;
}

void SPISingleESP32::end() FL_NO_EXCEPT {
    cleanup();
}

DMABuffer SPISingleESP32::acquireDMABuffer(size_t bytes_per_lane) FL_NO_EXCEPT {
    if (!mInitialized || (!mSPIHandle &&
        spi_bus_add_device(mHost, &mDeviceConfig, &mSPIHandle) != ESP_OK)) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For single SPI: total size = bytes_per_lane × 1 lane
    constexpr size_t num_lanes = 1;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against max_transfer_sz from bus config
    // ESP32 SPI max is typically 64KB per transaction
    if (total_size > 65536) {
        return DMABuffer(SPIError::BUFFER_TOO_LARGE);
    }

    // Allocate new DMABuffer - it will manage its own memory
    mBufferAcquired = false;
    mDMABuffer = DMABuffer(total_size);
    // The shared buffer object can exist even if its vector storage failed.
    if (!mDMABuffer.ok() || mDMABuffer.size() != total_size) {
        mDMABuffer.reset();
        return DMABuffer(SPIError::ALLOCATION_FAILED);
    }

    mBufferAcquired = true;

    // Return the DMABuffer
    return mDMABuffer;
}

bool SPISingleESP32::transmit(TransmitMode mode) FL_NO_EXCEPT {
    if (!mInitialized || !mSPIHandle || !mBufferAcquired || mTransactionActive) {
        return false;
    }

    if (!mDMABuffer.ok() || mDMABuffer.size() == 0) {
        return true;  // Nothing to transmit
    }

    // Mode is ignored - ESP32 always does async via DMA
    (void)mode;

    // Get the buffer span
    fl::span<u8> buffer_span = mDMABuffer.data();

    // Configure transaction using internal DMA buffer
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
    mTransaction.length = buffer_span.size() * 8;   // Length in BITS (critical!)
    mTransaction.tx_buffer = buffer_span.data();
    mTransaction.user = this;

    // Queue transaction (non-blocking)
    mTransactionActive = true;
    esp_err_t ret = spi_device_queue_trans(mSPIHandle, &mTransaction, portMAX_DELAY);
    if (ret != ESP_OK) {
        mTransactionActive = false;
        return false;
    }

    return true;
}

bool SPISingleESP32::supportsStreaming() const FL_NO_EXCEPT {
    return kSpiStreamCacheSafe && (!mInitialized || mBusInitializedByUs);
}

void SPISingleESP32::setStreamCompletionCallback(void (*callback)(void*), void* context) FL_NO_EXCEPT {
    // The adapter installs one permanent trampoline before transmission and
    // clears it only after quiescing the device. Runtime manager registration
    // changes go through the adapter's atomic PollNeededCallbackSlot instead.
    if (mTransactionActive) return;
    mStreamCompletion = callback;
    mStreamCompletionContext = context;
}

bool SPISingleESP32::ensureQueueCapacity(size_t count) FL_NO_EXCEPT {
    if (!mSPIHandle && spi_bus_add_device(mHost, &mDeviceConfig, &mSPIHandle) != ESP_OK) return false;
    if (count <= mQueueCapacity) return mSPIHandle != nullptr;
    if (!mSPIHandle || spi_bus_remove_device(mSPIHandle) != ESP_OK) return false;
    mSPIHandle = nullptr;
    auto config = mDeviceConfig;
    config.queue_size = static_cast<fl::i32>(count);
    if (spi_bus_add_device(mHost, &config, &mSPIHandle) != ESP_OK) {
        // Restore the previous usable device after an allocation failure.
        spi_bus_add_device(mHost, &mDeviceConfig, &mSPIHandle);
        return false;
    }
    mDeviceConfig = config;
    mQueueCapacity = count;
    return true;
}

bool SPISingleESP32::transmitStream(const ChannelDataPtr& source) FL_NO_EXCEPT {
    if (!mInitialized || !supportsStreaming() || !source || mTransactionActive) return false;
    const auto& bytes = source->getData();
    if (bytes.empty()) return true;
    if (bytes.size() <= detail::SpiDmaStreamPlan::kChunkBytes) {
        // One-chunk frames fit the staging budget already. Preserve the small
        // whole-frame path instead of allocating stream ownership/descriptors.
        // The copy completes in task context; only mDMABuffer is needed by DMA.
        auto buffer = acquireDMABuffer(bytes.size());
        if (!buffer.ok() || buffer.size() != bytes.size()) return false;
        fl::memcpy(buffer.data().data(), bytes.data(), bytes.size());
        return transmit(TransmitMode::ASYNC);
    }
    const size_t count = detail::SpiDmaStreamPlan::chunkCount(bytes.size());
    if (count == 0 || !ensureQueueCapacity(count)) return false;

    const size_t capacity = bytes.size() < detail::SpiDmaStreamPlan::kChunkBytes
        ? bytes.size() : detail::SpiDmaStreamPlan::kChunkBytes;
    const size_t alignedCapacity = (capacity + 3) & ~size_t(3);
    auto allocation = fl::make_shared<SpiDmaStorage>(alignedCapacity);
    if (!allocation || !allocation->storage) return false;
    // Share the RAII owner's control block through the public aliasing
    // constructor; adopting malloc storage directly is intentionally private.
    mStreamBuffer = fl::shared_ptr<u8>(allocation, allocation->storage.get());
    auto* storage = mStreamBuffer.get();
    mStreamSource = source;
    mStreamTransactions.resize(count);
    if (mStreamTransactions.size() != count) {
        releaseStream();
        return false;
    }

    // Every descriptor references the same internal DMA storage. post_cb runs
    // after that DMA finishes and BEFORE the SDK prepares the next queued
    // transfer (IDF 4.4 spi_master.c spi_intr, IDF 5.x esp_driver_spi/gpspi).
    // Refill there, rather than pre_cb: the latter runs after DMA preparation
    // and can race a DMA FIFO prefetch. No SDK task API is called from the ISR.
    for (size_t i = 0; i < count; ++i) {
        auto& trans = mStreamTransactions[i];
        trans = {};
        trans.tx_buffer = mStreamBuffer.get();
        trans.length = detail::SpiDmaStreamPlan::chunkSize(bytes.size(), i) * 8;
        trans.user = this;
    }
    detail::SpiDmaStreamPlan::copyChunk(bytes, 0, fl::span<u8>(storage, capacity));
    mDMABuffer.reset();
    mBufferAcquired = false;
    mStreamQueued = 0;
    mStreamReaped = 0;
    mTransactionActive = true;

    for (auto& trans : mStreamTransactions) {
        if (spi_device_queue_trans(mSPIHandle, &trans, 0) != ESP_OK) {
            // A failed submission does not cancel already queued descriptors.
            // Drain them before releasing the source/staging storage.
            if (mStreamQueued != 0) {
                if (!waitComplete()) __builtin_abort();
            } else {
                mTransactionActive = false;
                releaseStream();
            }
            return false;
        }
        ++mStreamQueued;
    }
    return true;
}

void SPISingleESP32::streamPostCallback(spi_transaction_t* trans) FL_NO_EXCEPT {
    auto* self = static_cast<SPISingleESP32*>(trans->user);
    if (!self) return;
    if (!self->mStreamSource) {
        // Small automatic transfers and legacy block transfers use the member
        // descriptor, not the stream descriptor vector. Wake without indexing it.
        if (self->mStreamCompletion) self->mStreamCompletion(self->mStreamCompletionContext);
        return;
    }
    const size_t index = static_cast<size_t>(trans - self->mStreamTransactions.data());
    if (index + 1 < self->mStreamTransactions.size()) {
        detail::SpiDmaStreamPlan::copyChunk(self->mStreamSource->getData(), index + 1,
            fl::span<u8>(self->mStreamBuffer.get(), detail::SpiDmaStreamPlan::kChunkBytes));
    } else if (self->mStreamCompletion) {
        self->mStreamCompletion(self->mStreamCompletionContext);
    }
}

void SPISingleESP32::releaseStream() FL_NO_EXCEPT {
    // Task context only: no shared ownership or allocator work in the ISR.
    mStreamSource.reset();
    mStreamBuffer.reset();
    fl::vector<spi_transaction_t>().swap(mStreamTransactions);
    mStreamQueued = 0;
    mStreamReaped = 0;
}

bool SPISingleESP32::waitComplete(u32 timeout_ms) FL_NO_EXCEPT {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }
    if (!mSPIHandle) return false;

    if (mStreamSource) {
        const TickType_t start = xTaskGetTickCount();
        const bool forever = timeout_ms == (fl::numeric_limits<u32>::max)();
        const TickType_t budget = forever ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        while (mStreamReaped < mStreamQueued) {
            const TickType_t elapsed = xTaskGetTickCount() - start;
            const TickType_t remaining = forever ? portMAX_DELAY :
                (elapsed >= budget ? 0 : budget - elapsed);
            spi_transaction_t* result = nullptr;
            if (spi_device_get_trans_result(mSPIHandle, &result, remaining) != ESP_OK) return false;
            ++mStreamReaped;
        }
        mTransactionActive = false;
        releaseStream();
        return true;
    }

    spi_transaction_t* result = nullptr;
    esp_err_t ret = spi_device_get_trans_result(
        mSPIHandle,
        &result,
        timeout_ms == (fl::numeric_limits<u32>::max)()
            ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms)
    );

    // A timeout does not return ownership of the queued transaction or its
    // storage. Keep both alive so a later wait/poll can reap the real result.
    if (ret != ESP_OK) return false;

    mTransactionActive = false;

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mDMABuffer.reset();

    return (ret == ESP_OK);
}

bool SPISingleESP32::isBusy() const FL_NO_EXCEPT {
    // Complete a timed-out wait when the SDK result arrives. Otherwise the
    // channel manager could poll BUSY forever after an ordinary timeout.
    return mTransactionActive &&
           !const_cast<SPISingleESP32*>(this)->waitComplete(0);
}

bool SPISingleESP32::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

int SPISingleESP32::getBusId() const FL_NO_EXCEPT {
    return mBusId;
}

const char* SPISingleESP32::getName() const FL_NO_EXCEPT {
    return mName;
}

void SPISingleESP32::cleanup() FL_NO_EXCEPT {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            if (!waitComplete()) return;
        }

        // Release DMA buffer
        mDMABuffer.reset();
        mBufferAcquired = false;

        // Remove device and free bus
        if (mSPIHandle) {
            spi_bus_remove_device(mSPIHandle);
            mSPIHandle = nullptr;
        }

        if (mBusInitializedByUs) {
            spi_bus_free(mHost);
            mBusInitializedByUs = false;
        }
        mInitialized = false;
    }
}

// ============================================================================
// Instance Management (accessed by spi_esp32_init.cpp)
// ============================================================================

// Singleton getters for controller instances (Meyer's Singleton pattern)
// These are called from the centralized registration in spi_esp32_init.cpp
// Return as SpiHw1 base class pointer to avoid forward declaration issues
namespace platforms {
namespace detail {
fl::shared_ptr<SpiHw1> makeNativeSpiHw1(u8 bus) FL_NO_EXCEPT {
    if (bus == 2) {
        return fl::make_shared<SPISingleESP32>(2, "SPI2");
    }
#if SOC_SPI_PERIPH_NUM > 2
    if (bus == 3) {
        return fl::make_shared<SPISingleESP32>(3, "SPI3");
    }
#endif
    return {};
}
} // namespace detail
} // namespace platforms

}  // namespace fl

#endif  // FL_IS_ESP32
