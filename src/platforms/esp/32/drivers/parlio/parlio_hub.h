/// @file parlio_hub.h
/// @brief Central hub for coordinating multiple PARLIO transmitters
///
/// The ParlioHub acts as the central nervous system for the parallel I/O architecture,
/// coordinating all transmitters to prevent conflicts and ensure proper sequencing.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "fl/vector.h"

namespace fl {

// Forward declaration
template<typename T, int N> class Singleton;

/// @brief Central hub coordinating all active PARLIO transmitters
///
/// Enables multi-chipset support by coordinating flush timing across different
/// transmitters to prevent hardware conflicts and ensure proper sequencing.
class ParlioHub {
public:
    /// Get singleton instance
    static ParlioHub& getInstance();

    /// Register a transmitter for coordination
    /// @param transmitterPtr Opaque pointer to the transmitter (void* for type erasure)
    /// @param flushFunc Function pointer to call transmitter's flush() method
    void registerTransmitter(void* transmitterPtr, void (*flushFunc)(void*));

    /// Unregister a transmitter (called during destruction)
    /// @param transmitterPtr Pointer to transmitter to remove from hub
    void unregisterTransmitter(void* transmitterPtr);

    /// Flush all registered transmitters
    /// Called at end of frame or when explicit flush needed
    void flushAll();

    /// Flush all transmitters except the specified one
    /// Called when switching chipsets mid-frame
    /// @param exceptPtr Pointer to transmitter that should NOT be flushed
    void flushAllExcept(void* exceptPtr);

    // Non-copyable, non-movable (singleton pattern)
    ParlioHub(const ParlioHub&) = delete;
    ParlioHub& operator=(const ParlioHub&) = delete;
    ParlioHub(ParlioHub&&) = delete;
    ParlioHub& operator=(ParlioHub&&) = delete;

private:
    /// Private constructor (singleton pattern)
    ParlioHub() = default;

    /// Destructor
    ~ParlioHub() = default;

    // Singleton needs access to private constructor
    friend class fl::Singleton<ParlioHub>;

    /// Transmitter entry in hub
    struct TransmitterEntry {
        void* transmitterPtr;        ///< Opaque pointer to transmitter
        void (*flushFunc)(void*);    ///< Function to flush this transmitter

        bool operator==(const TransmitterEntry& other) const {
            return transmitterPtr == other.transmitterPtr;
        }
    };

    /// List of registered transmitters
    fl::vector_fixed<TransmitterEntry, 16> mTransmitters;

    /// Check if entry already exists in hub
    /// @param entry Entry to search for
    /// @returns true if entry is already registered
    bool contains(const TransmitterEntry& entry);
};

} // namespace fl
