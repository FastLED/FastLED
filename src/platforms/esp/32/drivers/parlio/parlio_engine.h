/// @file parlio_engine.h
/// @brief ESP32-P4 PARLIO DMA engine - the powerhouse that drives it all
///
/// This is the beating heart of the parallel I/O system - the DMA engine that
/// powers multi-channel LED transmission with hardware-accelerated timing.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

namespace fl {

/// @brief Abstract interface for the PARLIO DMA engine
///
/// The engine manages exclusive access to the PARLIO peripheral hardware,
/// ensuring only one transmitter can use the DMA controller at a time
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
    /// This method is called after starting an async DMA transmission.
    /// The hardware remains "busy" until waitForCompletion() is called.
    ///
    /// @param owner Pointer to the driver releasing the engine (must match acquire)
    virtual void release(void* owner) = 0;

    /// @brief Wait for any in-progress transmission to complete
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
