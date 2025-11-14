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

#include "rmt5_worker_isr_mgr_threshold.h"

namespace fl {
namespace rmt5_threshold {

/**
 * ThresholdIsrData - Threshold Manager Private ISR Data
 *
 * Optimized for threshold interrupt-driven refilling.
 * ISR fires when buffer half-empty, refills with on-the-fly pixel encoding.
 *
 * Performance Target: < 1µs ISR execution time
 * Memory: 2 cache lines (128 bytes)
 */
struct alignas(64) ThresholdIsrData {
    // === CACHE LINE 1: HOT ISR STATE (64 bytes) ===

    // Nibble lookup table (256 bytes, separate allocation for cache efficiency)
    // Pointer placed here for fast access
    const rmt_nibble_lut_t* nibble_lut  __attribute__((aligned(8)));

    // Current pixel data pointer (updated during transmission)
    const uint8_t* pixel_data           __attribute__((aligned(8)));

    // RMT memory pointers
    volatile rmt_item32_t* rmt_mem_ptr  __attribute__((aligned(8)));
    volatile rmt_item32_t* rmt_mem_start __attribute__((aligned(8)));

    // Encoding state
    int cur_byte;                        // Current byte offset in pixel data
    int num_bytes;                       // Total bytes to transmit
    uint32_t reset_ticks_remaining;      // Reset pulse countdown
    uint8_t which_half;                  // Ping-pong buffer half (0 or 1)

    // Channel info
    uint8_t channel_id;                  // Hardware RMT channel (0-7)
    bool enabled;                        // Transmission active flag

    // Completion callback (ONLY SHARED ELEMENT)
    volatile bool* completed             __attribute__((aligned(8)));

    // Padding to 64 bytes
    uint8_t _pad1[64 - 56];

    // === CACHE LINE 2: CONFIG/COLD DATA (64 bytes) ===

    // Reset pulse total (template value, rarely accessed)
    uint32_t reset_ticks_total;

    // Threshold config (set once per transmission)
    uint32_t threshold_limit;

    // Padding to 128 bytes total
    uint8_t _pad2[64 - 8];

    // Constructor
    ThresholdIsrData()
        : nibble_lut(nullptr)
        , pixel_data(nullptr)
        , rmt_mem_ptr(nullptr)
        , rmt_mem_start(nullptr)
        , cur_byte(0)
        , num_bytes(0)
        , reset_ticks_remaining(0)
        , which_half(0)
        , channel_id(0xFF)
        , enabled(false)
        , completed(nullptr)
        , reset_ticks_total(0)
        , threshold_limit(0)
    {
        static_assert(sizeof(ThresholdIsrData) == 128, "ThresholdIsrData must be 128 bytes");
        static_assert(alignof(ThresholdIsrData) == 64, "ThresholdIsrData must be 64-byte aligned");
    }
};

// Nibble LUT storage (separate from ISR data for better cache control)
// Shared across all channels to save memory
static DRAM_ATTR rmt_nibble_lut_t g_ThresholdNibbleLut alignas(32) = {};

// Internal implementation class with all private members
class RmtWorkerIsrMgrThresholdImpl {
public:
    static RmtWorkerIsrMgrThresholdImpl& getInstance();

    Result<RmtIsrHandle, RmtRegisterError> startTransmission(
        uint8_t channel_id,
        volatile bool* completed,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const ChipsetTiming& timing,
        rmt_channel_handle_t channel,
        rmt_encoder_handle_t copy_encoder
    );

    void stopTransmission(const RmtIsrHandle& handle);

    RmtWorkerIsrMgrThresholdImpl();

    bool allocateInterrupt(uint8_t channel_id);
    void deallocateInterrupt(uint8_t channel_id);
    bool isChannelOccupied(uint8_t channel_id) const;
    ThresholdIsrData* getIsrData(uint8_t channel_id);

    static void IRAM_ATTR fillNextHalf(ThresholdIsrData* isr_data) __attribute__((hot));
    static void IRAM_ATTR fillAll(ThresholdIsrData* isr_data) __attribute__((hot));
    static void tx_start(uint8_t channel_id);
    static void IRAM_ATTR sharedGlobalISR(void* arg) __attribute__((hot));
    static FASTLED_FORCE_INLINE void IRAM_ATTR convertByteToRmt(
        uint8_t byte_val,
        const rmt_nibble_lut_t& lut,
        volatile rmt_item32_t* out
    ) __attribute__((hot));

    static inline uint32_t ns_to_ticks(uint32_t ns) {
        constexpr uint32_t ONE_GHZ = 1000000000UL;
        constexpr uint32_t ns_per_tick = ONE_GHZ / FASTLED_RMT5_CLOCK_HZ;
        constexpr uint32_t ns_per_tick_half = ns_per_tick / 2;
        return (ns + ns_per_tick_half) / ns_per_tick;
    }

    static inline uint32_t us_to_ticks(uint32_t us) {
        return ns_to_ticks(us * 1000);
    }

    // Static data members (DRAM_ATTR applied in definitions below)
    static ThresholdIsrData sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP];
    static uint8_t sMaxChannel;
    static intr_handle_t sGlobalInterruptHandle;
};

// Static member definitions - DRAM_ATTR applied here for ISR access
DRAM_ATTR ThresholdIsrData RmtWorkerIsrMgrThresholdImpl::sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP] alignas(64) = {};
DRAM_ATTR uint8_t RmtWorkerIsrMgrThresholdImpl::sMaxChannel = SOC_RMT_CHANNELS_PER_GROUP;
intr_handle_t RmtWorkerIsrMgrThresholdImpl::sGlobalInterruptHandle = nullptr;

RmtWorkerIsrMgrThresholdImpl::RmtWorkerIsrMgrThresholdImpl() {
    FL_LOG_RMT("RmtWorkerIsrMgr: Initialized with " << SOC_RMT_CHANNELS_PER_GROUP << " ISR data slots");
}

// Global singleton instance
static RmtWorkerIsrMgrThresholdImpl g_RmtWorkerIsrMgrImpl;

RmtWorkerIsrMgrThresholdImpl& RmtWorkerIsrMgrThresholdImpl::getInstance() {
    return g_RmtWorkerIsrMgrImpl;
}

// Public API implementation - forwards to internal implementation
RmtWorkerIsrMgrThreshold& RmtWorkerIsrMgrThreshold::getInstance() {
    return reinterpret_cast<RmtWorkerIsrMgrThreshold&>(RmtWorkerIsrMgrThresholdImpl::getInstance());
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrThreshold::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing,
    rmt_channel_handle_t channel,
    rmt_encoder_handle_t copy_encoder
) {
    return RmtWorkerIsrMgrThresholdImpl::getInstance().startTransmission(
        channel_id, completed, rmt_mem, pixel_data, timing, channel, copy_encoder
    );
}

void RmtWorkerIsrMgrThreshold::stopTransmission(const RmtIsrHandle& handle) {
    RmtWorkerIsrMgrThresholdImpl::getInstance().stopTransmission(handle);
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrThresholdImpl::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing,
    rmt_channel_handle_t channel,
    rmt_encoder_handle_t copy_encoder
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

    ThresholdIsrData* isr_data = &sIsrDataArray[channel_id];

    // Check if channel is already occupied
    if (isr_data->completed != nullptr) {
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

    // Build nibble LUT (shared across all channels)
    buildNibbleLut(g_ThresholdNibbleLut, zero_item.val, one_item.val);

    // Extract pointer and length from spans
    volatile rmt_item32_t* rmt_mem_start = rmt_mem.data();
    const uint8_t* pixel_data_ptr = pixel_data.data();
    int num_bytes = static_cast<int>(pixel_data.size());

    // Configure ISR data (direct field assignment for performance)
    isr_data->enabled = false;  // Will be set true when transmission starts
    isr_data->completed = completed;
    isr_data->channel_id = channel_id;
    isr_data->nibble_lut = &g_ThresholdNibbleLut;
    isr_data->pixel_data = pixel_data_ptr;
    isr_data->num_bytes = num_bytes;
    isr_data->cur_byte = 0;
    isr_data->which_half = 0;
    isr_data->rmt_mem_start = rmt_mem_start;
    isr_data->rmt_mem_ptr = rmt_mem_start;
    isr_data->reset_ticks_remaining = reset_ticks;
    isr_data->reset_ticks_total = reset_ticks;
    isr_data->threshold_limit = 0;  // Set by manager if needed

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

void RmtWorkerIsrMgrThresholdImpl::stopTransmission(const RmtIsrHandle& handle) {
    uint8_t channel_id = handle.channel_id;

    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel_id=" << (int)channel_id << " during unregister");
        return;
    }

    ThresholdIsrData* isr_data = &sIsrDataArray[channel_id];

    if (isr_data->completed != nullptr) {
        // wait till done
        while (*(isr_data->completed) == false) {
            // Yield to other tasks while waiting
            taskYIELD();
        }
    }

    // Clear completion flag pointer to mark as available
    isr_data->completed = nullptr;
    isr_data->enabled = false;

    // Reset state (optional - ISR data will be reconfigured on next use)
    isr_data->which_half = 0;
    isr_data->cur_byte = 0;
    isr_data->rmt_mem_ptr = isr_data->rmt_mem_start;
    isr_data->pixel_data = nullptr;
    isr_data->num_bytes = 0;

    // Also deallocate interrupt (unregister from worker registry)
    deallocateInterrupt(channel_id);

    FL_LOG_RMT("RmtWorkerIsrMgr: Unregistered channel " << (int)channel_id);
}

bool RmtWorkerIsrMgrThresholdImpl::isChannelOccupied(uint8_t channel_id) const {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return false;
    }
    return sIsrDataArray[channel_id].completed != nullptr;
}

ThresholdIsrData* RmtWorkerIsrMgrThresholdImpl::getIsrData(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return nullptr;
    }
    return &sIsrDataArray[channel_id];
}

// Convert byte to RMT symbols using nibble lookup table (inline helper)
// OPTIMIZATION: Use 64-bit writes instead of 8x 32-bit writes for better memory bandwidth
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorkerIsrMgrThresholdImpl::convertByteToRmt(
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
void IRAM_ATTR RmtWorkerIsrMgrThresholdImpl::fillNextHalf(RmtWorkerIsrData* __restrict__ isr_data) {
    // ISR data pointer passed directly - no array lookup needed

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    // Since we're in ISR context and own the buffer state, we can safely cache and update once
    FASTLED_REGISTER int cur = isr_data->cur_byte;
    FASTLED_REGISTER const int num_bytes = isr_data->num_bytes;
    const uint8_t* __restrict__ pixel_data = isr_data->pixel_data;
    // Cache LUT reference for ISR performance (avoid repeated member access)
    const rmt_nibble_lut_t& lut = isr_data->(*nibble_lut);
    // Remember that volatile writes are super fast, volatile reads are super slow.
    volatile FASTLED_REGISTER rmt_item32_t* pItem = isr_data->rmt_mem_ptr;

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
            if (__builtin_expect(isr_data->reset_ticks_remaining > 0, 1)) {
                uint16_t chunk_duration = (isr_data->reset_ticks_remaining > MAX_DURATION)
                    ? MAX_DURATION
                    : static_cast<uint16_t>(isr_data->reset_ticks_remaining);

                const bool more = (chunk_duration == MAX_DURATION);

                pItem->level0 = 0;
                pItem->duration0 = chunk_duration;
                pItem->level1 = 0;
                pItem->duration1 = more ? 1 : 0;  // 0 is signal for termination.

                isr_data->reset_ticks_remaining -= chunk_duration;
                pItem++;
            } else {
                // Reset complete - exit early
                break;
            }
        }
    }

    // Write back updated position (single volatile write instead of many)
    isr_data->cur_byte = cur;

    // OPTIMIZATION: Use XOR toggle for mWhichHalf (branchless, faster than increment+check)
    const uint8_t which_half = isr_data->which_half;
    isr_data->which_half = which_half ^ 1;  // Toggle between 0 and 1

    // Update pointer based on half (conditional move is faster than branch)
    // If which_half was 1, we wrap to start; if 0, we keep current pItem
    if (__builtin_expect(which_half == 1, 0)) {
        pItem = isr_data->rmt_mem_start;
    }
    isr_data->rmt_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP

// Fill all available space up to RMT hardware read pointer (interrupt context)
// OPTIMIZATION: Threshold mode uses byte-level filling (8 items at a time)
// This function reads the hardware read pointer and fills up to that position,
// maximizing buffer utilization beyond the ping-pong half-buffer strategy.
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorkerIsrMgrThresholdImpl::fillAll(RmtWorkerIsrData* __restrict__ isr_data) {
    // ISR data pointer passed directly - no array lookup needed

    // Get hardware read pointer (where the RMT engine is currently reading from)
    uint32_t read_addr = RMT5_GET_READ_ADDRESS(isr_data->mChannelId);

    // Calculate current write address (offset from buffer start)
    volatile rmt_item32_t* __restrict__ write_ptr = isr_data->rmt_mem_ptr;
    volatile rmt_item32_t* __restrict__ buffer_start = isr_data->rmt_mem_start;
    uint32_t write_addr = write_ptr - buffer_start;

    // Calculate available items to fill (with circular wraparound handling)
    constexpr uint32_t BUFFER_SIZE = FASTLED_RMT5_MAX_PULSES;
    constexpr uint32_t SAFETY_MARGIN = 8;  // 1 byte worth - byte-level filling in RMT mode

    int32_t available_items;
    if (write_addr < read_addr) {
        // Simple case: write pointer is behind read pointer
        available_items = read_addr - write_addr - SAFETY_MARGIN;
    } else if (write_addr > read_addr) {
        // Wraparound case: write pointer is ahead of read pointer
        // Can fill from write_addr to end, then from start to read_addr
        available_items = (BUFFER_SIZE - write_addr) + read_addr - SAFETY_MARGIN;
    } else {
        // write_addr == read_addr: buffer is empty (we're in ISR, so hardware consumed data)
        // Fill entire buffer minus safety margin
        available_items = BUFFER_SIZE - SAFETY_MARGIN;
    }

    // Early exit if no space available (need at least 8 items for one byte)
    if (__builtin_expect(available_items < 8, 0)) {
        return;
    }

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    FASTLED_REGISTER int cur = isr_data->cur_byte;
    FASTLED_REGISTER const int num_bytes = isr_data->num_bytes;
    const uint8_t* __restrict__ pixel_data = isr_data->pixel_data;
    const rmt_nibble_lut_t& lut = isr_data->(*nibble_lut);
    volatile FASTLED_REGISTER rmt_item32_t* pItem = write_ptr;

    // Phase 1: Fill pixel data - Threshold mode: byte-level granularity (8 items at a time)
    int available_bytes = available_items / 8;
    const int bytes_remaining = num_bytes - cur;
    const int bytes_to_convert = (bytes_remaining < available_bytes) ? bytes_remaining : available_bytes;

    for (FASTLED_REGISTER int i = 0; __builtin_expect(i < bytes_to_convert, 1); i++) {
        convertByteToRmt(pixel_data[cur], lut, pItem);
        pItem += 8;
        cur++;

        // Handle circular buffer wraparound
        if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
            pItem = buffer_start;
        }
    }

    available_items -= bytes_to_convert * 8;

    // Phase 2: Fill reset pulse if pixel data complete and space available
    if (__builtin_expect(cur >= num_bytes, 0)) {
        constexpr uint16_t MAX_DURATION = 65535;

        // Fill reset pulse items up to available space
        while (__builtin_expect(available_items > 0 && isr_data->reset_ticks_remaining > 0, 1)) {
            uint16_t chunk_duration = (isr_data->reset_ticks_remaining > MAX_DURATION)
                ? MAX_DURATION
                : static_cast<uint16_t>(isr_data->reset_ticks_remaining);

            const bool more = (chunk_duration == MAX_DURATION);

            pItem->level0 = 0;
            pItem->duration0 = chunk_duration;
            pItem->level1 = 0;
            pItem->duration1 = more ? 1 : 0;  // 0 signals termination

            isr_data->reset_ticks_remaining -= chunk_duration;
            pItem++;
            available_items--;

            // Handle circular buffer wraparound
            if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
                pItem = buffer_start;
            }
        }
    }

    // Write back updated position (single volatile write instead of many)
    isr_data->cur_byte = cur;
    isr_data->rmt_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP



// Start RMT transmission (called from main thread, not ISR)
void RmtWorkerIsrMgrThresholdImpl::tx_start(uint8_t channel_id) {
    // NOTE: mResetTicksRemaining does NOT need restoration here
    // It's set in config() during startTransmission() and consumed during transmission.
    // The workflow guarantees startTransmission() is called before each transmission,
    // which properly initializes mResetTicksRemaining for that transmission cycle.

    // Reset buffer state before initial fill
    RmtWorkerIsrData* isr_data = &sIsrDataArray[channel_id];
    isr_data->which_half = 0;
    isr_data->rmt_mem_ptr = isr_data->rmt_mem_start;
    isr_data->enabled = true;

    // Reset RMT memory read pointer
    RMT5_RESET_MEMORY_READ_POINTER(channel_id);

    // Fill both halves of ping-pong buffer initially (like RMT4)
    fillNextHalf(isr_data);  // Fill half 0
    fillNextHalf(isr_data);  // Fill half 1

    // Clear and enable both TX end and threshold interrupts
    RMT5_CLEAR_INTERRUPTS(channel_id, true, true);
    RMT5_ENABLE_INTERRUPTS(channel_id, true, true);

    // Start transmission
    RMT5_START_TRANSMISSION(channel_id);
}

// Allocate interrupt for channel
bool RmtWorkerIsrMgrThresholdImpl::allocateInterrupt(uint8_t channel_id) {
    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel ID during interrupt allocation: " << (int)channel_id);
        return false;
    }

    FL_LOG_RMT("RmtWorkerIsrMgr: Allocating RMT threshold interrupt for channel " << (int)channel_id);

    // Enable threshold interrupt for this channel using RMT device macro
    // This is idempotent - safe to call multiple times
    RMT5_ENABLE_THRESHOLD_INTERRUPT(channel_id);

    // Allocate shared global ISR (only once for all channels, like RMT4)
    if (sGlobalInterruptHandle == nullptr) {
        FL_LOG_RMT("RmtWorkerIsrMgr: Allocating shared global ISR for all RMT channels");

        esp_err_t ret = esp_intr_alloc(
            ETS_RMT_INTR_SOURCE,
            ESP_INTR_FLAG_IRAM | FL_RMT5_INTERRUPT_LEVEL,
            &RmtWorkerIsrMgrThresholdImpl::sharedGlobalISR,
            nullptr,  // No user data - ISR uses static member arrays
            &sGlobalInterruptHandle
        );

        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to allocate shared ISR: " << esp_err_to_name(ret) << " (0x" << (int)ret << ")");
            return false;
        }

        FL_LOG_RMT("RmtWorkerIsrMgr: Shared global ISR allocated successfully (Level "
                   << ((FL_RMT5_INTERRUPT_LEVEL >> 1) & 7) << ", ETS_RMT_INTR_SOURCE)");
    }

    return true;
}

// Deallocate interrupt for channel
void RmtWorkerIsrMgrThresholdImpl::deallocateInterrupt(uint8_t channel_id) {
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
void IRAM_ATTR RmtWorkerIsrMgrThresholdImpl::sharedGlobalISR(void* arg) {
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
        if (__builtin_expect(!isr_data->enabled, 0)) {
            continue;
        }

        // Signal completion by setting worker's completion flag to true
        // This allows the worker to detect completion and unregister
        *(isr_data->completed) = true;
        isr_data->enabled = false;  // Disable this channel

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
        if (__builtin_expect(!isr_data->enabled, 0)) {
            continue;
        }

        // Refill buffer aggressively up to hardware read pointer
        // This replaces fillNextHalf() with fillAll() for better buffer utilization
        fillAll(isr_data);

        // Clear threshold interrupt in hardware
        RMT5_CLEAR_INTERRUPTS(channel, false, true);
    }
}

} // namespace rmt5_threshold
} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
