// IWYU pragma: private

/// @file rmt_memory_manager.cpp
/// @brief RMT memory allocation manager implementation

#include "platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.h"

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32) && FASTLED_RMT5

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/common.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
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
void RmtMemoryManager::initPlatformLimits(size_t& total_tx, size_t& total_rx) FL_NOEXCEPT {
#if defined(FL_IS_ESP_32DEV)
    // ESP32: 8 flexible channels × 64 words = 512 words SHARED global pool
    total_tx = 8 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 512 words (global pool)
    total_rx = 0;  // Not used for global pool
    FL_DBG("RMT Memory (ESP32): " << total_tx << " words GLOBAL POOL (shared TX/RX)");

#elif defined(FL_IS_ESP_32S2)
    // ESP32-S2: 4 flexible channels × 64 words = 256 words SHARED global pool
    total_tx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 256 words (global pool)
    total_rx = 0;  // Not used for global pool
    FL_DBG("RMT Memory (ESP32-S2): " << total_tx << " words GLOBAL POOL (shared TX/RX)");

#elif defined(FL_IS_ESP_32S3)
    // ESP32-S3: 4 dedicated TX + 4 dedicated RX channels × 48 words
    total_tx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 192 words (dedicated TX pool)
    total_rx = 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // 192 words (dedicated RX pool)
    FL_DBG("RMT Memory (ESP32-S3): TX=" << total_tx << " words, RX=" << total_rx << " words (DEDICATED pools)");

#elif defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C6) || \
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

RmtMemoryManager::MemoryLedger::MemoryLedger() FL_NOEXCEPT
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

#if defined(FL_IS_ESP_32DEV) || defined(FL_IS_ESP_32S2)
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

RmtMemoryManager::RmtMemoryManager() FL_NOEXCEPT
    : mLedger()
    , mDMAAllocation{0, false, false}
    , mIdleBlocks(FASTLED_RMT_MEM_BLOCKS)
    , mNetworkBlocks(FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE) {
}

// Test-only constructor
RmtMemoryManager::RmtMemoryManager(size_t total_tx, size_t total_rx, bool is_global) FL_NOEXCEPT
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

RmtMemoryManager& RmtMemoryManager::instance() FL_NOEXCEPT {
    static RmtMemoryManager instance;
    return instance;
}

size_t RmtMemoryManager::getPlatformTxWords() FL_NOEXCEPT {
    size_t tx_limit = 0, rx_limit = 0;
    initPlatformLimits(tx_limit, rx_limit);
    return tx_limit;
}

size_t RmtMemoryManager::getPlatformRxWords() FL_NOEXCEPT {
    size_t tx_limit = 0, rx_limit = 0;
    initPlatformLimits(tx_limit, rx_limit);
    return rx_limit;
}

bool RmtMemoryManager::isPlatformGlobalPool() FL_NOEXCEPT {
#if defined(FL_IS_ESP_32DEV) || defined(FL_IS_ESP_32S2)
    return true;  // Global pool platforms
#else
    return false;  // Dedicated pool platforms
#endif
}

size_t RmtMemoryManager::calculateMemoryBlocks(bool networkActive) FL_NOEXCEPT {
#if FASTLED_RMT_STATIC_ALLOCATION
    // Static-allocation mode: skip the runtime planner entirely.
    // User has asserted single fixed strip / no network / boot-time init.
    // See #2773 item 2.5.
    (void)networkActive;
    return FASTLED_RMT_MEM_BLOCKS;
#else
    // Read memory block strategy from singleton instance
    auto& mgr = instance();
    size_t idleBlocks = mgr.mIdleBlocks;
    size_t networkBlocks = mgr.mNetworkBlocks;

    // Calculate platform maximum blocks based on available TX channels
    size_t max_blocks = SOC_RMT_TX_CANDIDATES_PER_GROUP;

    // Select strategy based on network activity
    size_t requested_blocks = networkActive ? networkBlocks : idleBlocks;

    // ITERATION 3 FIX: Adaptive allocation based on available TX memory
    // Problem: ESP32-S3 has only 192 words total TX memory (4 × 48-word blocks)
    // With 2 blocks/channel (96 words), only 2 channels fit before exhaustion.
    // Solution: Detect memory pressure and reduce to 1 block/channel when needed.
    //
    // Memory pressure detection:
    // - Count already-allocated TX channels (not RX, which uses separate pool)
    // - Calculate remaining TX memory after this allocation
    // - If insufficient to fit min(SOC_RMT_TX_CANDIDATES_PER_GROUP, 4) channels
    //   at the requested rate, switch to single-buffering. The clamp at 4
    //   avoids unnecessary fallback on classic ESP32/S2 (8 TX channels) while
    //   still triggering correctly on S3/C3/C6/H2 where #1873 lives.
    //
    // Example ESP32-S3 (192 words total):
    // - Channel 0: 96 words (2 blocks) → 96 remaining → only 2 channels fit ✗
    // - With adaptive: Channel 0: 48 words → 144 remaining → 4 channels fit ✓
    size_t total_memory = mgr.mLedger.is_global_pool ? mgr.mLedger.total_words : mgr.mLedger.total_tx_words;
    size_t allocated_memory = mgr.mLedger.is_global_pool ? mgr.mLedger.allocated_words : mgr.mLedger.allocated_tx_words;
    size_t available_memory = (total_memory > allocated_memory) ? (total_memory - allocated_memory) : 0;

    // Count ONLY TX channels (RX channels use separate pool on S3/C3/C6)
    size_t allocated_tx_channels = 0;
    for (const auto& alloc : mgr.mLedger.allocations) {
        if (alloc.is_tx) {
            allocated_tx_channels++;
        }
    }

    // Adaptive logic: Maximize TX channel count by detecting memory constraints
    // Platform constraints:
    //   ESP32-S3: 192 words TX memory = 4 channels × 48 words (single-buffer)
    //                                or 2 channels × 96 words (double-buffer)
    //   ESP32-C3/C6/H2: 96 words TX memory = 2 channels × 48 words (single-buffer ONLY)
    //                                      (cannot fit 2 channels with double-buffer)
    //
    // Strategy: Use single-buffering (1 block) when needed to maximize channel count.
    // Threshold: If available memory can't fit SOC_RMT_TX_CANDIDATES_PER_GROUP channels
    //            at the requested rate, fall back to 1 block per channel.
    //
    // CRITICAL for ESP32-C3/C6/H2 (GitHub issue #1873):
    //   These platforms have only 2 TX channels with 96 words shared between them.
    //   With default 2× buffering (96 words each), the FIRST channel consumes ALL
    //   TX memory, causing ESP-IDF to fail on second rmt_new_tx_channel() with
    //   ESP_ERR_NOT_FOUND ("no free tx channels"). The adaptive logic below detects
    //   this memory pressure and switches to 1× buffering, enabling two strips to
    //   coexist. Combined with the worker-pool in ChannelEngineRMT, more than 2
    //   strips can be serialized through the same HW channels.
    //
    // Example ESP32-S3 scenarios (max=4 TX channels):
    // - First TX allocation (192 available, 2 blocks requested = 96 words):
    //   192 / 96 = 2 channels → NOT enough for 4+ channels → use 1 block
    // - After 1 TX channel allocated (144 available, 2 blocks requested = 96 words):
    //   144 / 96 = 1.5 channels → NOT enough for 3+ more channels → use 1 block
    //
    // Example ESP32-C3 scenarios (max=2 TX channels):
    // - First TX allocation (96 available, 2 blocks requested = 96 words):
    //   96 / 96 = 1 channel → NOT enough for 2 channels → use 1 block (48 words)
    // - After 1 TX channel allocated (48 available, 2 blocks requested = 96 words):
    //   48 / 96 = 0 channels → NOT enough for 1 more channel → use 1 block (48 words)
    size_t words_per_block = SOC_RMT_MEM_WORDS_PER_CHANNEL;
    size_t requested_words = requested_blocks * words_per_block;

    // Calculate how many TX channels we could fit at the requested rate
    size_t channels_at_requested_rate = (requested_words > 0) ? (available_memory / requested_words) : 0;

    // Memory pressure threshold: trigger single-buffering fallback when we
    // can't fit this many TX channels at the requested rate.
    //
    // Clamped at 4 so we don't demote classic ESP32/S2 (8 TX channels,
    // 512 words) on the first allocation — there, 2× buffering fits 4 strips
    // comfortably and falling back to 1× would change observable behaviour
    // without fixing any real bug. Platforms with ≤4 TX channels
    // (ESP32-S3, C3, C6, H2) use their own SOC_RMT_TX_CANDIDATES_PER_GROUP,
    // which is what drives the #1873 fix on C3/C6/H2.
    //
    //   ESP32 / ESP32-S2:     min(8, 4) = 4 → pressure if <4 fit (unchanged)
    //   ESP32-S3:             min(4, 4) = 4 → pressure if <4 fit (unchanged)
    //   ESP32-C3 / C6 / H2:   min(2, 4) = 2 → pressure if <2 fit (fixes #1873)
    constexpr size_t kMemoryPressureThreshold =
        (SOC_RMT_TX_CANDIDATES_PER_GROUP < 4)
            ? static_cast<size_t>(SOC_RMT_TX_CANDIDATES_PER_GROUP)
            : static_cast<size_t>(4);
    bool memory_pressure =
        (requested_blocks > 1 &&
         channels_at_requested_rate < kMemoryPressureThreshold);

    if (memory_pressure) {
        FL_LOG_RMT("Adaptive RMT allocation: Memory pressure detected");
        FL_LOG_RMT("  Total TX: " << total_memory << " words, Allocated TX: " << allocated_memory
                << " words (" << allocated_tx_channels << " TX channels)");
        FL_LOG_RMT("  Available: " << available_memory << " words");
        FL_LOG_RMT("  Requested: " << requested_blocks << " blocks (" << requested_words << " words)");
        FL_LOG_RMT("  → At this rate, only " << channels_at_requested_rate << " TX channel(s) would fit");
        FL_LOG_RMT("  → Reducing to 1 block (" << words_per_block << " words) for better channel density");
        requested_blocks = 1;
    }

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
           << ", network=" << networkBlocks << ", max=" << max_blocks
           << ", allocated_tx_channels=" << allocated_tx_channels << ")");

    return requested_blocks;
#endif // FASTLED_RMT_STATIC_ALLOCATION
}

void RmtMemoryManager::setMemoryBlockStrategy(size_t idleBlocks, size_t networkBlocks) FL_NOEXCEPT {
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

void RmtMemoryManager::getMemoryBlockStrategy(size_t& idleBlocks, size_t& networkBlocks) const FL_NOEXCEPT {
    // Read strategy configuration
    idleBlocks = mIdleBlocks;
    networkBlocks = mNetworkBlocks;
}

result<size_t, RmtMemoryError> RmtMemoryManager::allocateTx(u8 channel_id, bool use_dma, bool networkActive) FL_NOEXCEPT {
    // Check if channel already allocated
    if (findAllocation(channel_id, true) != nullptr) {
        FL_WARN("RMT TX channel " << static_cast<int>(channel_id) << " already allocated");
        return result<size_t, RmtMemoryError>::failure(RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
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
#if defined(FL_IS_ESP_32S3)
        // ESP32-S3: DMA channel consumes 1 memory block (48 words)
        size_t dma_words = SOC_RMT_MEM_WORDS_PER_CHANNEL;
        if (!tryAllocateWords(dma_words, true)) {
            FL_WARN("RMT TX DMA allocation failed for channel "
                    << static_cast<int>(channel_id)
                    << " - insufficient on-chip memory");
            FL_WARN("  Requested: " << dma_words << " words (1 block for DMA descriptor)");
            FL_WARN("  Available: " << getAvailableWords(true) << " words");
            return result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_TX_MEMORY);
        }
        mLedger.allocations.push_back(ChannelAllocation(channel_id, dma_words, true, true));
        FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, " << dma_words << " words for descriptor)");
        return result<size_t, RmtMemoryError>::success(dma_words);
#else
        // Other platforms (if DMA supported): Assume DMA bypasses on-chip memory
        mLedger.allocations.push_back(ChannelAllocation(channel_id, 0, true, true));
        FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, bypasses on-chip memory)");
        return result<size_t, RmtMemoryError>::success(0);
#endif
    }

    // Calculate adaptive buffer size based on network state
    size_t mem_blocks = calculateMemoryBlocks(networkActive);
    size_t words_needed = mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;

    // Try to allocate from appropriate pool. The failure path (single-buffer
    // fallback + multi-line diagnostic block) is extracted to an
    // FL_NO_INLINE cold helper so the hot path here stays small. See
    // #2773 item 2.5.
    if (!tryAllocateWords(words_needed, true)) {
        return handleAllocateTxFailure(channel_id, mem_blocks, words_needed,
                                       networkActive);
    }

    mLedger.allocations.push_back(ChannelAllocation(channel_id, words_needed, true, false));

    FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
               << " allocated: " << words_needed << " words (" << mem_blocks << "× buffer"
               << (networkActive ? ", Network mode" : "") << ")");

    return result<size_t, RmtMemoryError>::success(words_needed);
}

FL_NO_INLINE result<size_t, RmtMemoryError>
RmtMemoryManager::handleAllocateTxFailure(u8 channel_id, size_t mem_blocks,
                                          size_t words_needed,
                                          bool networkActive) FL_NOEXCEPT {
    // ITERATION 2 FIX: Progressive fallback for multi-channel scenarios.
    // When double-buffering (2 blocks) fails, try single-buffering (1 block)
    // so more channels can coexist on memory-constrained platforms like
    // ESP32-S3 (192 words total = 4 × 48-word blocks).
    //
    // Example: ESP32-S3 with 8 lanes:
    // - 2 blocks/channel: 2 × 96 = 192 words → only 2 channels fit
    // - 1 block/channel: 1 × 48 = 48 words → 4 channels fit
    //
    // Trade-off: Single-buffering reduces performance slightly but enables
    // multi-lane validation.
    if (mem_blocks > 1) {
        size_t fallback_blocks = 1;
        size_t fallback_words = fallback_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;

        FL_LOG_RMT("RMT TX allocation failed with " << mem_blocks << "× buffering (" << words_needed << " words)");
        FL_LOG_RMT("  Attempting fallback to " << fallback_blocks << "× buffering (" << fallback_words << " words)...");

        if (tryAllocateWords(fallback_words, true)) {
            FL_LOG_RMT("  [OK] Fallback successful: allocated " << fallback_words << " words (single-buffer mode)");
            mLedger.allocations.push_back(ChannelAllocation(channel_id, fallback_words, true, false));
            FL_LOG_RMT("RMT TX channel " << static_cast<int>(channel_id)
                       << " allocated (non-DMA, " << fallback_words << " words, single-buffer)");
            return result<size_t, RmtMemoryError>::success(fallback_words);
        }

        FL_LOG_RMT("  [FAIL] Fallback failed: insufficient memory even for single-buffer");
    }

    // Fallback failed or not attempted - provide detailed diagnostic message.
    // Gate the entire block on FASTLED_LOG_RUNTIME_ENABLED — in release
    // builds (NDEBUG → FASTLED_LOG_VERBOSITY=0 per Stage 1) the 9 FL_WARN
    // bodies collapse to do-while-0, but the locals they feed (including
    // the getAvailableWords() call + 3 mLedger reads) still run. See #2956.
#if FASTLED_LOG_RUNTIME_ENABLED
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
#endif

    return result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_TX_MEMORY);
}

result<size_t, RmtMemoryError> RmtMemoryManager::allocateRx(u8 channel_id, size_t symbols, bool use_dma) FL_NOEXCEPT {
    // Check if channel already allocated
    if (findAllocation(channel_id, false) != nullptr) {
        FL_WARN("RMT RX channel " << static_cast<int>(channel_id) << " already allocated");
        return result<size_t, RmtMemoryError>::failure(RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
    }

    // DMA channels bypass on-chip memory (use DRAM instead)
    if (use_dma) {
        mLedger.allocations.push_back(ChannelAllocation(channel_id, 0, false, true));
        FL_LOG_RMT("RMT RX channel " << static_cast<int>(channel_id)
                   << " allocated (DMA, bypasses on-chip memory, uses DRAM buffer)");
        return result<size_t, RmtMemoryError>::success(0);
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

        return result<size_t, RmtMemoryError>::failure(RmtMemoryError::INSUFFICIENT_RX_MEMORY);
    }

    mLedger.allocations.push_back(ChannelAllocation(channel_id, words_needed, false, false));

    FL_LOG_RMT("RMT RX channel " << static_cast<int>(channel_id)
               << " allocated: " << words_needed << " words (" << symbols << " symbols)");

    return result<size_t, RmtMemoryError>::success(words_needed);
}

bool RmtMemoryManager::tryAllocateTx(u8 channel_id, bool use_dma, bool networkActive,
                                      size_t& out_words) FL_NOEXCEPT {
    auto r = allocateTx(channel_id, use_dma, networkActive);
    if (r.ok()) {
        out_words = r.value();
        return true;
    }
    return false;
}

bool RmtMemoryManager::tryAllocateRx(u8 channel_id, size_t symbols, bool use_dma,
                                      size_t& out_words) FL_NOEXCEPT {
    auto r = allocateRx(channel_id, symbols, use_dma);
    if (r.ok()) {
        out_words = r.value();
        return true;
    }
    return false;
}

void RmtMemoryManager::free(u8 channel_id, bool is_tx) FL_NOEXCEPT {
#if FASTLED_RMT_STATIC_ALLOCATION
    // Static-allocation mode: the user has asserted no removeLeds() / late
    // addLeds(). The destructor still calls free() at process end but the
    // ledger isn't meaningfully tracked, so erasing from the allocations
    // vector + emitting the FL_LOG_RMT diagnostic is dead work. Skipping it
    // lets the linker drop fl::vector_inlined::erase + the diagnostic
    // operator<< chain. See #2856 item 3.6.
    (void)channel_id;
    (void)is_tx;
#else
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
#endif
}

void RmtMemoryManager::recordRecoveryAllocation(u8 channel_id, size_t words, bool is_tx) FL_NOEXCEPT {
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

size_t RmtMemoryManager::availableTxWords() const FL_NOEXCEPT {
    return getAvailableWords(true);
}

size_t RmtMemoryManager::availableRxWords() const FL_NOEXCEPT {
    return getAvailableWords(false);
}

bool RmtMemoryManager::canAllocateTx(bool use_dma, bool networkActive) const FL_NOEXCEPT {
    if (use_dma) {
        return true;  // DMA always succeeds (bypasses on-chip memory)
    }
    size_t mem_blocks = calculateMemoryBlocks(networkActive);
    size_t words_needed = mem_blocks * SOC_RMT_MEM_WORDS_PER_CHANNEL;
    return words_needed <= getAvailableWords(true);
}

bool RmtMemoryManager::canAllocateRx(size_t symbols) const FL_NOEXCEPT {
    return symbols <= getAvailableWords(false);
}

size_t RmtMemoryManager::getAllocatedWords(u8 channel_id, bool is_tx) const FL_NOEXCEPT {
    const ChannelAllocation* alloc = findAllocation(channel_id, is_tx);
    return alloc ? alloc->words : 0;
}

void RmtMemoryManager::reset() FL_NOEXCEPT {
    FL_LOG_RMT("RMT Memory Manager reset - clearing all allocations");

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

size_t RmtMemoryManager::getTotalTxWords() const FL_NOEXCEPT {
    if (mLedger.is_global_pool) {
        return mLedger.total_words;  // Global pool total
    } else {
        return mLedger.total_tx_words;  // Dedicated TX pool
    }
}

size_t RmtMemoryManager::getTotalRxWords() const FL_NOEXCEPT {
    if (mLedger.is_global_pool) {
        return 0;  // Global pool doesn't have separate RX total
    } else {
        return mLedger.total_rx_words;  // Dedicated RX pool
    }
}

size_t RmtMemoryManager::getAllocatedTxWords() const FL_NOEXCEPT {
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

size_t RmtMemoryManager::getAllocatedRxWords() const FL_NOEXCEPT {
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

bool RmtMemoryManager::hasActiveRxChannels() const FL_NOEXCEPT {
    // Check if any RX allocations exist
    for (const auto& alloc : mLedger.allocations) {
        if (!alloc.is_tx) {
            return true;  // Found an RX allocation
        }
    }
    return false;
}

size_t RmtMemoryManager::getAllocationCount() const FL_NOEXCEPT {
    return mLedger.allocations.size();
}

bool RmtMemoryManager::isGlobalPool() const FL_NOEXCEPT {
    return mLedger.is_global_pool;
}

// ============================================================================
// DMA Channel Management
// ============================================================================

bool RmtMemoryManager::isDMAAvailable() const FL_NOEXCEPT {
#if FASTLED_RMT5_DMA_SUPPORTED
    if (mDMAAllocation.allocated) {
        return false;  // DMA slot already in use
    }
#if defined(FL_IS_ESP_32S3)
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

bool RmtMemoryManager::allocateDMA(u8 channel_id, bool is_tx) FL_NOEXCEPT {
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

void RmtMemoryManager::freeDMA(u8 channel_id, bool is_tx) FL_NOEXCEPT {
#if FASTLED_RMT_STATIC_ALLOCATION
    // Static-allocation mode: see RmtMemoryManager::free() for rationale.
    // The destructor still reaches this; skipping the mismatch-diagnostic
    // FL_WARN sites lets the linker drop the operator<< chains. See #2856
    // item 3.6.
    (void)channel_id;
    (void)is_tx;
#elif FASTLED_RMT5_DMA_SUPPORTED
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

int RmtMemoryManager::getDMAChannelsInUse() const FL_NOEXCEPT {
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
RmtMemoryManager::findAllocation(u8 channel_id, bool is_tx) FL_NOEXCEPT {
    for (auto& alloc : mLedger.allocations) {
        if (alloc.channel_id == channel_id && alloc.is_tx == is_tx) {
            return &alloc;
        }
    }
    return nullptr;
}

const RmtMemoryManager::ChannelAllocation*
RmtMemoryManager::findAllocation(u8 channel_id, bool is_tx) const FL_NOEXCEPT {
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

void RmtMemoryManager::reserveExternalMemory(size_t tx_words, size_t rx_words) FL_NOEXCEPT {
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

void RmtMemoryManager::getReservedMemory(size_t& tx_words, size_t& rx_words) const FL_NOEXCEPT {
    tx_words = mLedger.reserved_tx_words;
    rx_words = mLedger.reserved_rx_words;
}

// ============================================================================
// Helper Methods
// ============================================================================

size_t RmtMemoryManager::getAvailableWords(bool is_tx) const FL_NOEXCEPT {
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

bool RmtMemoryManager::tryAllocateWords(size_t words_needed, bool is_tx) FL_NOEXCEPT {
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

void RmtMemoryManager::freeWords(size_t words, bool is_tx) FL_NOEXCEPT {
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

#endif // FL_IS_ESP32 && FASTLED_RMT5
