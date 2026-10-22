/// @file rmt_memory_manager_4.h
/// @brief RMT memory allocation manager for classic ESP32 / IDF 4.x (#3469).
///
/// Ported from the RMT5 `RmtMemoryManager`, simplified for the RMT4
/// hardware model:
///
///   - Classic ESP32: 8 flexible RMT channels × 64 words per block =
///     512 words in a single shared global pool.
///   - ESP32-S2 (IDF 4 path): 4 flexible channels × 64 words = 256
///     words in a single shared global pool.
///   - No DMA on the RMT4 hardware — the entire DMA slot ledger from
///     the RMT5 sibling is deleted, not ported.
///
/// The manager tracks allocations per-channel and exposes the
/// adaptive-buffer recommendation used by TX during network-active
/// windows. Wiring the recommendation into `rmt_config.mem_block_num`
/// gives TX more headroom before the ISR misses its refill deadline
/// while the SDK byte translator retains a late-refill deadline check.
///
/// The accounting ledger has no ESP-IDF dependencies. Shared physical-span
/// helpers live in the companion `_shared.cpp.hpp`, also included by host tests (see
/// `tests/platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.cpp`).
/// Only the platform-default `instance()` singleton constructor lives
/// out-of-line in the `.cpp.hpp` behind the usual `FL_IS_ESP32 &&
/// !FASTLED_RMT5` guard.

#pragma once

// IWYU pragma: private

#include "fl/log/log.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/mutex.h"
#include "fl/stl/atomic.h"
#include "platforms/is_platform.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_config.h"

namespace fl {

/// @brief Error codes for RMT4 memory allocation.
enum class RmtMemoryError4 : u8 {
    OK = 0,
    INSUFFICIENT_MEMORY = 1,       ///< Global pool exhausted.
    CHANNEL_ALREADY_ALLOCATED = 2, ///< Channel ID has an active allocation.
    CHANNEL_NOT_FOUND = 3,         ///< free()/lookup miss.
    INVALID_CHANNEL_ID = 4,        ///< Channel ID out of range.
};

/// @brief One record in the ledger. Kept small so the vector stays
///        cheap even with all channels in use.
struct RmtAllocation4 {
    int channel_id; ///< TX: 0..N-1 raw RMT channel. RX: 128 + (pin & 0x7F).
    u16 words;      ///< Symbol-word count consumed from the pool.
    bool is_tx;     ///< true if TX, false if RX.
};

namespace detail {
/// @brief Words per RMT hardware memory block on classic ESP32 / S2.
///        Kept as a header-inline constant so the class body doesn't
///        need to include the ESP-IDF SOC header.
constexpr size_t kRmt4WordsPerBlock = 64;
constexpr size_t kRmt4TxIdleWords = 2 * kRmt4WordsPerBlock;
constexpr size_t kRmt4TxNetworkActiveWords = 3 * kRmt4WordsPerBlock;

#if defined(FL_IS_ESP32)
// Sleep rather than merely yielding: a higher-priority TX worker must allow
// an application task holding the ledger to run. No interrupt masking or heap.
void rmt4MemoryMutexContentionWait() FL_NO_EXCEPT;
#else
// Host scheduling is preemptive; brief spinning needs no platform dependency.
inline void rmt4MemoryMutexContentionWait() FL_NO_EXCEPT {}
#endif

class Rmt4AsyncMemoryMutex {
  public:
    void lock() const FL_NO_EXCEPT {
        bool expected = false;
        while (!mLocked.compare_exchange_weak(expected, true, memory_order_acquire)) {
            expected = false;
            rmt4MemoryMutexContentionWait();
        }
    }
    void unlock() const FL_NO_EXCEPT { mLocked.store(false, memory_order_release); }

  private:
    mutable atomic_bool mLocked{false};
};

// Empty in blocking builds, so the manager gains no per-instance storage.
class Rmt4BlockingMemoryMutex {
  public:
    void lock() const FL_NO_EXCEPT {}
    void unlock() const FL_NO_EXCEPT {}
};

#if FL_RMT4_ASYNC
using Rmt4MemoryMutex = Rmt4AsyncMemoryMutex;
#else
using Rmt4MemoryMutex = Rmt4BlockingMemoryMutex;
#endif
} // namespace detail

/// @brief RMT4 memory allocator — global-pool tracker for classic
///        ESP32 / S2 with the IDF 4.x driver.
///
/// Async TX and application RX share this ledger. Blocking builds omit the
/// mutex storage and operations.
class RmtMemoryManager4 : private detail::Rmt4MemoryMutex {
  public:
    /// @brief Get the platform-default singleton instance. Only
    ///        defined on `FL_IS_ESP32 && !FASTLED_RMT5` — host tests
    ///        should use the test-only pool-size constructor below
    ///        instead.
    static RmtMemoryManager4 &instance() FL_NO_EXCEPT;

    /// @brief Test-only constructor to inject a custom pool size.
    explicit RmtMemoryManager4(size_t total_words,
                              size_t words_per_block = detail::kRmt4WordsPerBlock) FL_NO_EXCEPT
        : mPoolWords(total_words), mWordsPerBlock(words_per_block),
          mAllocatedWords(0), mReservedBlocks(0),
          mAllocations() {
        mAllocations.reserve(8);
    }

    ~RmtMemoryManager4() FL_NO_EXCEPT = default;

    RmtMemoryManager4(const RmtMemoryManager4 &) = delete;
    RmtMemoryManager4 &operator=(const RmtMemoryManager4 &) = delete;

    //=========================================================================
    // Allocation
    //=========================================================================

    bool tryAllocateTx(int channel_id, bool network_active,
                       size_t &out_words) FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        for (const auto &alloc : mAllocations) {
            if (alloc.channel_id == channel_id && alloc.is_tx) {
                FL_WARN("RmtMemoryManager4: TX channel " << channel_id << " already allocated");
                return false;
            }
        }

        size_t desired = (network_active ? 3 : 2) * mWordsPerBlock;

        if (desired > availableWordsUnlocked()) {
            if (network_active &&
                2 * mWordsPerBlock <= availableWordsUnlocked()) {
                desired = 2 * mWordsPerBlock; // Graceful demotion.
            } else {
                FL_WARN_ONCE("RmtMemoryManager4: TX allocation refused, pool exhausted (want " << desired << ", have " << availableWordsUnlocked() << " / " << mPoolWords << ")");
                return false;
            }
        }

        RmtAllocation4 rec = {};
        rec.channel_id = channel_id;
        rec.words = static_cast<u16>(desired);
        rec.is_tx = true;
        const size_t previous = mAllocations.size();
        mAllocations.push_back(rec);
        if (mAllocations.size() == previous) { return false; }
        mAllocatedWords += desired;
        out_words = desired;
        return true;
    }

    bool tryAllocateRx(int channel_id, size_t symbols,
                       size_t &out_words) FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        for (const auto &alloc : mAllocations) {
            if (alloc.channel_id == channel_id && !alloc.is_tx) {
                FL_WARN("RmtMemoryManager4: RX channel " << channel_id << " already allocated");
                return false;
            }
        }
        if (symbols == 0) {
            FL_WARN("RmtMemoryManager4: RX allocation refused, zero symbols");
            return false;
        }
        if (symbols > availableWordsUnlocked()) {
            FL_WARN("RmtMemoryManager4: RX allocation refused, pool exhausted (want " << symbols << ", have " << availableWordsUnlocked() << " / " << mPoolWords << ")");
            return false;
        }

        RmtAllocation4 rec = {};
        rec.channel_id = channel_id;
        rec.words = static_cast<u16>(symbols);
        rec.is_tx = false;
        const size_t previous = mAllocations.size();
        mAllocations.push_back(rec);
        if (mAllocations.size() == previous) { return false; }
        mAllocatedWords += symbols;
        out_words = symbols;
        return true;
    }

    /// @brief Release an allocation. Silent on miss — callers use
    ///        this in a "free-before-reallocate" idiom where the
    ///        first-time-configure path always misses.
    bool free(int channel_id, bool is_tx) FL_NO_EXCEPT { // ok bare allocation — member function, not C free()
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        for (size_t i = 0; i < mAllocations.size(); ++i) {
            if (mAllocations[i].channel_id == channel_id &&
                mAllocations[i].is_tx == is_tx) {
                mAllocatedWords -= mAllocations[i].words;
                mAllocations[i] = mAllocations.back();
                mAllocations.pop_back();
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // Queries
    //=========================================================================

    /// Reserve the actual contiguous hardware blocks, independently of the
    /// word ledger. RX and TX must both claim before configuring hardware.
    bool tryReserveBlocks(int first, u8 count) FL_NO_EXCEPT;
    void releaseBlocks(int first, u8 count) FL_NO_EXCEPT;

    size_t allocatedWords() const FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        return mAllocatedWords;
    }
    size_t poolSize() const FL_NO_EXCEPT { return mPoolWords; }
    size_t wordsPerBlock() const FL_NO_EXCEPT;
    size_t availableWords() const FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        return availableWordsUnlocked();
    }

    bool hasActiveRxChannels() const FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        for (const auto &alloc : mAllocations) {
            if (!alloc.is_tx) {
                return true;
            }
        }
        return false;
    }

    /// @brief Adaptive-buffer recommendation for a TX channel.
    /// @param network_active  Result of `NetworkStateTracker4::isActive()`.
    /// @return 3 (triple-buffer) when the network is active AND the
    ///         pool has room; otherwise 2 (double-buffer, historic default).
    u8 calculateMemoryBlocks(bool network_active) const FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        if (!network_active) {
            return 2;
        }
        if (availableWordsUnlocked() >= mWordsPerBlock) {
            return 3;
        }
        return 2;
    }

    //=========================================================================
    // Test helpers
    //=========================================================================

    void reset() FL_NO_EXCEPT {
        fl::lock_guard<const detail::Rmt4MemoryMutex> lock(*this);
        mAllocations.clear();
        mAllocatedWords = 0;
        mReservedBlocks = 0;
    }

  private:
    size_t availableWordsUnlocked() const FL_NO_EXCEPT {
        return (mPoolWords > mAllocatedWords) ? (mPoolWords - mAllocatedWords)
                                            : 0;
    }
    size_t mPoolWords;
    size_t mWordsPerBlock;
    size_t mAllocatedWords;
    u32 mReservedBlocks;
    fl::vector<RmtAllocation4> mAllocations;
};

} // namespace fl
