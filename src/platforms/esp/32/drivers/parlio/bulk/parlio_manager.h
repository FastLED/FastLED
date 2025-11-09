/// @file parlio_manager.h
/// @brief Central manager for coordinating multiple PARLIO groups
///
/// The ParlioManager acts as the central nervous system for the parallel I/O architecture,
/// coordinating all groups to prevent conflicts and ensure proper sequencing.
/// It also serves as the SOLE gatekeeper for engine access.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "parlio_engine.h"

namespace fl {

// Forward declaration
class IParlioGroup;

/// @brief Abstract interface for PARLIO manager coordinating all active groups
///
/// Enables multi-chipset support by coordinating flush timing across different
/// groups to prevent hardware conflicts and ensure proper sequencing.
/// Acts as the SOLE gatekeeper for engine access - no other component should
/// directly access IParlioEngine.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IParlioManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to the global PARLIO manager
    static IParlioManager& getInstance();

    /// @brief Virtual destructor
    virtual ~IParlioManager() = default;

    /// @brief Register a group for coordination
    /// @param groupPtr Opaque pointer to the group (void* for type erasure)
    /// @param flushFunc Function pointer to call group's flush() method
    virtual void registerGroup(void* groupPtr, void (*flushFunc)(void*)) = 0;

    /// @brief Unregister a group (called during destruction)
    /// @param groupPtr Pointer to group to remove from hub
    virtual void unregisterGroup(void* groupPtr) = 0;

    /// @brief Flush all registered groups
    /// Called at end of frame or when explicit flush needed
    virtual void flushAll() = 0;

    /// @brief Flush all groups except the specified one
    /// Called when switching chipsets mid-frame
    /// @param exceptPtr Pointer to group that should NOT be flushed
    virtual void flushAllExcept(void* exceptPtr) = 0;

    // ===== BATCHING METHODS (Task 2.1) =====

    /// @brief Register a group for batched write operation at end of frame
    /// @param group Pointer to the IParlioGroup requesting write operation
    /// @note Groups call this instead of directly calling engine methods
    virtual void onEndShow(IParlioGroup* group) = 0;

    /// @brief Write all pending groups sequentially
    /// @note Called by flushAll() at end of frame
    /// @note This is the ONLY method that should call engine write methods
    virtual void write_all_groups() = 0;

    // ===== ENGINE GATEKEEPER METHODS =====
    // These methods proxy access to the engine singleton.
    // NO OTHER COMPONENT should call IParlioEngine::getInstance() directly.

    /// @brief Acquire exclusive access to PARLIO hardware (BLOCKING)
    /// @param owner Pointer to the driver acquiring the engine
    virtual void engineAcquire(void* owner) = 0;

    /// @brief Release PARLIO hardware for next user (NON-BLOCKING)
    /// @param owner Pointer to the driver releasing the engine
    virtual void engineRelease(void* owner) = 0;

    /// @brief Configure PARLIO hardware with GPIO pins and settings
    /// @param config Hardware configuration
    /// @return true if configuration succeeded
    virtual bool engineConfigure(const ParlioHardwareConfig& config) = 0;

    /// @brief Write data via PARLIO DMA
    /// @param data Pointer to DMA buffer
    /// @param bits Number of bits to write
    /// @param callback_arg Context pointer passed to callback
    /// @return true if write started successfully
    virtual bool engineWrite(const uint8_t* data, size_t bits, void* callback_arg) = 0;

    /// @brief Shutdown and release PARLIO hardware
    virtual void engineShutdown() = 0;

    /// @brief Check if hardware is configured and ready
    /// @return true if configure() was called successfully
    virtual bool engineIsConfigured() const = 0;

    /// @brief Wait for current write operation to complete
    virtual void engineWaitWriteDone() = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IParlioManager() = default;

    // Non-copyable, non-movable
    IParlioManager(const IParlioManager&) = delete;
    IParlioManager& operator=(const IParlioManager&) = delete;
    IParlioManager(IParlioManager&&) = delete;
    IParlioManager& operator=(IParlioManager&&) = delete;
};

} // namespace fl
