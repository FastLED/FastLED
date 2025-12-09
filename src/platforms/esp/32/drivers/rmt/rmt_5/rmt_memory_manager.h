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
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "ftl/vector.h"
#include "fl/result.h"

FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
FL_EXTERN_C_END

namespace fl {

/// @brief Error codes for RMT memory allocation
enum class RmtMemoryError : uint8_t {
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
    Result<size_t, RmtMemoryError> allocateTx(uint8_t channel_id, bool use_dma, bool networkActive = false);

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
    Result<size_t, RmtMemoryError> allocateRx(uint8_t channel_id, size_t symbols, bool use_dma = false);

    /// @brief Free allocated memory for a channel
    /// @param channel_id RMT channel ID
    /// @param is_tx true for TX channel, false for RX channel
    void free(uint8_t channel_id, bool is_tx);

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
    size_t getAllocatedWords(uint8_t channel_id, bool is_tx) const;

    /// @brief Reset all allocations (for testing or error recovery)
    void reset();

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
    bool allocateDMA(uint8_t channel_id, bool is_tx);

    /// @brief Free the DMA channel slot
    /// @param channel_id Channel ID releasing DMA
    /// @param is_tx true for TX channel, false for RX channel
    ///
    /// Allows another channel to use DMA after this channel releases it.
    void freeDMA(uint8_t channel_id, bool is_tx);

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
        uint8_t channel_id;
        size_t words;
        bool is_tx;
        bool is_dma;  ///< DMA channels don't consume on-chip memory

        ChannelAllocation() : channel_id(0), words(0), is_tx(false), is_dma(false) {}
        ChannelAllocation(uint8_t id, size_t w, bool tx, bool dma)
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

        fl::vector_inlined<ChannelAllocation, 8> allocations;  ///< Active allocations

        MemoryLedger();
    };

    /// @brief DMA channel allocation tracking (ESP32-S3: 1 channel shared TX/RX)
    struct DMAAllocation {
        uint8_t channel_id;  ///< Channel ID holding DMA
        bool is_tx;          ///< true if TX channel, false if RX channel
        bool allocated;      ///< true if DMA slot is currently allocated
    };

    MemoryLedger mLedger;
    DMAAllocation mDMAAllocation;  ///< Single DMA channel tracking

    /// @brief Find allocation record for a channel
    /// @param channel_id RMT channel ID
    /// @param is_tx true for TX, false for RX
    /// @return Pointer to allocation, or nullptr if not found
    ChannelAllocation* findAllocation(uint8_t channel_id, bool is_tx);

    /// @brief Find allocation record for a channel (const version)
    const ChannelAllocation* findAllocation(uint8_t channel_id, bool is_tx) const;

    /// @brief Initialize platform-specific memory limits
    static void initPlatformLimits(size_t& total_tx, size_t& total_rx);
};

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
