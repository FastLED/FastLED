/// @file rmt_memory_manager.cpp
/// @brief RMT memory allocation manager implementation

#include "rmt_memory_manager.h"

#if defined(ESP32) && FASTLED_RMT5

#include "fl/dbg.h"
#include "common.h"

FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
FL_EXTERN_C_END

namespace fl {

// ============================================================================
// Platform-Specific Memory Configuration
// ============================================================================

/// @brief Initialize platform-specific memory limits
///
/// Memory architecture varies by platform:
/// - ESP32/S2: Global pool (single shared memory for TX and RX)
/// - ESP32-S3/C3/C6/H2: Dedicated pools (separate TX and RX memory)
void RmtMemoryManager::initPlatformLimits(size_t& total_tx, size_t& total_rx) {
#if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32: 8 flexible channels × 64 words = 512 words SHARED global pool
    total_tx = 8 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 512 words (global pool)
    total_rx = 0;  // Not used for global pool
    FL_DBG("RMT Memory (ESP32): " << total_tx << " words GLOBAL POOL (shared TX/RX)");

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // ESP32-S2: 4 flexible channels × 64 words = 256 words SHARED global pool
    total_tx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 256 words (global pool)
    total_rx = 0;  // Not used for global pool
    FL_DBG("RMT Memory (ESP32-S2): " << total_tx << " words GLOBAL POOL (shared TX/RX)");

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: 4 dedicated TX + 4 dedicated RX channels × 48 words
    total_tx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 192 words (dedicated TX pool)
    total_rx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 192 words (dedicated RX pool)
    FL_DBG("RMT Memory (ESP32-S3): TX=" << total_tx << " words, RX=" << total_rx << " words (DEDICATED pools)");

#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
      defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    // ESP32-C3/C6/H2/C5: 2 dedicated TX + 2 dedicated RX channels × 48 words
    total_tx = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 96 words (dedicated TX pool)
    total_rx = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 96 words (dedicated RX pool)
    FL_DBG("RMT Memory (ESP32-C3/C6/H2): TX=" << total_tx << " words, RX=" << total_rx << " words (DEDICATED pools)");

#else
    // Unknown platform - assume dedicated pools (conservative)
    total_tx = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // Assume 2 TX channels
    total_rx = 2 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // Assume 2 RX channels
    FL_WARN("RMT Memory (Unknown platform): TX=" << total_tx << " words, RX=" << total_rx << " words (assumed DEDICATED)");
#endif
}

// ============================================================================
// MemoryLedger Implementation
// ============================================================================

RmtMemoryManager::MemoryLedger::MemoryLedger()
    : is_global_pool(false)
    , total_words(0)
    , allocated_words(0)
    , total_tx_words(0)
    , total_rx_words(0)
    , allocated_tx_words(0)
    , allocated_rx_words(0)
    , reserved_tx_words(0)
    , reserved_rx_words(0) {

    size_t tx_limit = 0, rx_limit = 0;
    initPlatformLimits(tx_limit, rx_limit);

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
    // Global pool platforms
    is_global_pool = true;
    total_words = tx_limit;  // tx_limit holds total pool size for global platforms
    allocated_words = 0;
#else
    // Dedicated pool platforms
    is_global_pool = false;
    total_tx_words = tx_limit;
    total_rx_words = rx_limit;
    allocated_tx_words = 0;
    allocated_rx_words = 0;
#endif
}

// ============================================================================
// RmtMemoryManager Implementation
// ============================================================================

RmtMemoryManager::RmtMemoryManager()
    : mLedger()
    , mDMAAllocation{0, false, false} {
}

RmtMemoryManager& RmtMemoryManager::instance() {
    static RmtMemoryManager instance;
    return instance;
}

size_t RmtMemoryManager::calculateMemoryBlocks(bool networkActive) {
    // Calculate if platform has enough TX memory for triple-buffering (3× blocks)
    // Triple-buffering one channel requires: 3 × SOC_RMT_MEM_WORDS_PER_CHANNEL words
    // Total TX memory available: SOC_RMT_TX_CANDIDATES_PER_GROUP × SOC_RMT_MEM_WORDS_PER_CHANNEL
    //
    // Example calculations:
    // - C3/C6/H2/C5: 2 × 48 = 96 words < 3 × 48 = 144 words → Cannot triple buffer
    // - ESP32-S3: 4 × 48 = 192 words >= 3 × 48 = 144 words → Can triple buffer
    // - ESP32: 8 × 64 = 512 words >= 3 × 64 = 192 words → Can triple buffer
    // - ESP32-S2: 4 × 64 = 256 words >= 3 × 64 = 192 words → Can triple buffer
#if SOC_RMT_TX_CANDIDATES_PER_GROUP < 3
    // Insufficient TX memory for triple-buffering (platforms with < 3 TX channels)
    (void)networkActive;  // Suppress unused parameter warning
    return FASTLED_RMT_MEM_BLOCKS;  // Always 2 (cannot support 3×)
#else
    // Sufficient TX memory for network-aware allocation
    if (networkActive) {
        return FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE;  // Default: 3 (triple-buffer for network)
    } else {
        return FASTLED_RMT_MEM_BLOCKS;  // Default: 2 (double-buffer for idle)
    }
#endif
}

Result<size_t, RmtMemoryError> RmtMemoryManager::allocateTx(uint8_t channel_id, bool use_dma, bool networkActive) {
    // Check if channel already allocated
    if (findAllocation(channel_id, true) != nullptr) {
        FL_WARN("RMT TX channel " << static_cast<int>(channel_id) << " already allocated");
        return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
    }

    // DMA channels bypass on-chip memory
    if (use_dma) {
        mLedger.allocations.push_back(ChannelAllocation(channel_id, 0, true, true));
        FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, bypasses on-chip memory)");
        return Result<size_t, RmtMemoryError>::success(0);
    }

    // Calculate adaptive buffer size based on network state
    size_t mem_blocks = calculateMemoryBlocks(networkActive);
    size_t words_needed = mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;

    // Try to allocate from appropriate pool
    if (!tryAllocateWords(words_needed, true)) {
        FL_WARN("RMT TX allocation failed: need " << words_needed
                << " words, only " << getAvailableWords(true) << " available");
        return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_TX_MEMORY);
    }

    mLedger.allocations.push_back(ChannelAllocation(channel_id, words_needed, true, false));

    FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
               << " allocated: " << words_needed << " words (" << mem_blocks << "× buffer"
               << (networkActive ? ", Network mode" : "") << ")");

    return Result<size_t, RmtMemoryError>::success(words_needed);
}

Result<size_t, RmtMemoryError> RmtMemoryManager::allocateRx(uint8_t channel_id, size_t symbols, bool use_dma) {
    // Check if channel already allocated
    if (findAllocation(channel_id, false) != nullptr) {
        FL_WARN("RMT RX channel " << static_cast<int>(channel_id) << " already allocated");
        return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
    }

    // DMA channels bypass on-chip memory (use DRAM instead)
    if (use_dma) {
        mLedger.allocations.push_back(ChannelAllocation(channel_id, 0, false, true));
        FL_LOG_RMT("RMT RX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, bypasses on-chip memory, uses DRAM buffer)");
        return Result<size_t, RmtMemoryError>::success(0);
    }

    // RX symbols = words (1 symbol = 1 word = 4 bytes)
    size_t words_needed = symbols;

    // Try to allocate from appropriate pool
    if (!tryAllocateWords(words_needed, false)) {
        FL_WARN("RMT RX allocation failed: need " << words_needed
                << " words, only " << getAvailableWords(false) << " available");
        return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_RX_MEMORY);
    }

    mLedger.allocations.push_back(ChannelAllocation(channel_id, words_needed, false, false));

    FL_LOG_RMT("RMT RX channel " << static_cast<int>(channel_id)
               << " allocated: " << words_needed << " words (" << symbols << " symbols)");

    return Result<size_t, RmtMemoryError>::success(words_needed);
}

void RmtMemoryManager::free(uint8_t channel_id, bool is_tx) {
    ChannelAllocation* alloc = findAllocation(channel_id, is_tx);
    if (!alloc) {
        FL_WARN("RMT " << (is_tx ? "TX" : "RX") << " channel "
                << static_cast<int>(channel_id) << " not found in allocations");
        return;
    }

    // Free words from appropriate pool (DMA channels have 0 words, so this is safe)
    freeWords(alloc->words, is_tx);

    FL_LOG_RMT("RMT " << (is_tx ? "TX" : "RX") << " channel "
               << static_cast<int>(channel_id) << " freed: " << alloc->words << " words"
               << (alloc->is_dma ? " (DMA)" : ""));

    // Remove from allocations vector
    for (auto it = mLedger.allocations.begin(); it != mLedger.allocations.end(); ++it) {
        if (it->channel_id == channel_id && it->is_tx == is_tx) {
            mLedger.allocations.erase(it);
            break;
        }
    }
}

size_t RmtMemoryManager::availableTxWords() const {
    return getAvailableWords(true);
}

size_t RmtMemoryManager::availableRxWords() const {
    return getAvailableWords(false);
}

bool RmtMemoryManager::canAllocateTx(bool use_dma, bool networkActive) const {
    if (use_dma) {
        return true;  // DMA always succeeds (bypasses on-chip memory)
    }
    size_t mem_blocks = calculateMemoryBlocks(networkActive);
    size_t words_needed = mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;
    return words_needed <= getAvailableWords(true);
}

bool RmtMemoryManager::canAllocateRx(size_t symbols) const {
    return symbols <= getAvailableWords(false);
}

size_t RmtMemoryManager::getAllocatedWords(uint8_t channel_id, bool is_tx) const {
    const ChannelAllocation* alloc = findAllocation(channel_id, is_tx);
    return alloc ? alloc->words : 0;
}

void RmtMemoryManager::reset() {
    FL_WARN("RMT Memory Manager reset - clearing all allocations");

    // Reset appropriate fields based on pool architecture
    if (mLedger.is_global_pool) {
        mLedger.allocated_words = 0;
    } else {
        mLedger.allocated_tx_words = 0;
        mLedger.allocated_rx_words = 0;
    }

    mLedger.allocations.clear();
    mDMAAllocation.allocated = false;
    mDMAAllocation.channel_id = 0;
    mDMAAllocation.is_tx = false;
}

// ============================================================================
// DMA Channel Management
// ============================================================================

bool RmtMemoryManager::isDMAAvailable() const {
#if FASTLED_RMT5_DMA_SUPPORTED
    return !mDMAAllocation.allocated;
#else
    return false;  // No DMA support on this platform
#endif
}

bool RmtMemoryManager::allocateDMA(uint8_t channel_id, bool is_tx) {
#if FASTLED_RMT5_DMA_SUPPORTED
    if (mDMAAllocation.allocated) {
        FL_WARN("DMA allocation failed: DMA already allocated to "
                << (mDMAAllocation.is_tx ? "TX" : "RX") << " channel "
                << static_cast<int>(mDMAAllocation.channel_id));
        return false;
    }

    mDMAAllocation.channel_id = channel_id;
    mDMAAllocation.is_tx = is_tx;
    mDMAAllocation.allocated = true;

    FL_LOG_RMT("DMA allocated to " << (is_tx ? "TX" : "RX") << " channel "
               << static_cast<int>(channel_id)
               << " | DMA slots: 1/" << FASTLED_RMT5_MAX_DMA_CHANNELS);
    return true;
#else
    (void)channel_id;
    (void)is_tx;
    return false;  // No DMA support on this platform
#endif
}

void RmtMemoryManager::freeDMA(uint8_t channel_id, bool is_tx) {
#if FASTLED_RMT5_DMA_SUPPORTED
    if (!mDMAAllocation.allocated) {
        FL_WARN("DMA free called but no DMA allocated");
        return;
    }

    if (mDMAAllocation.channel_id != channel_id || mDMAAllocation.is_tx != is_tx) {
        FL_WARN("DMA free mismatch: expected "
                << (mDMAAllocation.is_tx ? "TX" : "RX") << " channel "
                << static_cast<int>(mDMAAllocation.channel_id)
                << ", got " << (is_tx ? "TX" : "RX") << " channel "
                << static_cast<int>(channel_id));
        return;
    }

    FL_LOG_RMT("DMA freed from " << (is_tx ? "TX" : "RX") << " channel "
               << static_cast<int>(channel_id)
               << " | DMA slots: 0/" << FASTLED_RMT5_MAX_DMA_CHANNELS);

    mDMAAllocation.allocated = false;
    mDMAAllocation.channel_id = 0;
    mDMAAllocation.is_tx = false;
#else
    (void)channel_id;
    (void)is_tx;
#endif
}

int RmtMemoryManager::getDMAChannelsInUse() const {
#if FASTLED_RMT5_DMA_SUPPORTED
    return mDMAAllocation.allocated ? 1 : 0;
#else
    return 0;
#endif
}

// ============================================================================
// Private Helper Methods
// ============================================================================

RmtMemoryManager::ChannelAllocation*
RmtMemoryManager::findAllocation(uint8_t channel_id, bool is_tx) {
    for (auto& alloc : mLedger.allocations) {
        if (alloc.channel_id == channel_id && alloc.is_tx == is_tx) {
            return &alloc;
        }
    }
    return nullptr;
}

const RmtMemoryManager::ChannelAllocation*
RmtMemoryManager::findAllocation(uint8_t channel_id, bool is_tx) const {
    for (const auto& alloc : mLedger.allocations) {
        if (alloc.channel_id == channel_id && alloc.is_tx == is_tx) {
            return &alloc;
        }
    }
    return nullptr;
}

// ============================================================================
// External Memory Reservation API
// ============================================================================

void RmtMemoryManager::reserveExternalMemory(size_t tx_words, size_t rx_words) {
    mLedger.reserved_tx_words = tx_words;
    mLedger.reserved_rx_words = rx_words;

    if (mLedger.is_global_pool) {
        size_t total_reserved = tx_words + rx_words;
        FL_DBG("RMT External Reservation (GLOBAL pool): " << total_reserved
               << " words (TX:" << tx_words << " + RX:" << rx_words << ")");
        FL_DBG("  Available after reservation: "
               << (mLedger.total_words > total_reserved ? mLedger.total_words - total_reserved : 0)
               << "/" << mLedger.total_words << " words");
    } else {
        FL_DBG("RMT External Reservation (DEDICATED pools):");
        FL_DBG("  TX: " << tx_words << " words reserved, "
               << (mLedger.total_tx_words > tx_words ? mLedger.total_tx_words - tx_words : 0)
               << "/" << mLedger.total_tx_words << " available");
        FL_DBG("  RX: " << rx_words << " words reserved, "
               << (mLedger.total_rx_words > rx_words ? mLedger.total_rx_words - rx_words : 0)
               << "/" << mLedger.total_rx_words << " available");
    }
}

void RmtMemoryManager::getReservedMemory(size_t& tx_words, size_t& rx_words) const {
    tx_words = mLedger.reserved_tx_words;
    rx_words = mLedger.reserved_rx_words;
}

// ============================================================================
// Helper Methods
// ============================================================================

size_t RmtMemoryManager::getAvailableWords(bool is_tx) const {
    if (mLedger.is_global_pool) {
        // Global pool: return total available (shared with TX/RX, minus reservations)
        size_t total_reserved = mLedger.reserved_tx_words + mLedger.reserved_rx_words;
        size_t total_available = (mLedger.total_words > total_reserved) ? (mLedger.total_words - total_reserved) : 0;
        return (total_available > mLedger.allocated_words) ? (total_available - mLedger.allocated_words) : 0;
    } else {
        // Dedicated pools: return pool-specific available (minus reservations)
        if (is_tx) {
            size_t available_tx = (mLedger.total_tx_words > mLedger.reserved_tx_words) ?
                                  (mLedger.total_tx_words - mLedger.reserved_tx_words) : 0;
            return (available_tx > mLedger.allocated_tx_words) ? (available_tx - mLedger.allocated_tx_words) : 0;
        } else {
            size_t available_rx = (mLedger.total_rx_words > mLedger.reserved_rx_words) ?
                                  (mLedger.total_rx_words - mLedger.reserved_rx_words) : 0;
            return (available_rx > mLedger.allocated_rx_words) ? (available_rx - mLedger.allocated_rx_words) : 0;
        }
    }
}

bool RmtMemoryManager::tryAllocateWords(size_t words_needed, bool is_tx) {
    if (words_needed > getAvailableWords(is_tx)) {
        return false;
    }

    if (mLedger.is_global_pool) {
        mLedger.allocated_words += words_needed;
    } else {
        if (is_tx) {
            mLedger.allocated_tx_words += words_needed;
        } else {
            mLedger.allocated_rx_words += words_needed;
        }
    }

    return true;
}

void RmtMemoryManager::freeWords(size_t words, bool is_tx) {
    if (mLedger.is_global_pool) {
        mLedger.allocated_words -= words;
    } else {
        if (is_tx) {
            mLedger.allocated_tx_words -= words;
        } else {
            mLedger.allocated_rx_words -= words;
        }
    }
}

} // namespace fl

#endif // ESP32 && FASTLED_RMT5
