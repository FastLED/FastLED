/// @file rmt_memory_manager.h
/// @brief RMT memory allocation manager for ESP32 TX and RX channels
///
/// Centralized accounting ledger that tracks all RMT memory allocations (TX and RX)
/// with a strict double-buffer policy for TX channels. Prevents over-allocation and
/// coordinates memory usage between TX and RX channels.
///
/// Key Features:
/// - Double-buffer policy for TX channels (2× SOC_RMT_MEM_WORDS_PER_CHANNEL)
/// - Configurable RX buffer allocation
/// - DMA channels bypass on-chip memory accounting
/// - Fail-fast on memory exhaustion
/// - Singleton pattern for centralized coordination

#pragma once

#include "fl/compiler_control.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/result.h"

namespace fl {

/// @brief Error codes for RMT memory allocation
enum class RmtMemoryError : u8 {
    INSUFFICIENT_TX_MEMORY = 1,  ///< Not enough TX memory available
    INSUFFICIENT_RX_MEMORY = 2,  ///< Not enough RX memory available
    CHANNEL_ALREADY_ALLOCATED = 3,  ///< Channel already has an allocation
    CHANNEL_NOT_FOUND = 4,  ///< Channel not found in allocations
    INVALID_CHANNEL_ID = 5  ///< Channel ID out of range
};

/// @brief RMT Memory Manager - Centralized allocation ledger for TX and RX channels
///
/// This singleton manages all RMT on-chip memory allocations to prevent over-allocation
/// and ensure predictable behavior across TX and RX channels.
///
/// Allocation Policy:
/// - TX channels: Always double-buffer (2× SOC_RMT_MEM_WORDS_PER_CHANNEL)
/// - RX channels: User-specified symbol count
/// - DMA channels: Bypass on-chip memory (allocated from DRAM instead)
///
/// **IMPORTANT: DMA Channels Bypass On-Chip Memory**
/// ====================================================
/// When a channel uses DMA (with_dma = true), it does NOT consume RMT on-chip
/// memory. Instead, DMA uses DRAM buffers. The mem_block_symbols parameter
/// controls the DMA buffer size (not on-chip RMT memory).
///
/// This means:
/// - DMA TX/RX channels: Use DRAM, do NOT count against pools below
/// - Non-DMA channels: Use on-chip RMT memory from dedicated pools
///
/// Platform Limits - **NON-DMA ONLY**:
///
/// **Global Pool Platforms** (ESP32, ESP32-S2):
/// - ESP32: 8 flexible channels × 64 words = 512 words (SHARED global pool)
/// - ESP32-S2: 4 flexible channels × 64 words = 256 words (SHARED global pool)
/// - Any channel (TX or RX) can use memory from the global pool
///
/// **Dedicated Pool Platforms** (ESP32-S3, C3, C6, H2):
/// - ESP32-S3: 4 TX × 48 words + 4 RX × 48 words = 384 words (SEPARATE pools)
/// - ESP32-C3/C6/H2: 2 TX × 48 words + 2 RX × 48 words = 192 words (SEPARATE pools)
/// - TX channels use TX pool, RX channels use RX pool (no sharing)
///
/// Example Usage:
/// @code
/// auto& mgr = RmtMemoryManager::instance();
///
/// // Allocate TX channel (non-DMA)
/// auto tx_result = mgr.allocateTx(0, false);
/// if (tx_result.ok()) {
///     size_t words = tx_result.value();
///     FL_DBG("Allocated " << words << " words for TX channel 0");
/// }
///
/// // Allocate RX channel (1024 symbols)
/// auto rx_result = mgr.allocateRx(0, 1024);
/// if (rx_result.ok()) {
///     FL_DBG("Allocated RX channel 0");
/// }
///
/// // Free when done
/// mgr.free(0, true);  // Free TX channel 0
/// mgr.free(0, false); // Free RX channel 0
/// @endcode
class RmtMemoryManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to the global RMT memory manager
    static RmtMemoryManager& instance();

    // ========================================================================
    // Testing Support - Allows mocking platform limits for unit tests
    // ========================================================================

    /// @brief Test-only constructor - allows mocking platform limits
    /// @param total_tx Total TX memory words (for dedicated pools or global pool)
    /// @param total_rx Total RX memory words (0 for global pool platforms)
    /// @param is_global true for global pool (ESP32/S2), false for dedicated pools (S3/C3/C6/H2)
    ///
    /// This constructor is intended for unit testing ONLY. It allows tests to:
    /// - Mock different platform configurations (ESP32-S3, C3, etc.)
    /// - Test memory exhaustion scenarios with controlled limits
    /// - Verify accounting logic without hardware dependencies
    ///
    /// Example:
    /// @code
    /// // Mock ESP32-S3: 192 TX words, 192 RX words (dedicated pools)
    /// RmtMemoryManager mgr_s3(192, 192, false);
    ///
    /// // Mock ESP32-C3: 96 TX words, 96 RX words (dedicated pools)
    /// RmtMemoryManager mgr_c3(96, 96, false);
    ///
    /// // Mock ESP32: 512 words global pool
    /// RmtMemoryManager mgr_esp32(512, 0, true);
    /// @endcode
    RmtMemoryManager(size_t total_tx, size_t total_rx, bool is_global);

    /// @brief Get platform-specific TX memory limit
    /// @return Number of TX words available on this platform
    static size_t getPlatformTxWords();

    /// @brief Get platform-specific RX memory limit
    /// @return Number of RX words available on this platform (0 for global pool)
    static size_t getPlatformRxWords();

    /// @brief Check if platform uses global memory pool
    /// @return true for ESP32/S2 (global pool), false for S3/C3/C6/H2 (dedicated pools)
    static bool isPlatformGlobalPool();

    /// @brief Allocate memory for TX channel with adaptive buffering policy
    /// @param channel_id RMT channel ID (0-7 for ESP32, 0-3 for S3, 0-1 for C3/C6/H2)
    /// @param use_dma Whether this channel uses DMA (bypasses on-chip memory)
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth) is currently active (affects buffer size)
    /// @return Number of words allocated, or error if insufficient memory
    ///
    /// **DMA vs Non-DMA Memory Usage:**
    /// - DMA: Returns 0 (uses DRAM buffer, bypasses RMT on-chip memory entirely)
    /// - Non-DMA: N× SOC_RMT_MEM_WORDS_PER_CHANNEL where N = calculateMemoryBlocks()
    ///
    /// **Memory Block Calculation:**
    /// - Network OFF: Uses FASTLED_RMT_MEM_BLOCKS (default 2)
    /// - Network ON: Uses FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE (default 3)
    /// - C3/C6/H2: Always capped at 2× (insufficient memory for 3×)
    ///
    /// When with_dma=true in rmt_tx_channel_config_t, mem_block_symbols controls
    /// the DRAM buffer size, NOT on-chip RMT memory.
    Result<size_t, RmtMemoryError> allocateTx(u8 channel_id, bool use_dma, bool networkActive = false);

    /// @brief Allocate memory for RX channel with user-specified size
    /// @param channel_id RMT channel ID (0-7 for ESP32, 0-3 for S3, 0-1 for C3/C6/H2)
    /// @param symbols Number of symbols to allocate (1 symbol = 4 bytes = 1 word)
    /// @param use_dma Whether this channel uses DMA (bypasses on-chip memory, default false)
    /// @return Number of words allocated, or error if insufficient memory
    ///
    /// **DMA vs Non-DMA Memory Usage:**
    /// - DMA: Returns 0 (uses DRAM buffer, bypasses RMT on-chip memory entirely)
    /// - Non-DMA: Allocates 'symbols' words from RX memory pool
    ///
    /// When with_dma=true in rmt_rx_channel_config_t, mem_block_symbols controls
    /// the DRAM buffer size, NOT on-chip RMT memory.
    Result<size_t, RmtMemoryError> allocateRx(u8 channel_id, size_t symbols, bool use_dma = false);

    /// @brief Free allocated memory for a channel
    /// @param channel_id RMT channel ID
    /// @param is_tx true for TX channel, false for RX channel
    void free(u8 channel_id, bool is_tx);

    /// @brief Record allocation after recovery (channel already created externally)
    /// @param channel_id RMT channel ID
    /// @param words Number of words actually allocated
    /// @param is_tx true for TX channel, false for RX channel
    ///
    /// This is used during memory recovery when the ESP-IDF channel was created
    /// successfully but our internal allocation attempt had already been freed.
    /// Adds the allocation to the ledger without re-allocating memory.
    void recordRecoveryAllocation(u8 channel_id, size_t words, bool is_tx);

    /// @brief Query available TX memory
    /// @return Number of words available for TX allocation
    size_t availableTxWords() const;

    /// @brief Query available RX memory
    /// @return Number of words available for RX allocation
    size_t availableRxWords() const;

    /// @brief Check if TX allocation would succeed
    /// @param use_dma Whether DMA would be used (always succeeds)
    /// @param networkActive Whether any network is currently active (affects buffer size)
    /// @return true if allocation would succeed, false otherwise
    bool canAllocateTx(bool use_dma, bool networkActive = false) const;

    /// @brief Check if RX allocation would succeed
    /// @param symbols Number of symbols requested
    /// @return true if allocation would succeed, false otherwise
    bool canAllocateRx(size_t symbols) const;

    /// @brief Get allocation info for a channel (debug/logging)
    /// @param channel_id RMT channel ID
    /// @param is_tx true for TX channel, false for RX channel
    /// @return Number of words allocated, or 0 if not found
    size_t getAllocatedWords(u8 channel_id, bool is_tx) const;

    // ========================================================================
    // State Inspection Methods - For Testing and Debugging
    // ========================================================================

    /// @brief Get total TX memory words configured
    /// @return Total TX words (for dedicated pools or global pool)
    size_t getTotalTxWords() const;

    /// @brief Get total RX memory words configured
    /// @return Total RX words (0 for global pool platforms)
    size_t getTotalRxWords() const;

    /// @brief Get currently allocated TX memory words
    /// @return Allocated TX words
    size_t getAllocatedTxWords() const;

    /// @brief Get currently allocated RX memory words
    /// @return Allocated RX words
    size_t getAllocatedRxWords() const;

    /// @brief Check if any RX channels are currently active (registered)
    /// @return true if at least one RX channel is allocated
    ///
    /// Used by TX driver to detect RX activity and avoid DMA conflicts.
    /// On ESP32-S3, simultaneous RMT TX (with DMA) and RMT RX can cause
    /// transmission issues. When RX is active, TX should use non-DMA mode.
    bool hasActiveRxChannels() const;

    /// @brief Get number of active allocations
    /// @return Count of allocated channels (TX + RX)
    size_t getAllocationCount() const;

    /// @brief Check if using global pool architecture
    /// @return true for global pool (ESP32/S2), false for dedicated pools (S3/C3/C6/H2)
    bool isGlobalPool() const;

    /// @brief Reset all allocations (for testing or error recovery)
    void reset();

    /// @brief Reserve memory for external RMT usage (user-controlled accounting)
    /// @param tx_words Number of TX memory words to reserve (reduce available pool)
    /// @param rx_words Number of RX memory words to reserve (reduce available pool)
    ///
    /// Use this when external code (non-FastLED) is using RMT channels and you need
    /// to prevent FastLED from over-allocating the hardware resources.
    ///
    /// Example:
    /// @code
    /// // External code uses 1 RMT channel with 64 words
    /// auto& mgr = fl::RmtMemoryManager::instance();
    /// mgr.reserveExternalMemory(64, 0);  // Reserve 64 TX words, 0 RX words
    /// @endcode
    void reserveExternalMemory(size_t tx_words, size_t rx_words);

    /// @brief Get currently reserved external memory (debug/logging)
    /// @param tx_words Output: Reserved TX words
    /// @param rx_words Output: Reserved RX words
    void getReservedMemory(size_t& tx_words, size_t& rx_words) const;

    /// @brief Calculate adaptive memory blocks based on network state
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth) is currently active
    /// @return Number of memory blocks to allocate (2 or 3)
    ///
    /// **Memory Block Strategy:**
    /// - Network OFF: Return FASTLED_RMT_MEM_BLOCKS (default 2)
    /// - Network ON: Return FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE (default 3)
    /// - C3/C6/H2/C5 platforms: Always return 2 (insufficient memory for 3×)
    ///
    /// C3/C6/H2/C5 platforms have only 96 words (2 channels × 48 words) of TX memory.
    /// Triple-buffering would require 144 words (3 × 48), which exceeds capacity.
    static size_t calculateMemoryBlocks(bool networkActive);

    /// @brief Configure custom memory block strategy
    /// @param idleBlocks Number of memory blocks when network is inactive (idle state)
    /// @param networkBlocks Number of memory blocks when network is active
    ///
    /// Allows runtime override of the default memory block strategy defined by
    /// FASTLED_RMT_MEM_BLOCKS and FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE.
    ///
    /// **Platform Constraints:**
    /// Values exceeding platform limits are automatically capped:
    /// - ESP32-C3/C6/H2: Max 2 blocks (96 TX words / 48 = 2)
    /// - ESP32-S3: Max 4 blocks (192 TX words / 48 = 4)
    /// - ESP32: Max 8 blocks (512 TX words / 64 = 8)
    ///
    /// Zero values are clamped to minimum of 1 block.
    ///
    /// Example:
    /// @code
    /// auto& mgr = fl::RmtMemoryManager::instance();
    /// mgr.setMemoryBlockStrategy(1, 2);  // 1× idle, 2× network-active
    /// @endcode
    void setMemoryBlockStrategy(size_t idleBlocks, size_t networkBlocks);

    /// @brief Query current memory block strategy
    /// @param idleBlocks Output: Number of blocks for network-idle state
    /// @param networkBlocks Output: Number of blocks for network-active state
    ///
    /// Returns the current memory block strategy, including any runtime
    /// customization via setMemoryBlockStrategy().
    ///
    /// Example:
    /// @code
    /// auto& mgr = fl::RmtMemoryManager::instance();
    /// size_t idle, network;
    /// mgr.getMemoryBlockStrategy(idle, network);
    /// FL_DBG("Strategy: " << idle << "× idle, " << network << "× network");
    /// @endcode
    void getMemoryBlockStrategy(size_t& idleBlocks, size_t& networkBlocks) const;

    // ========================================================================
    // DMA Channel Management (ESP32-S3 only - 1 DMA channel shared TX/RX)
    // ========================================================================

    /// @brief Check if DMA channel is available
    /// @return true if DMA slot available, false if exhausted
    ///
    /// ESP32-S3 has only 1 DMA channel shared between all TX and RX channels.
    /// This checks if the DMA slot is currently available.
    bool isDMAAvailable() const;

    /// @brief Allocate the DMA channel slot
    /// @param channel_id Channel ID requesting DMA
    /// @param is_tx true for TX channel, false for RX channel
    /// @return true on success, false if DMA already allocated
    ///
    /// Only one channel (TX or RX) can hold the DMA slot at a time.
    /// Subsequent allocations will fail until the DMA channel is freed.
    bool allocateDMA(u8 channel_id, bool is_tx);

    /// @brief Free the DMA channel slot
    /// @param channel_id Channel ID releasing DMA
    /// @param is_tx true for TX channel, false for RX channel
    ///
    /// Allows another channel to use DMA after this channel releases it.
    void freeDMA(u8 channel_id, bool is_tx);

    /// @brief Get current DMA allocation info (debug/logging)
    /// @return Number of DMA channels in use (0 or 1)
    int getDMAChannelsInUse() const;

private:
    RmtMemoryManager();
    ~RmtMemoryManager() = default;

    // Prevent copying
    RmtMemoryManager(const RmtMemoryManager&) = delete;
    RmtMemoryManager& operator=(const RmtMemoryManager&) = delete;

    /// @brief Per-channel allocation record
    struct ChannelAllocation {
        u8 channel_id;
        size_t words;
        bool is_tx;
        bool is_dma;  ///< DMA channels don't consume on-chip memory

        ChannelAllocation() : channel_id(0), words(0), is_tx(false), is_dma(false) {}
        ChannelAllocation(u8 id, size_t w, bool tx, bool dma)
            : channel_id(id), words(w), is_tx(tx), is_dma(dma) {}
    };

    /// @brief Memory accounting ledger
    ///
    /// Supports two architectures:
    /// - Global Pool (ESP32, ESP32-S2): Single shared pool for TX and RX
    /// - Dedicated Pools (ESP32-S3, C3, C6, H2): Separate TX and RX pools
    struct MemoryLedger {
        bool is_global_pool;        ///< true = global pool (ESP32/S2), false = dedicated pools (S3/C3/C6/H2)

        // Global pool fields (used when is_global_pool = true)
        size_t total_words;         ///< Total memory available (global pool only)
        size_t allocated_words;     ///< Currently allocated memory (global pool only)

        // Dedicated pool fields (used when is_global_pool = false)
        size_t total_tx_words;      ///< Total TX memory available (dedicated pools only)
        size_t total_rx_words;      ///< Total RX memory available (dedicated pools only)
        size_t allocated_tx_words;  ///< Currently allocated TX memory (dedicated pools only)
        size_t allocated_rx_words;  ///< Currently allocated RX memory (dedicated pools only)

        // External reservation tracking (user-controlled accounting for non-FastLED RMT usage)
        size_t reserved_tx_words;   ///< TX words reserved for external RMT usage
        size_t reserved_rx_words;   ///< RX words reserved for external RMT usage

        fl::vector_inlined<ChannelAllocation, 8> allocations;  ///< Active allocations

        MemoryLedger();
    };

    /// @brief DMA channel allocation tracking (ESP32-S3: 1 channel shared TX/RX)
    struct DMAAllocation {
        u8 channel_id;  ///< Channel ID holding DMA
        bool is_tx;          ///< true if TX channel, false if RX channel
        bool allocated;      ///< true if DMA slot is currently allocated
    };

    MemoryLedger mLedger;
    DMAAllocation mDMAAllocation;  ///< Single DMA channel tracking

    // Memory block strategy configuration (Phase 1A: New API)
    size_t mIdleBlocks;        ///< Number of memory blocks when network is inactive
    size_t mNetworkBlocks;     ///< Number of memory blocks when network is active

    /// @brief Find allocation record for a channel
    /// @param channel_id RMT channel ID
    /// @param is_tx true for TX, false for RX
    /// @return Pointer to allocation, or nullptr if not found
    ChannelAllocation* findAllocation(u8 channel_id, bool is_tx);

    /// @brief Find allocation record for a channel (const version)
    const ChannelAllocation* findAllocation(u8 channel_id, bool is_tx) const;

    /// @brief Initialize platform-specific memory limits
    static void initPlatformLimits(size_t& total_tx, size_t& total_rx);

    /// @brief Helper to get available memory for TX or RX
    /// @param is_tx true for TX pool, false for RX pool
    /// @return Available words (accounting for reservations)
    size_t getAvailableWords(bool is_tx) const;

    /// @brief Helper to allocate words from the appropriate pool
    /// @param words_needed Number of words to allocate
    /// @param is_tx true for TX pool, false for RX pool
    /// @return true if allocation succeeded, false otherwise
    bool tryAllocateWords(size_t words_needed, bool is_tx);

    /// @brief Helper to free words from the appropriate pool
    /// @param words Number of words to free
    /// @param is_tx true for TX pool, false for RX pool
    void freeWords(size_t words, bool is_tx);
};

} // namespace fl

#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
