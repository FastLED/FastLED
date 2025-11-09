/// @file parlio_engine.h
/// @brief ESP32-P4 PARLIO DMA engine - the powerhouse that drives it all
///
/// This is the beating heart of the parallel I/O system - the DMA engine that
/// powers multi-channel LED output with hardware-accelerated timing.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "fl/stdint.h"

namespace fl {

/// @brief Hardware configuration for PARLIO engine
struct ParlioHardwareConfig {
    int data_gpios[16];      ///< GPIO pins for data lanes
    int num_lanes;           ///< Active lane count (1, 2, 4, 8, or 16)
    uint32_t clock_freq_hz;  ///< PARLIO clock frequency
    size_t max_transfer_size; ///< Maximum DMA transfer size in bytes

    /// @brief Callback function for write completion
    /// @param tx_unit Hardware handle (opaque to caller)
    /// @param edata Event data from ESP-IDF
    /// @param user_ctx User context pointer
    /// @return true if high-priority task was awakened
    bool (*on_write_done)(void* tx_unit, const void* edata, void* user_ctx);

    void* callback_arg;      ///< User context for callback
};

/// @brief Abstract interface for the PARLIO DMA engine
///
/// The engine manages exclusive access to the PARLIO peripheral hardware,
/// ensuring only one group can use the DMA controller at a time
/// using semaphore-based locking for thread-safe access.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IParlioEngine {
public:
    /// @brief Get singleton instance
    /// @return Reference to the global DMA engine
    static IParlioEngine& getInstance();

    /// @brief Virtual destructor
    virtual ~IParlioEngine() = default;

    /// @brief Acquire exclusive access to PARLIO hardware (BLOCKING)
    ///
    /// This method blocks until the hardware becomes available.
    /// Multiple drivers calling acquire() will be serialized.
    ///
    /// @param owner Pointer to the driver acquiring the engine
    virtual void acquire(void* owner) = 0;

    /// @brief Release PARLIO hardware for next user (NON-BLOCKING)
    ///
    /// This method is called after starting an async DMA write operation.
    /// The hardware remains "busy" until waitForCompletion() is called.
    ///
    /// @param owner Pointer to the driver releasing the engine (must match acquire)
    virtual void release(void* owner) = 0;

    /// @brief Wait for any in-progress write operation to complete
    ///
    /// This method blocks until the hardware is idle.
    /// Useful for ensuring hardware is idle before reconfiguration.
    virtual void waitForCompletion() = 0;

    /// @brief Check if hardware is currently busy
    /// @return true if hardware is in use
    virtual bool isBusy() const = 0;

    /// @brief Get current owner of the hardware
    /// @return Pointer to current owner, or nullptr if idle
    virtual void* getCurrentOwner() const = 0;

    /// @brief Configure PARLIO hardware with GPIO pins and settings
    ///
    /// This must be called before any write operation can occur.
    /// Reconfiguration requires shutdown() first.
    ///
    /// @param config Hardware configuration (pins, frequency, callbacks)
    /// @return true if configuration succeeded
    virtual bool configure(const ParlioHardwareConfig& config) = 0;

    /// @brief Write data via PARLIO DMA
    ///
    /// Starts an asynchronous DMA write operation. The callback will be
    /// invoked when the write completes.
    ///
    /// @param data Pointer to DMA buffer (must be DMA-capable memory)
    /// @param bits Number of bits to write
    /// @param callback_arg Context pointer passed to callback
    /// @return true if write started successfully
    virtual bool write(const uint8_t* data, size_t bits, void* callback_arg) = 0;

    /// @brief Write data with automatic chunking for large transfers
    ///
    /// Breaks large writes into hardware-compatible chunks,
    /// ensuring chunk boundaries align with component boundaries.
    /// This is critical for maintaining LED color integrity.
    ///
    /// @param data Pointer to DMA buffer (must be DMA-capable memory)
    /// @param total_bits Total bits to write across all chunks
    /// @param bits_per_component Bits per LED component (for chunk alignment)
    /// @param callback_arg Context pointer passed to callback
    /// @return true if all chunks written successfully
    virtual bool write_chunked(
        const uint8_t* data,
        size_t total_bits,
        size_t bits_per_component,
        void* callback_arg
    ) = 0;

    /// @brief Wait for current write operation to complete
    ///
    /// Blocks until the hardware finishes the current DMA operation.
    virtual void wait_write_done() = 0;

    /// @brief Shutdown and release PARLIO hardware
    ///
    /// Disables the PARLIO peripheral and releases all resources.
    /// Must be called before reconfiguration.
    virtual void shutdown() = 0;

    /// @brief Check if hardware is configured and ready
    /// @return true if configure() was called successfully
    virtual bool is_configured() const = 0;

    /// @brief Allocate DMA buffers for double-buffering
    virtual bool allocate_buffers(size_t buffer_size) = 0;

    /// @brief Free DMA buffers
    virtual void free_buffers() = 0;

    /// @brief Get write buffer for packing data
    virtual uint8_t* get_write_buffer() = 0;

    /// @brief Get read buffer for transmission
    virtual const uint8_t* get_read_buffer() = 0;

    /// @brief Swap buffers after packing
    virtual void swap_buffers() = 0;

    /// @brief Get buffer size
    virtual size_t get_buffer_size() const = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IParlioEngine() = default;

    // Non-copyable, non-movable
    IParlioEngine(const IParlioEngine&) = delete;
    IParlioEngine& operator=(const IParlioEngine&) = delete;
    IParlioEngine(IParlioEngine&&) = delete;
    IParlioEngine& operator=(IParlioEngine&&) = delete;
};

}  // namespace fl
