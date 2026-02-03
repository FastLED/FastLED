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
    , mDMAAllocation{0, false, false}
    , mIdleBlocks(FASTLED_RMT_MEM_BLOCKS)
    , mNetworkBlocks(FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE) {
}

// Test-only constructor
RmtMemoryManager::RmtMemoryManager(size_t total_tx, size_t total_rx, bool is_global)
    : mLedger()
    , mDMAAllocation{0, false, false}
    , mIdleBlocks(2)
    , mNetworkBlocks(3) {

    mLedger.is_global_pool = is_global;

    if (is_global) {
        // Global pool mode (ESP32, ESP32-S2)
        mLedger.total_words = total_tx;  // total_tx holds global pool size
        mLedger.allocated_words = 0;
        mLedger.total_tx_words = 0;
        mLedger.total_rx_words = 0;
        FL_DBG("RMT Memory Manager (TEST): " << total_tx << " words GLOBAL POOL");
    } else {
        // Dedicated pool mode (ESP32-S3, C3, C6, H2)
        mLedger.total_tx_words = total_tx;
        mLedger.total_rx_words = total_rx;
        mLedger.allocated_tx_words = 0;
        mLedger.allocated_rx_words = 0;
        mLedger.total_words = 0;
        FL_DBG("RMT Memory Manager (TEST): TX=" << total_tx << " words, RX=" << total_rx << " words (DEDICATED pools)");
    }
}

RmtMemoryManager& RmtMemoryManager::instance() {
    static RmtMemoryManager instance;
    return instance;
}

size_t RmtMemoryManager::getPlatformTxWords() {
    size_t tx_limit = 0, rx_limit = 0;
    initPlatformLimits(tx_limit, rx_limit);
    return tx_limit;
}

size_t RmtMemoryManager::getPlatformRxWords() {
    size_t tx_limit = 0, rx_limit = 0;
    initPlatformLimits(tx_limit, rx_limit);
    return rx_limit;
}

bool RmtMemoryManager::isPlatformGlobalPool() {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
    return true;  // Global pool platforms
#else
    return false;  // Dedicated pool platforms
#endif
}

size_t RmtMemoryManager::calculateMemoryBlocks(bool networkActive) {
    // Read memory block strategy from singleton instance
    auto& mgr = instance();
    size_t idleBlocks = mgr.mIdleBlocks;
    size_t networkBlocks = mgr.mNetworkBlocks;

    // Calculate platform maximum blocks based on available TX channels
    size_t max_blocks = SOC_RMT_TX_CANDIDATES_PER_GROUP;

    // Select strategy based on network activity
    size_t requested_blocks = networkActive ? networkBlocks : idleBlocks;

    // Platform constraint enforcement
#if SOC_RMT_TX_CANDIDATES_PER_GROUP < 3
    // Insufficient TX memory for triple-buffering (C3/C6/H2/C5: 2 TX channels)
    // Force cap to 2 blocks regardless of strategy
    if (requested_blocks > 2) {
        FL_WARN("Platform limited to 2× buffering (SOC_RMT_TX_CANDIDATES_PER_GROUP="
                << SOC_RMT_TX_CANDIDATES_PER_GROUP << "), capping from "
                << requested_blocks << " to 2 blocks");
        requested_blocks = 2;
    }
#else
    // Sufficient TX memory for network-aware allocation
    // Validate requested blocks do not exceed platform maximum
    if (requested_blocks > max_blocks) {
        FL_WARN("Requested " << requested_blocks << " blocks exceeds platform max "
                << max_blocks << " (SOC_RMT_TX_CANDIDATES_PER_GROUP), capping to "
                << max_blocks);
        requested_blocks = max_blocks;
    }
#endif

    // Ensure minimum 1 block
    if (requested_blocks == 0) {
        FL_WARN("Zero blocks requested, clamping to minimum 1 block");
        requested_blocks = 1;
    }

    FL_DBG("calculateMemoryBlocks(networkActive=" << networkActive
           << "): using " << requested_blocks << " blocks (idle=" << idleBlocks
           << ", network=" << networkBlocks << ", max=" << max_blocks << ")");

    return requested_blocks;
}

void RmtMemoryManager::setMemoryBlockStrategy(size_t idleBlocks, size_t networkBlocks) {
    // Calculate platform maximum blocks based on available TX memory
    // max_blocks = total_TX_words / words_per_channel
    size_t max_blocks = SOC_RMT_TX_CANDIDATES_PER_GROUP;

    // Zero-block validation: clamp to minimum 1 block
    if (idleBlocks == 0) {
        FL_WARN("RMT setMemoryBlockStrategy: idleBlocks=0 invalid, clamping to 1");
        idleBlocks = 1;
    }
    if (networkBlocks == 0) {
        FL_WARN("RMT setMemoryBlockStrategy: networkBlocks=0 invalid, clamping to 1");
        networkBlocks = 1;
    }

    // Platform limit validation: cap to max_blocks
    if (idleBlocks > max_blocks) {
        FL_WARN("RMT setMemoryBlockStrategy: idleBlocks=" << idleBlocks
                << " exceeds platform limit=" << max_blocks << ", capping");
        idleBlocks = max_blocks;
    }
    if (networkBlocks > max_blocks) {
        FL_WARN("RMT setMemoryBlockStrategy: networkBlocks=" << networkBlocks
                << " exceeds platform limit=" << max_blocks << ", capping");
        networkBlocks = max_blocks;
    }

    // Update strategy configuration
    mIdleBlocks = idleBlocks;
    mNetworkBlocks = networkBlocks;

    FL_DBG("RMT Memory Strategy updated: idle=" << idleBlocks << "×, network=" << networkBlocks << "×");
}

void RmtMemoryManager::getMemoryBlockStrategy(size_t& idleBlocks, size_t& networkBlocks) const {
    // Read strategy configuration
    idleBlocks = mIdleBlocks;
    networkBlocks = mNetworkBlocks;
}

Result<size_t, RmtMemoryError> RmtMemoryManager::allocateTx(uint8_t channel_id, bool use_dma, bool networkActive) {
    // Check if channel already allocated
    if (findAllocation(channel_id, true) != nullptr) {
        FL_WARN("RMT TX channel " << static_cast<int>(channel_id) << " already allocated");
        return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
    }

    // DMA channels on ESP32-S3 still consume one memory block from on-chip RMT memory
    // Fix for issue #2156: The DMA controller needs a memory block for its descriptor/control.
    // Previous code assumed DMA bypasses on-chip memory (0 words), but ESP32-S3 hardware
    // requires one block (48 words) for DMA channel operation.
    //
    // Reference: https://github.com/FastLED/FastLED/issues/2156
    // Related ESP-IDF issues:
    // - https://github.com/espressif/esp-idf/issues/12564
    // - https://github.com/espressif/idf-extra-components/issues/466
    if (use_dma) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // ESP32-S3: DMA channel consumes 1 memory block (48 words)
        size_t dma_words = SOC_RMT_MEM_WORDS_PER_CHANNEL;
        if (!tryAllocateWords(dma_words, true)) {
            FL_WARN("RMT TX DMA allocation failed for channel "
                    << static_cast<int>(channel_id)
                    << " - insufficient on-chip memory");
            FL_WARN("  Requested: " << dma_words << " words (1 block for DMA descriptor)");
            FL_WARN("  Available: " << getAvailableWords(true) << " words");
            return Result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_TX_MEMORY);
        }
        mLedger.allocations.push_back(ChannelAllocation(channel_id, dma_words, true, true));
        FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, " << dma_words << " words for descriptor)");
        return Result<size_t, RmtMemoryError>::success(dma_words);
#else
        // Other platforms (if DMA supported): Assume DMA bypasses on-chip memory
        mLedger.allocations.push_back(ChannelAllocation(channel_id, 0, true, true));
        FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, bypasses on-chip memory)");
        return Result<size_t, RmtMemoryError>::success(0);
#endif
    }

    // Calculate adaptive buffer size based on network state
    size_t mem_blocks = calculateMemoryBlocks(networkActive);
    size_t words_needed = mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;

    // Try to allocate from appropriate pool
    if (!tryAllocateWords(words_needed, true)) {
        // Calculate detailed memory breakdown for diagnostic message
        size_t total = mLedger.is_global_pool ? mLedger.total_words : mLedger.total_tx_words;
        size_t allocated = mLedger.is_global_pool ? mLedger.allocated_words : mLedger.allocated_tx_words;
        size_t reserved = mLedger.reserved_tx_words;
        size_t available = getAvailableWords(true);

        FL_WARN("RMT TX allocation failed for channel " << static_cast<int>(channel_id));
        FL_WARN("  Requested: " << words_needed << " words (" << mem_blocks << "× buffer"
                << (networkActive ? ", Network mode" : "") << ")");
        FL_WARN("  Available: " << available << " words");
        FL_WARN("  Memory breakdown: Total=" << total << ", Allocated=" << allocated
                << ", Reserved=" << reserved << " (external RMT usage)");

        // Provide actionable suggestions based on the failure scenario
        if (reserved > 0) {
            FL_WARN("  Suggestion: " << reserved << " words reserved by external RMT usage (e.g., USB CDC)");
            FL_WARN("              Consider using DMA channels (use_dma=true) to bypass on-chip memory");
        }
        if (allocated > 0) {
            FL_WARN("  Suggestion: " << allocated << " words already allocated to other channels");
            FL_WARN("              Consider reducing LED count or using fewer channels");
        }
        if (mem_blocks > 2 && networkActive) {
            FL_WARN("  Suggestion: Network mode uses 3× buffering (" << (mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL)
                    << " words per channel)");
            FL_WARN("              Consider disabling network or using DMA channels");
        }

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
        // Calculate detailed memory breakdown for diagnostic message
        size_t total = mLedger.is_global_pool ? mLedger.total_words : mLedger.total_rx_words;
        size_t allocated = mLedger.is_global_pool ? mLedger.allocated_words : mLedger.allocated_rx_words;
        size_t reserved = mLedger.reserved_rx_words;
        size_t available = getAvailableWords(false);

        FL_WARN("RMT RX allocation failed for channel " << static_cast<int>(channel_id));
        FL_WARN("  Requested: " << words_needed << " words (" << symbols << " symbols)");
        FL_WARN("  Available: " << available << " words");
        FL_WARN("  Memory breakdown: Total=" << total << ", Allocated=" << allocated
                << ", Reserved=" << reserved << " (external RMT usage)");

        // Provide actionable suggestions
        if (reserved > 0) {
            FL_WARN("  Suggestion: " << reserved << " words reserved by external RMT usage");
            FL_WARN("              Consider using DMA channels (use_dma=true) to bypass on-chip memory");
        }
        if (allocated > 0) {
            FL_WARN("  Suggestion: " << allocated << " words already allocated to other channels");
            FL_WARN("              Consider reducing symbol count or using fewer channels");
        }

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

void RmtMemoryManager::recordRecoveryAllocation(uint8_t channel_id, size_t words, bool is_tx) {
    // Check if already exists (shouldn't, but be safe)
    ChannelAllocation* existing = findAllocation(channel_id, is_tx);
    if (existing) {
        FL_WARN("RMT " << (is_tx ? "TX" : "RX") << " channel "
                << static_cast<int>(channel_id) << " already has allocation during recovery");
        return;
    }

    // Record the allocation in ledger
    // Note: The words are already "used" by ESP-IDF, we're just recording it
    mLedger.allocations.push_back(ChannelAllocation(channel_id, words, is_tx, false));

    // Update accounting (the memory is in use even though we didn't formally allocate it)
    if (mLedger.is_global_pool) {
        mLedger.allocated_words += words;
    } else {
        if (is_tx) {
            mLedger.allocated_tx_words += words;
        } else {
            mLedger.allocated_rx_words += words;
        }
    }

    FL_LOG_RMT("RMT " << (is_tx ? "TX" : "RX") << " channel "
               << static_cast<int>(channel_id) << " recovery allocation recorded: "
               << words << " words");
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
// State Inspection Methods
// ============================================================================

size_t RmtMemoryManager::getTotalTxWords() const {
    if (mLedger.is_global_pool) {
        return mLedger.total_words;  // Global pool total
    } else {
        return mLedger.total_tx_words;  // Dedicated TX pool
    }
}

size_t RmtMemoryManager::getTotalRxWords() const {
    if (mLedger.is_global_pool) {
        return 0;  // Global pool doesn't have separate RX total
    } else {
        return mLedger.total_rx_words;  // Dedicated RX pool
    }
}

size_t RmtMemoryManager::getAllocatedTxWords() const {
    if (mLedger.is_global_pool) {
        // For global pool, calculate TX words from allocations
        size_t tx_words = 0;
        for (const auto& alloc : mLedger.allocations) {
            if (alloc.is_tx) {
                tx_words += alloc.words;
            }
        }
        return tx_words;
    } else {
        return mLedger.allocated_tx_words;
    }
}

size_t RmtMemoryManager::getAllocatedRxWords() const {
    if (mLedger.is_global_pool) {
        // For global pool, calculate RX words from allocations
        size_t rx_words = 0;
        for (const auto& alloc : mLedger.allocations) {
            if (!alloc.is_tx) {
                rx_words += alloc.words;
            }
        }
        return rx_words;
    } else {
        return mLedger.allocated_rx_words;
    }
}

bool RmtMemoryManager::hasActiveRxChannels() const {
    // Check if any RX allocations exist
    for (const auto& alloc : mLedger.allocations) {
        if (!alloc.is_tx) {
            return true;  // Found an RX allocation
        }
    }
    return false;
}

size_t RmtMemoryManager::getAllocationCount() const {
    return mLedger.allocations.size();
}

bool RmtMemoryManager::isGlobalPool() const {
    return mLedger.is_global_pool;
}

// ============================================================================
// DMA Channel Management
// ============================================================================

bool RmtMemoryManager::isDMAAvailable() const {
#if FASTLED_RMT5_DMA_SUPPORTED
    if (mDMAAllocation.allocated) {
        return false;  // DMA slot already in use
    }
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: DMA channel still requires 1 memory block (48 words) for descriptor
    // Check if there's enough on-chip memory for DMA descriptor
    if (getAvailableWords(true) < SOC_RMT_MEM_WORDS_PER_CHANNEL) {
        return false;  // Not enough memory for DMA descriptor
    }
#endif
    return true;
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
