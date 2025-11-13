#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_isr_mgr.h"
#include "rmt5_worker.h"
#include "rmt5_device.h"
#include "common.h"
#include "fl/log.h"
#include "fl/register.h"

FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
#include "soc/rmt_struct.h"
#include "esp_err.h"
#include "soc/rmt_periph.h"
#include "esp_intr_alloc.h"
FL_EXTERN_C_END

#define RMT_ISR_MGR_TAG "rmt5_isr_mgr"

namespace fl {

/**
 * RmtWorkerIsrMgrImpl - Concrete implementation of RmtWorkerIsrMgr interface
 *
 * Internal implementation class that manages ISR data and interrupt allocation.
 * Kept in cpp file to hide implementation details.
 */
class RmtWorkerIsrMgrImpl {
public:

    static RmtWorkerIsrMgrImpl& getInstance();

    RmtWorkerIsrMgrImpl();
    ~RmtWorkerIsrMgrImpl() = default;

    // Implement pure virtual interface methods
    Result<RmtIsrHandle, RmtRegisterError> startTransmission(
        uint8_t channel_id,
        volatile bool* completed,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const ChipsetTiming& timing
    );

    void stopTransmission(const RmtIsrHandle& handle);


    // Internal helper methods
    bool allocateInterrupt(uint8_t channel_id);
    void deallocateInterrupt(uint8_t channel_id);
    bool isChannelOccupied(uint8_t channel_id) const;
    RmtWorkerIsrData* getIsrData(uint8_t channel_id);

    // Static ISR methods (internal implementation details)
    // Marked with __attribute__((hot)) for aggressive optimization
    static void IRAM_ATTR fillNextHalf(uint8_t channel_id) __attribute__((hot));
    static void tx_start(uint8_t channel_id);
    static void IRAM_ATTR sharedGlobalISR(void* arg) __attribute__((hot));
    static FASTLED_FORCE_INLINE void IRAM_ATTR convertByteToRmt(
        uint8_t byte_val,
        const rmt_nibble_lut_t& lut,
        volatile rmt_item32_t* out
    ) __attribute__((hot));

private:
    // Helper: Convert nanoseconds to RMT ticks
    // Works for any clock frequency defined by FASTLED_RMT5_CLOCK_HZ
    // Returns uint32_t to support high-frequency clocks (40MHz+) with long pulse times
    static inline uint32_t ns_to_ticks(uint32_t ns) {
        // Formula: ticks = (ns * CLOCK_HZ) / 1,000,000,000
        // To avoid overflow for large ns values, we rearrange:
        // ticks = ns / (1,000,000,000 / CLOCK_HZ)
        constexpr uint32_t ONE_GHZ = 1000000000UL;
        constexpr uint32_t ns_per_tick = ONE_GHZ / FASTLED_RMT5_CLOCK_HZ;
        constexpr uint32_t ns_per_tick_half = ns_per_tick / 2;

        // Add half a tick for rounding
        return (ns + ns_per_tick_half) / ns_per_tick;
    }

    // Helper: Convert microseconds to RMT ticks
    // Used for reset pulses which are specified in microseconds and can be large (280µs+)
    // Returns uint32_t to support high-frequency clocks (40MHz+) with long reset times
    static inline uint32_t us_to_ticks(uint32_t us) {
        return ns_to_ticks(us * 1000);
    }

    // Static data members - shared across all instances
    // ISR data pool - one per hardware channel
    // DRAM attribute for ISR access
    static RmtWorkerIsrData sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP];

    // Global interrupt handle (shared by all channels)
    static intr_handle_t sGlobalInterruptHandle;

    // Maximum channel number
    static uint8_t sMaxChannel;

    // Allow static methods to access implementation
    friend class RmtWorkerIsrMgr;
};


// Static member definitions with DRAM_ATTR for ISR access
DRAM_ATTR RmtWorkerIsrData RmtWorkerIsrMgrImpl::sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP] = {};
DRAM_ATTR uint8_t RmtWorkerIsrMgrImpl::sMaxChannel = SOC_RMT_CHANNELS_PER_GROUP;

// Non-ISR static members (no DRAM_ATTR needed)
intr_handle_t RmtWorkerIsrMgrImpl::sGlobalInterruptHandle = nullptr;

RmtWorkerIsrMgrImpl::RmtWorkerIsrMgrImpl() {
    // Static members are initialized above with default values
    // ISR data array is default-constructed (all mCompleted pointers are nullptr)
    FL_LOG_RMT("RmtWorkerIsrMgr: Initialized with " << SOC_RMT_CHANNELS_PER_GROUP << " ISR data slots");
}

// Global singleton instance - now just a thin wrapper
// All data is in static members for direct ISR access
static RmtWorkerIsrMgrImpl g_RmtWorkerIsrMgr;

RmtWorkerIsrMgrImpl& RmtWorkerIsrMgrImpl::getInstance() {
    return g_RmtWorkerIsrMgr;
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrImpl::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing
) {
    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel_id=" << (int)channel_id
                << " (max=" << (SOC_RMT_CHANNELS_PER_GROUP - 1) << ")");
        return Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::INVALID_CHANNEL,
            "Channel ID out of valid range"
        );
    }

    // Validate completion flag pointer
    if (completed == nullptr) {
        FL_WARN("RmtWorkerIsrMgr: Null completion flag pointer for channel " << (int)channel_id);
        return Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::INVALID_CHANNEL,
            "Null completion flag pointer"
        );
    }

    RmtWorkerIsrData* isr_data = &sIsrDataArray[channel_id];

    // Check if channel is already occupied
    if (isr_data->mCompleted != nullptr) {
        FL_WARN("RmtWorkerIsrMgr: Channel " << (int)channel_id
                << " already occupied by another worker");
        return Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::CHANNEL_OCCUPIED,
            "Channel already in use"
        );
    }

    // Allocate interrupt on first registration (lazy initialization)
    if (!allocateInterrupt(channel_id)) {
        FL_WARN("RmtWorkerIsrMgr: Failed to allocate interrupt for channel " << (int)channel_id);
        return Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::INTERRUPT_ALLOC_FAILED,
            "Failed to allocate interrupt"
        );
    }

    // Convert timing to RMT ticks
    // T1, T2, T3 are in nanoseconds; RESET is in microseconds
    // T1-T3 fit in uint16_t even at 40MHz (max ~1.6µs per pulse)
    // Reset time uses uint32_t to support long latches at high frequencies
    uint16_t t1_ticks = static_cast<uint16_t>(ns_to_ticks(timing.T1));
    uint16_t t2_ticks = static_cast<uint16_t>(ns_to_ticks(timing.T2));
    uint16_t t3_ticks = static_cast<uint16_t>(ns_to_ticks(timing.T3));
    uint32_t reset_ticks = us_to_ticks(timing.RESET);

    // Build RMT items for 0 and 1 bits
    rmt_item32_t zero_item;
    zero_item.level0 = 1;
    zero_item.duration0 = t1_ticks;
    zero_item.level1 = 0;
    zero_item.duration1 = t2_ticks + t3_ticks;

    rmt_item32_t one_item;
    one_item.level0 = 1;
    one_item.duration0 = t1_ticks + t2_ticks;
    one_item.level1 = 0;
    one_item.duration1 = t3_ticks;

    // Build nibble LUT using helper function
    rmt_nibble_lut_t nibble_lut;
    buildNibbleLut(nibble_lut, zero_item.val, one_item.val);

    // Extract pointer and length from spans
    volatile rmt_item32_t* rmt_mem_start = rmt_mem.data();
    const uint8_t* pixel_data_ptr = pixel_data.data();
    int num_bytes = static_cast<int>(pixel_data.size());

    // Configure ISR data using the provided config() method
    isr_data->config(completed, channel_id, rmt_mem_start, pixel_data_ptr, num_bytes, nibble_lut, reset_ticks);

    FL_LOG_RMT("RmtWorkerIsrMgr: Registered and configured worker on channel " << (int)channel_id);

    // Memory barrier: Ensure all ISR data writes are visible before starting transmission
    // This prevents race conditions where the ISR (potentially on another CPU core) might
    // see partially initialized data when interrupts are enabled by tx_start()
    __sync_synchronize();

    // Start transmission immediately after registration
    tx_start(channel_id);

    // Return success with handle
    return Result<RmtIsrHandle, RmtRegisterError>::success(RmtIsrHandle(channel_id));
}

void RmtWorkerIsrMgrImpl::stopTransmission(const RmtIsrHandle& handle) {
    uint8_t channel_id = handle.channel_id;

    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel_id=" << (int)channel_id << " during unregister");
        return;
    }

    RmtWorkerIsrData* isr_data = &sIsrDataArray[channel_id];

    if (isr_data->mCompleted != nullptr) {
        // wait till done
        while (*(isr_data->mCompleted) == false) {
            // Yield to other tasks while waiting
            taskYIELD();
        }
    }

    // Clear completion flag pointer to mark as available
    isr_data->mCompleted = nullptr;
    isr_data->mEnabled = false;

    // Reset state (optional - ISR data will be reconfigured on next use)
    isr_data->mWhichHalf = 0;
    isr_data->mCur = 0;
    isr_data->mRMT_mem_ptr = isr_data->mRMT_mem_start;
    isr_data->mPixelData = nullptr;
    isr_data->mNumBytes = 0;

    // Also deallocate interrupt (unregister from worker registry)
    deallocateInterrupt(channel_id);

    FL_LOG_RMT("RmtWorkerIsrMgr: Unregistered channel " << (int)channel_id);
}

bool RmtWorkerIsrMgrImpl::isChannelOccupied(uint8_t channel_id) const {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return false;
    }
    return sIsrDataArray[channel_id].mCompleted != nullptr;
}

RmtWorkerIsrData* RmtWorkerIsrMgrImpl::getIsrData(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return nullptr;
    }
    return &sIsrDataArray[channel_id];
}

// Convert byte to RMT symbols using nibble lookup table (inline helper)
// OPTIMIZATION: Use 64-bit writes instead of 8x 32-bit writes for better memory bandwidth
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorkerIsrMgrImpl::convertByteToRmt(
    uint8_t byte_val,
    const rmt_nibble_lut_t& lut,
    volatile rmt_item32_t* out
) {
    // ESP32 supports efficient unaligned 64-bit access
    // Use 64-bit writes (2 RMT items per write) for 4 total writes instead of 8
    // This reduces memory bus traffic and improves cache efficiency

    // Get pointers to LUT entries
    const rmt_item32_t* __restrict__ high_lut = lut[byte_val >> 4];
    const rmt_item32_t* __restrict__ low_lut = lut[byte_val & 0x0F];

    // Cast to 64-bit pointers for wider writes
    // Each write transfers 2 RMT items (2 x 32-bit = 64-bit)
    volatile uint64_t* __restrict__ out64 = reinterpret_cast<volatile uint64_t*>(out);
    const uint64_t* __restrict__ high_lut64 = reinterpret_cast<const uint64_t*>(high_lut);
    const uint64_t* __restrict__ low_lut64 = reinterpret_cast<const uint64_t*>(low_lut);

    // Copy high nibble (4 items = 2 x 64-bit writes)
    out64[0] = high_lut64[0];  // Items 0-1
    out64[1] = high_lut64[1];  // Items 2-3

    // Copy low nibble (4 items = 2 x 64-bit writes)
    out64[2] = low_lut64[0];  // Items 4-5
    out64[3] = low_lut64[1];  // Items 6-7
}
FL_DISABLE_WARNING_POP

// Fill next half of RMT buffer (interrupt context)
// OPTIMIZATION: Matches RMT4 approach - no defensive checks, trust the buffer sizing math
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorkerIsrMgrImpl::fillNextHalf(uint8_t channel_id) {
    // Direct static member access (no instance needed, no function call overhead)
    RmtWorkerIsrData* __restrict__ isr_data = &sIsrDataArray[channel_id];

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    // Since we're in ISR context and own the buffer state, we can safely cache and update once
    FASTLED_REGISTER int cur = isr_data->mCur;
    FASTLED_REGISTER const int num_bytes = isr_data->mNumBytes;
    const uint8_t* __restrict__ pixel_data = isr_data->mPixelData;
    // Cache LUT reference for ISR performance (avoid repeated member access)
    const rmt_nibble_lut_t& lut = isr_data->mNibbleLut;
    // Remember that volatile writes are super fast, volatile reads are super slow.
    volatile FASTLED_REGISTER rmt_item32_t* pItem = isr_data->mRMT_mem_ptr;

    // Calculate buffer constants (matching RmtWorker values from common.h)
    constexpr int PULSES_PER_FILL = FASTLED_RMT5_PULSES_PER_FILL;
    constexpr int BYTES_PER_FILL = PULSES_PER_FILL / 8;

    // OPTIMIZATION: Split into two phases for better branch prediction
    // Phase 1: Fill pixel data (hot path - highly predictable)
    const int bytes_remaining = num_bytes - cur;
    const int bytes_to_convert = (bytes_remaining < BYTES_PER_FILL) ? bytes_remaining : BYTES_PER_FILL;

    // Main pixel conversion loop (no branches inside, perfect for branch prediction)
    for (FASTLED_REGISTER int i = 0; __builtin_expect(i < bytes_to_convert, 1); i++) {
        convertByteToRmt(pixel_data[cur], lut, pItem);
        pItem += 8;
        cur++;
    }

    // Phase 2: Fill reset pulse if needed (cold path - only happens at end)
    if (__builtin_expect(cur >= num_bytes, 0)) {
        // Reset pulse - LOW level for reset duration, then terminator
        // This ensures proper LED latch timing for chipsets like WS2812
        // For high-frequency clocks (40MHz+), reset time may exceed uint16_t max (65,535 ticks)
        // In this case, chain multiple RMT items across multiple fillNextHalf() calls
        // mResetTicksRemaining is initialized in config() during startTransmission()

        constexpr uint16_t MAX_DURATION = 65535;
        const int items_filled = bytes_to_convert * 8;
        const int items_remaining = PULSES_PER_FILL - items_filled;

        // Fill remaining buffer space with reset pulse chunks
        for (FASTLED_REGISTER int i = 0; i < items_remaining; i++) {
            if (__builtin_expect(isr_data->mResetTicksRemaining > 0, 1)) {
                uint16_t chunk_duration = (isr_data->mResetTicksRemaining > MAX_DURATION)
                    ? MAX_DURATION
                    : static_cast<uint16_t>(isr_data->mResetTicksRemaining);

                const bool more = (chunk_duration == MAX_DURATION);

                pItem->level0 = 0;
                pItem->duration0 = chunk_duration;
                pItem->level1 = 0;
                pItem->duration1 = more ? 1 : 0;  // 0 is signal for termination.

                isr_data->mResetTicksRemaining -= chunk_duration;
                pItem++;
            } else {
                // Reset complete - exit early
                break;
            }
        }
    }

    // Write back updated position (single volatile write instead of many)
    isr_data->mCur = cur;

    // OPTIMIZATION: Use XOR toggle for mWhichHalf (branchless, faster than increment+check)
    const uint8_t which_half = isr_data->mWhichHalf;
    isr_data->mWhichHalf = which_half ^ 1;  // Toggle between 0 and 1

    // Update pointer based on half (conditional move is faster than branch)
    // If which_half was 1, we wrap to start; if 0, we keep current pItem
    if (__builtin_expect(which_half == 1, 0)) {
        pItem = isr_data->mRMT_mem_start;
    }
    isr_data->mRMT_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP



// Start RMT transmission (called from main thread, not ISR)
void RmtWorkerIsrMgrImpl::tx_start(uint8_t channel_id) {
    // NOTE: mResetTicksRemaining does NOT need restoration here
    // It's set in config() during startTransmission() and consumed during transmission.
    // The workflow guarantees startTransmission() is called before each transmission,
    // which properly initializes mResetTicksRemaining for that transmission cycle.

    // Fill both halves of ping-pong buffer initially (like RMT4)
    fillNextHalf(channel_id);  // Fill half 0
    fillNextHalf(channel_id);  // Fill half 1

    // Reset buffer state before starting transmission (fill both halves).
    RmtWorkerIsrData* isr_data = &sIsrDataArray[channel_id];
    isr_data->mWhichHalf = 0;
    isr_data->mRMT_mem_ptr = isr_data->mRMT_mem_start;
    isr_data->mEnabled = true;

    // Reset RMT memory read pointer
    RMT5_RESET_MEMORY_READ_POINTER(channel_id);

    // Clear and enable both TX end and threshold interrupts
    RMT5_CLEAR_INTERRUPTS(channel_id, true, true);
    RMT5_ENABLE_INTERRUPTS(channel_id, true, true);

    // Start transmission
    RMT5_START_TRANSMISSION(channel_id);
}

// Allocate interrupt for channel
bool RmtWorkerIsrMgrImpl::allocateInterrupt(uint8_t channel_id) {
    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel ID during interrupt allocation: " << (int)channel_id);
        return false;
    }

    FL_LOG_RMT("RmtWorkerIsrMgr: Allocating interrupt for channel " << (int)channel_id);

    // Enable threshold interrupt for this channel using RMT device macro
    // This is idempotent - safe to call multiple times
    RMT5_ENABLE_THRESHOLD_INTERRUPT(channel_id);

    // Allocate shared global ISR (only once for all channels, like RMT4)
    if (sGlobalInterruptHandle == nullptr) {
        FL_LOG_RMT("RmtWorkerIsrMgr: Allocating shared global ISR for all RMT channels");

        esp_err_t ret = esp_intr_alloc(
            ETS_RMT_INTR_SOURCE,
            ESP_INTR_FLAG_IRAM | FL_RMT5_INTERRUPT_LEVEL,
            &RmtWorkerIsrMgrImpl::sharedGlobalISR,
            nullptr,  // No user data - ISR uses static member arrays
            &sGlobalInterruptHandle
        );

        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to allocate shared ISR: " << esp_err_to_name(ret) << " (0x" << (int)ret << ")");
            return false;
        }

        FL_LOG_RMT("RmtWorkerIsrMgr: Shared global ISR allocated successfully (Level 3, ETS_RMT_INTR_SOURCE)");
    }

    return true;
}

// Deallocate interrupt for channel
void RmtWorkerIsrMgrImpl::deallocateInterrupt(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return;
    }

    // Disable threshold interrupt for this channel
    // This is idempotent - safe to call multiple times
    RMT5_DISABLE_THRESHOLD_INTERRUPT(channel_id);

    FL_LOG_RMT("RmtWorkerIsrMgr: Deallocated interrupt for channel " << (int)channel_id);
}

// Shared global ISR - processes all channels using bit-scanning for optimal performance
// OPTIMIZATION: Uses __builtin_ctz (count trailing zeros) to find active channels
// instead of linear iteration. This provides 3-4x speedup for sparse interrupts (1-2 channels).
// The ESP32's Xtensa NSAU instruction makes __builtin_ctz a single-cycle operation.
void IRAM_ATTR RmtWorkerIsrMgrImpl::sharedGlobalISR(void* arg) {
    // Direct static member access (no instance needed, zero overhead)
    // Read interrupt status once - captures all pending channel interrupts atomically
    uint32_t intr_st = RMT5_READ_INTERRUPT_STATUS();

    // Early exit if no interrupts pending (optimization for common case)
    if (__builtin_expect(intr_st == 0, 0)) {
        return;
    }

    // Platform-specific bit layout validation
#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "RMT5 worker ISR not yet implemented for this ESP32 variant"
#endif

    // OPTIMIZATION: Split processing for TX done and threshold interrupts
    // This approach is cleaner and allows better optimization by the compiler

    // Process TX done interrupts (bits 0-7)
    // These signal transmission completion
    uint32_t done_mask = intr_st & 0xFF;
    while (done_mask != 0) {
        // Find first set bit using count-trailing-zeros (single-cycle on ESP32)
        const uint8_t channel = __builtin_ctz(done_mask);

        // Clear this bit from our working mask (not the hardware status yet)
        done_mask &= ~(1U << channel);

        // Bounds check (should never fail, but safety first in ISR)
        if (__builtin_expect(channel >= sMaxChannel, 0)) {
            continue;
        }

        RmtWorkerIsrData* __restrict__ isr_data = &sIsrDataArray[channel];

        // Skip inactive channels (worker not currently transmitting)
        if (__builtin_expect(!isr_data->mEnabled, 0)) {
            continue;
        }

        // Signal completion by setting worker's completion flag to true
        // This allows the worker to detect completion and unregister
        *(isr_data->mCompleted) = true;
        isr_data->mEnabled = false;  // Disable this channel

        // Clear TX done interrupt in hardware
        RMT5_CLEAR_INTERRUPTS(channel, true, false);
    }

    // Process threshold interrupts (bits 8-15, representing channels 0-7)
    // These signal that buffer half is empty and needs refilling
    uint32_t thresh_mask = (intr_st >> 8) & 0xFF;
    while (thresh_mask != 0) {
        // Find first set bit using count-trailing-zeros (single-cycle on ESP32)
        const uint8_t channel = __builtin_ctz(thresh_mask);

        // Clear this bit from our working mask (not the hardware status yet)
        thresh_mask &= ~(1U << channel);

        // Bounds check (should never fail, but safety first in ISR)
        if (__builtin_expect(channel >= sMaxChannel, 0)) {
            continue;
        }

        RmtWorkerIsrData* __restrict__ isr_data = &sIsrDataArray[channel];

        // Skip inactive channels (worker not currently transmitting)
        if (__builtin_expect(!isr_data->mEnabled, 0)) {
            continue;
        }

        // Refill next half of ping-pong buffer
        fillNextHalf(channel);

        // Clear threshold interrupt in hardware
        RMT5_CLEAR_INTERRUPTS(channel, false, true);
    }
}


// Bind to api.
Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgr::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing
) {
    return RmtWorkerIsrMgrImpl::getInstance().startTransmission(channel_id, completed, rmt_mem, pixel_data, timing);
}

void RmtWorkerIsrMgr::stopTransmission(const RmtIsrHandle& handle) {
    RmtWorkerIsrMgrImpl::getInstance().stopTransmission(handle);
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
