/// @file rmt5_support_stubs.h
/// @brief Stub implementations of RMT5 support classes for host-based testing
///
/// This file provides mock/stub implementations of ESP32-specific classes used
/// by the RMT5 channel engine. These stubs enable unit testing of
/// ChannelEngineRMT on host platforms without requiring real ESP32 hardware.
///
/// Classes provided:
/// - NetworkDetector: Stub network detection (always returns false)
/// - NetworkStateTracker: Stub network state tracking (no-op)
/// - RmtMemoryManager: Stub RMT memory management (always succeeds)
/// - RMTBufferPool: Stub buffer pool (uses std::vector)
///
/// These are only used when FASTLED_STUB_IMPL is defined (host testing builds).

#pragma once

// IWYU pragma: private

#include "fl/stl/vector.h"
#include "fl/stl/span.h"

// Platform constants for stub builds
#define FASTLED_RMT5_CLOCK_HZ 40000000  // 40 MHz (stub value)
#define SOC_RMT_MEM_WORDS_PER_CHANNEL 64  // Stub value (doesn't matter for host tests)
#define FL_RMT5_INTERRUPT_LEVEL 3  // Stub value
#define CONFIG_IDF_TARGET "stub"  // Platform identifier
#define IRAM_ATTR  // Stub: No IRAM attribute on host

// Stub ESP-IDF error types
#define ESP_OK 0  // Success code
using esp_err_t = int;  // Error type
inline const char* esp_err_to_name(esp_err_t err) {
    return err == 0 ? "ESP_OK" : "ESP_ERR";
}

// Stub ESP32 RMT types
struct rmt_tx_done_event_data_t {
    fl::u32 num_symbols;
};
using rmt_channel_handle_t = void*;

namespace fl {
namespace detail {

//=============================================================================
// Stub NetworkDetector
//=============================================================================

/// @brief Stub network detection for host testing
///
/// Always returns false (no network active) since host tests don't have WiFi/BT.
class NetworkDetector {
public:
    static bool isAnyNetworkActive() { return false; }
};

//=============================================================================
// Stub NetworkStateTracker
//=============================================================================

/// @brief Stub network state tracker for host testing
///
/// No-op implementation - network state never changes in host tests.
class NetworkStateTracker {
public:
    static NetworkStateTracker& instance() {
        static NetworkStateTracker inst;
        return inst;
    }
    bool hasChanged() { return false; }
    bool isActive() { return false; }
};

//=============================================================================
// Stub RmtMemoryManager
//=============================================================================

/// @brief Stub RMT memory manager for host testing
///
/// Always succeeds in allocations since host platforms have unlimited memory.
/// Tracks allocation calls for testing purposes but doesn't enforce limits.
class RmtMemoryManager {
public:
    struct AllocationResult {
        bool ok() const { return true; }
        fl::size value() const { return 64; }
    };

    static RmtMemoryManager& instance() {
        static RmtMemoryManager mgr;
        return mgr;
    }

    static fl::size calculateMemoryBlocks(bool) { return 2; }

    AllocationResult allocateTx(u8, bool, bool) { return AllocationResult{}; }
    void free(u8, bool) {}
    void recordRecoveryAllocation(u8, fl::size, bool) {}
    bool isDMAAvailable() { return false; }
    bool allocateDMA(u8, bool) { return false; }
    void freeDMA(u8, bool) {}
    fl::size availableTxWords() { return 256; }
    int getDMAChannelsInUse() { return 0; }
    bool hasActiveRxChannels() const { return false; }
};

//=============================================================================
// Stub RMTBufferPool
//=============================================================================

/// @brief Stub RMT buffer pool for host testing
///
/// Uses std::vector for buffer storage instead of DMA-capable memory.
/// No alignment requirements or DMA constraints on host platforms.
class RMTBufferPool {
    fl::vector<u8> mInternalBuffer;
    fl::vector<u8> mDMABuffer;
public:
    fl::span<u8> acquireInternal(fl::size size) {
        mInternalBuffer.resize(size);
        return fl::span<u8>(mInternalBuffer.data(), size);
    }

    fl::span<u8> acquireDMA(fl::size size) {
        mDMABuffer.resize(size);
        return fl::span<u8>(mDMABuffer.data(), size);
    }

    void releaseInternal(fl::span<u8>) {}
    void releaseDMA() {}
};

} // namespace detail
} // namespace fl
