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
#include "driver/gptimer.h"
FL_EXTERN_C_END

#define RMT_ISR_MGR_TAG "rmt5_isr_mgr"

#include "rmt5_worker_isr_mgr_timer.h"

namespace fl {
namespace rmt5_timer {

/**
 * TimerIsrData - Timer Manager Private ISR Data
 *
 * Optimized for high-frequency timer ISR with nibble-level encoding.
 * ISR fires every ~0.5-5µs unconditionally, fills 4 items (1 nibble) at a time.
 *
 * Performance Target: < 500ns ISR execution time
 * Memory: 2 cache lines (128 bytes)
 */
struct alignas(64) TimerIsrData {
    // === CACHE LINE 1: ULTRA-HOT ISR STATE (64 bytes) ===

    // Nibble lookup table pointer (256 bytes, separate allocation)
    const rmt_nibble_lut_t* nibble_lut  __attribute__((aligned(8)));

    // Current pixel data pointer
    const uint8_t* pixel_data           __attribute__((aligned(8)));

    // RMT memory pointers
    volatile rmt_item32_t* rmt_mem_ptr  __attribute__((aligned(8)));
    volatile rmt_item32_t* rmt_mem_start __attribute__((aligned(8)));

    // Encoding state (nibble-level for timer ISR)
    int cur_byte;                        // Current byte offset
    int num_bytes;                       // Total bytes to transmit
    uint8_t current_byte;                // Current byte being processed
    uint8_t nibble_state;                // 0=need new byte, 1=high nibble done
    uint8_t which_half;                  // Ping-pong buffer half (0 or 1)

    // Reset pulse tracking
    uint32_t reset_ticks_remaining;      // Reset pulse countdown

    // Channel info
    uint8_t channel_id;                  // Hardware RMT channel (0-7)
    bool enabled;                        // Transmission active flag

    // Completion callback (ONLY SHARED ELEMENT)
    volatile bool* completed             __attribute__((aligned(8)));

    // Padding to 64 bytes
    uint8_t _pad1[64 - 56];

    // === CACHE LINE 2: TIMER CONFIG/COLD DATA (64 bytes) ===

    // Timer handle
    gptimer_handle_t timer_handle;

    // Encoder fields (for copy encoder approach)
    rmt_channel_handle_t mChannel;       // RMT channel handle
    rmt_encoder_handle_t mCopyEncoder;   // Copy encoder handle
    rmt_symbol_word_t* mSymbolBuffer;    // Pre-converted symbol buffer
    size_t mSymbolBufferSize;            // Symbol buffer size

    // Reset pulse total
    uint32_t reset_ticks_total;

    // Timer config (rarely accessed in ISR)
    uint64_t timer_interval_us;

    // Padding to 128 bytes total
    uint8_t _pad2[64 - 52];

    // Constructor
    TimerIsrData()
        : nibble_lut(nullptr)
        , pixel_data(nullptr)
        , rmt_mem_ptr(nullptr)
        , rmt_mem_start(nullptr)
        , cur_byte(0)
        , num_bytes(0)
        , current_byte(0)
        , nibble_state(0)
        , which_half(0)
        , reset_ticks_remaining(0)
        , channel_id(0xFF)
        , enabled(false)
        , completed(nullptr)
        , timer_handle(nullptr)
        , mChannel(nullptr)
        , mCopyEncoder(nullptr)
        , mSymbolBuffer(nullptr)
        , mSymbolBufferSize(0)
        , reset_ticks_total(0)
        , timer_interval_us(0)
    {
        static_assert(sizeof(TimerIsrData) == 128, "TimerIsrData must be 128 bytes");
        static_assert(alignof(TimerIsrData) == 64, "TimerIsrData must be 64-byte aligned");
    }
};

// Nibble LUT storage (separate from ISR data for better cache control)
// Shared across all channels to save memory
static DRAM_ATTR rmt_nibble_lut_t g_TimerNibbleLut alignas(32) = {};

// Internal implementation class with all private members
class RmtWorkerIsrMgrTimerImpl {
public:
    static RmtWorkerIsrMgrTimerImpl& getInstance();

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

    RmtWorkerIsrMgrTimerImpl();

    bool allocateInterrupt(uint8_t channel_id);
    void deallocateInterrupt(uint8_t channel_id);
    bool isChannelOccupied(uint8_t channel_id) const;
    TimerIsrData* getIsrData(uint8_t channel_id);

    static void IRAM_ATTR fillNextHalf(TimerIsrData* isr_data) __attribute__((hot));
    static void IRAM_ATTR fillAll(TimerIsrData* isr_data) __attribute__((hot));
    static void tx_start(uint8_t channel_id);
    static void IRAM_ATTR sharedGlobalISR_FillAll(void* arg) __attribute__((hot));
    static bool IRAM_ATTR timer_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx);
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
    static TimerIsrData sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP];
    static uint8_t sMaxChannel;
    static gptimer_handle_t sTimerHandle;
    static bool sTimerInitialized;
};

// Static member definitions - DRAM_ATTR applied here for ISR access
DRAM_ATTR TimerIsrData RmtWorkerIsrMgrTimerImpl::sIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP] alignas(64) = {};
DRAM_ATTR uint8_t RmtWorkerIsrMgrTimerImpl::sMaxChannel = SOC_RMT_CHANNELS_PER_GROUP;
gptimer_handle_t RmtWorkerIsrMgrTimerImpl::sTimerHandle = nullptr;
bool RmtWorkerIsrMgrTimerImpl::sTimerInitialized = false;

RmtWorkerIsrMgrTimerImpl::RmtWorkerIsrMgrTimerImpl() {
    FL_LOG_RMT("RmtWorkerIsrMgr: Initialized with " << SOC_RMT_CHANNELS_PER_GROUP << " ISR data slots");
}

// Global singleton instance
static RmtWorkerIsrMgrTimerImpl g_RmtWorkerIsrMgrImpl;

RmtWorkerIsrMgrTimerImpl& RmtWorkerIsrMgrTimerImpl::getInstance() {
    return g_RmtWorkerIsrMgrImpl;
}

// Public API implementation - forwards to internal implementation
RmtWorkerIsrMgrTimer& RmtWorkerIsrMgrTimer::getInstance() {
    return reinterpret_cast<RmtWorkerIsrMgrTimer&>(RmtWorkerIsrMgrTimerImpl::getInstance());
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrTimer::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing,
    rmt_channel_handle_t channel,
    rmt_encoder_handle_t copy_encoder
) {
    return RmtWorkerIsrMgrTimerImpl::getInstance().startTransmission(
        channel_id, completed, rmt_mem, pixel_data, timing, channel, copy_encoder
    );
}

void RmtWorkerIsrMgrTimer::stopTransmission(const RmtIsrHandle& handle) {
    RmtWorkerIsrMgrTimerImpl::getInstance().stopTransmission(handle);
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrTimerImpl::startTransmission(
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

    TimerIsrData* isr_data = &sIsrDataArray[channel_id];

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
    buildNibbleLut(g_TimerNibbleLut, zero_item.val, one_item.val);

    // Extract pointer and length from spans
    volatile rmt_item32_t* rmt_mem_start = rmt_mem.data();
    const uint8_t* pixel_data_ptr = pixel_data.data();
    int num_bytes = static_cast<int>(pixel_data.size());

    // Configure ISR data (direct field assignment for performance)
    isr_data->enabled = false;  // Will be set true when transmission starts
    isr_data->completed = completed;
    isr_data->channel_id = channel_id;
    isr_data->nibble_lut = &g_TimerNibbleLut;
    isr_data->pixel_data = pixel_data_ptr;
    isr_data->num_bytes = num_bytes;
    isr_data->cur_byte = 0;
    isr_data->current_byte = 0;
    isr_data->nibble_state = 0;
    isr_data->which_half = 0;
    isr_data->rmt_mem_start = rmt_mem_start;
    isr_data->rmt_mem_ptr = rmt_mem_start;
    isr_data->reset_ticks_remaining = reset_ticks;
    isr_data->reset_ticks_total = reset_ticks;
    isr_data->timer_handle = sTimerHandle;
    isr_data->timer_interval_us = 0;  // Set by timer init if needed

    FL_LOG_RMT("RmtWorkerIsrMgr: Registered and configured worker on channel " << (int)channel_id);

    // Memory barrier: Ensure all ISR data writes are visible before starting transmission
    // This prevents race conditions where the ISR (potentially on another CPU core) might
    // see partially initialized data when interrupts are enabled by tx_start()
    __sync_synchronize();

    FL_WARN("RMT5 ch" << (int)channel_id << " BEFORE tx_start call - isr_data configured");

    // Start transmission immediately after registration
    tx_start(channel_id);

    FL_WARN("RMT5 ch" << (int)channel_id << " AFTER tx_start call - returned");

    // Return success with handle
    return Result<RmtIsrHandle, RmtRegisterError>::success(RmtIsrHandle(channel_id));
}

void RmtWorkerIsrMgrTimerImpl::stopTransmission(const RmtIsrHandle& handle) {
    uint8_t channel_id = handle.channel_id;

    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel_id=" << (int)channel_id << " during unregister");
        return;
    }

    TimerIsrData* isr_data = &sIsrDataArray[channel_id];

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
    isr_data->current_byte = 0;
    isr_data->nibble_state = 0;
    isr_data->rmt_mem_ptr = isr_data->rmt_mem_start;
    isr_data->pixel_data = nullptr;
    isr_data->num_bytes = 0;

    // Also deallocate interrupt (unregister from worker registry)
    deallocateInterrupt(channel_id);

    FL_LOG_RMT("RmtWorkerIsrMgr: Unregistered channel " << (int)channel_id);
}

bool RmtWorkerIsrMgrTimerImpl::isChannelOccupied(uint8_t channel_id) const {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return false;
    }
    return sIsrDataArray[channel_id].completed != nullptr;
}

TimerIsrData* RmtWorkerIsrMgrTimerImpl::getIsrData(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return nullptr;
    }
    return &sIsrDataArray[channel_id];
}

// Convert byte to RMT symbols using nibble lookup table (inline helper)
// OPTIMIZATION: Use 64-bit writes instead of 8x 32-bit writes for better memory bandwidth
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorkerIsrMgrTimerImpl::convertByteToRmt(
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
void IRAM_ATTR RmtWorkerIsrMgrTimerImpl::fillNextHalf(TimerIsrData* __restrict__ isr_data) {
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
// OPTIMIZATION: Timer mode uses nibble-level filling for maximum buffer utilization
// This function reads the hardware read pointer and fills up to that position,
// maximizing buffer utilization beyond the ping-pong half-buffer strategy.
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorkerIsrMgrTimerImpl::fillAll(TimerIsrData* __restrict__ isr_data) {
    // ISR data pointer passed directly - no array lookup needed

    // Get hardware read pointer (where the RMT engine is currently reading from)
    uint32_t read_addr = RMT5_GET_READ_ADDRESS(isr_data->channel_id);

    // Calculate current write address (offset from buffer start)
    volatile rmt_item32_t* __restrict__ write_ptr = isr_data->rmt_mem_ptr;
    volatile rmt_item32_t* __restrict__ buffer_start = isr_data->rmt_mem_start;
    uint32_t write_addr = write_ptr - buffer_start;

    // Calculate available items to fill (with circular wraparound handling)
    constexpr uint32_t BUFFER_SIZE = FASTLED_RMT5_MAX_PULSES;
    constexpr uint32_t SAFETY_MARGIN = 4;  // 1 nibble worth - allows nibble-level filling

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

    // Early exit if no space available (need at least 4 items for one nibble)
    if (__builtin_expect(available_items < 4, 0)) {
        static DRAM_ATTR uint32_t early_exit_count[SOC_RMT_CHANNELS_PER_GROUP] = {0};
        if (++early_exit_count[isr_data->channel_id] % 50 == 1) {
            FL_WARN("RMT5 fillAll EARLY EXIT ch" << (int)isr_data->channel_id
                   << " read=" << read_addr << " write=" << write_addr
                   << " avail=" << available_items << " (need 4+)");
        }
        return;
    }

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    FASTLED_REGISTER int cur = isr_data->cur_byte;
    FASTLED_REGISTER const int num_bytes = isr_data->num_bytes;

    // Debug: Log available items calculation
    static DRAM_ATTR uint32_t fill_debug_count[SOC_RMT_CHANNELS_PER_GROUP] = {0};
    if (++fill_debug_count[isr_data->channel_id] % 20 == 1) {
        FL_WARN("RMT5 fillAll ch" << (int)isr_data->channel_id
               << " read=" << read_addr << " write=" << write_addr
               << " avail=" << available_items << " cur=" << cur << "/" << num_bytes);
    }
    const uint8_t* __restrict__ pixel_data = isr_data->pixel_data;
    const rmt_nibble_lut_t& lut = isr_data->(*nibble_lut);
    volatile FASTLED_REGISTER rmt_item32_t* pItem = write_ptr;
    FASTLED_REGISTER uint8_t current_byte = isr_data->current_byte;
    FASTLED_REGISTER uint8_t nibble_state = isr_data->nibble_state;

    // Phase 1: Fill pixel data - Timer mode: nibble-level granularity for maximum fill rate
    while (available_items >= 4 && cur < num_bytes) {
        // Check if we have a partial byte in progress
        if (nibble_state == 1) {
            // We have high nibble done, need to process low nibble
            const rmt_item32_t* __restrict__ low_lut = lut[current_byte & 0x0F];

            // Copy low nibble (4 items)
            pItem[0].val = low_lut[0].val;
            pItem[1].val = low_lut[1].val;
            pItem[2].val = low_lut[2].val;
            pItem[3].val = low_lut[3].val;

            pItem += 4;
            available_items -= 4;
            cur++;  // Byte complete
            nibble_state = 0;  // Need new byte

            // Handle circular buffer wraparound
            if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
                pItem = buffer_start;
            }
        } else if (available_items >= 8) {
            // We have space for a full byte, process it whole
            current_byte = pixel_data[cur];
            convertByteToRmt(current_byte, lut, pItem);
            pItem += 8;
            available_items -= 8;
            cur++;
            nibble_state = 0;  // Byte complete

            // Handle circular buffer wraparound
            if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
                pItem = buffer_start;
            }
        } else {
            // Only 4-7 items available, fetch byte and process high nibble only
            current_byte = pixel_data[cur];
            const rmt_item32_t* __restrict__ high_lut = lut[current_byte >> 4];

            // Copy high nibble (4 items)
            pItem[0].val = high_lut[0].val;
            pItem[1].val = high_lut[1].val;
            pItem[2].val = high_lut[2].val;
            pItem[3].val = high_lut[3].val;

            pItem += 4;
            available_items -= 4;
            nibble_state = 1;  // High nibble done, need low nibble next time

            // Handle circular buffer wraparound
            if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
                pItem = buffer_start;
            }

            break;  // Can't fit more, exit loop
        }
    }

    // Phase 2: Fill reset pulse if pixel data complete and space available
    if (__builtin_expect(cur >= num_bytes && nibble_state == 0, 0)) {
        constexpr uint16_t MAX_DURATION = 65535;

        // Debug: Track reset pulse progress
        uint32_t reset_before = isr_data->reset_ticks_remaining;
        int reset_items_written = 0;

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
            reset_items_written++;

            // Handle circular buffer wraparound
            if (__builtin_expect(pItem >= buffer_start + BUFFER_SIZE, 0)) {
                pItem = buffer_start;
            }
        }

        // Debug: Log reset pulse progress (only when we write reset items)
        if (reset_items_written > 0) {
            FL_WARN("RMT5 ch" << (int)isr_data->channel_id << " reset: wrote " << reset_items_written
                   << " items, ticks " << reset_before << " -> " << isr_data->reset_ticks_remaining);
        }
    }

    // Write back updated position (single volatile write instead of many)
    isr_data->cur_byte = cur;
    isr_data->current_byte = current_byte;
    isr_data->nibble_state = nibble_state;
    isr_data->rmt_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP


// Convert all pixel data to RMT symbols in user-space buffer
// Returns true if successful, false if allocation failed
static bool convertPixelsToSymbols(TimerIsrData* isr_data) {
    constexpr uint16_t MAX_DURATION = 65535;  // Max RMT symbol duration
    const size_t num_bytes = isr_data->num_bytes;
    const size_t num_symbols = num_bytes * 8;  // 8 RMT items per byte
    const size_t num_reset_symbols = (isr_data->reset_ticks_remaining + MAX_DURATION - 1) / MAX_DURATION;
    const size_t total_symbols = num_symbols + num_reset_symbols;

    // Allocate buffer if needed
    if (!isr_data->mSymbolBuffer || isr_data->mSymbolBufferSize < total_symbols) {
        if (isr_data->mSymbolBuffer) {
            fl::free(isr_data->mSymbolBuffer);
        }
        isr_data->mSymbolBuffer = (rmt_symbol_word_t*)malloc(total_symbols * sizeof(rmt_symbol_word_t));
        if (!isr_data->mSymbolBuffer) {
            FL_WARN("RMT5 ch" << (int)isr_data->channel_id << " failed to allocate " << total_symbols << " symbols");
            return false;
        }
        isr_data->mSymbolBufferSize = total_symbols;
    }

    // Convert pixel data using nibble LUT
    const uint8_t* pixel_data = isr_data->pixel_data;
    const rmt_nibble_lut_t& lut = isr_data->(*nibble_lut);
    rmt_symbol_word_t* out_ptr = isr_data->mSymbolBuffer;

    for (size_t i = 0; i < num_bytes; i++) {
        uint8_t byte = pixel_data[i];

        // High nibble (bits 7-4) - 4 symbols
        const rmt_item32_t* high_lut = lut[(byte >> 4) & 0x0F];
        out_ptr[0].val = high_lut[0].val;
        out_ptr[1].val = high_lut[1].val;
        out_ptr[2].val = high_lut[2].val;
        out_ptr[3].val = high_lut[3].val;
        out_ptr += 4;

        // Low nibble (bits 3-0) - 4 symbols
        const rmt_item32_t* low_lut = lut[byte & 0x0F];
        out_ptr[0].val = low_lut[0].val;
        out_ptr[1].val = low_lut[1].val;
        out_ptr[2].val = low_lut[2].val;
        out_ptr[3].val = low_lut[3].val;
        out_ptr += 4;
    }

    // Add reset pulse at the end
    if (num_reset_symbols > 0) {
        // Reset pulse: LOW level for mResetTicksRemaining duration
        // Split into multiple symbols if duration exceeds max tick count
        uint32_t ticks_remaining = isr_data->reset_ticks_remaining;
        for (size_t i = 0; i < num_reset_symbols; i++) {
            uint32_t ticks_this_symbol = (ticks_remaining > MAX_DURATION)
                                        ? MAX_DURATION
                                        : ticks_remaining;
            out_ptr->duration0 = ticks_this_symbol;
            out_ptr->level0 = 0;
            out_ptr->duration1 = 0;
            out_ptr->level1 = 0;
            out_ptr++;
            ticks_remaining -= ticks_this_symbol;
        }
    }

    FL_WARN("RMT5 ch" << (int)isr_data->channel_id << " converted " << num_bytes
           << " bytes to " << num_symbols << " symbols + " << num_reset_symbols << " reset symbols");

    return true;
}


// Start RMT transmission (called from main thread, not ISR)
void RmtWorkerIsrMgrTimerImpl::tx_start(uint8_t channel_id) {
    FL_WARN("RMT5 ch" << (int)channel_id << " tx_start ENTRY - function called");

    // NOTE: mResetTicksRemaining does NOT need restoration here
    // It's set in config() during startTransmission() and consumed during transmission.
    // The workflow guarantees startTransmission() is called before each transmission,
    // which properly initializes mResetTicksRemaining for that transmission cycle.

    // Reset buffer state before initial fill
    TimerIsrData* isr_data = &sIsrDataArray[channel_id];

    FL_WARN("RMT5 ch" << (int)channel_id << " tx_start: isr_data=" << (void*)isr_data
           << " mChannel=" << (void*)isr_data->mChannel
           << " mCopyEncoder=" << (void*)isr_data->mCopyEncoder);
    isr_data->which_half = 0;
    isr_data->rmt_mem_ptr = isr_data->rmt_mem_start;
    // NOTE: Do NOT set mEnabled=true here! We need to call rmt_transmit() first
    // to initialize the ESP-IDF driver FSM. ISR will be enabled after that.

    FL_DBG("RMT5 ch" << (int)channel_id << " tx_start: bytes=" << isr_data->num_bytes
           << " reset_ticks=" << isr_data->reset_ticks_remaining);

    // Convert pixel data to RMT symbols in user-space buffer
    // This prepares ALL symbols (including reset pulse) for rmt_transmit()
    if (!convertPixelsToSymbols(isr_data)) {
        FL_WARN("RMT5 ch" << (int)channel_id << " convertPixelsToSymbols FAILED - aborting");
        return;
    }

    // Use rmt_transmit() API with copy encoder to send pre-converted symbols
    // The copy encoder will handle ping-pong refilling automatically
    if (isr_data->mChannel && isr_data->mCopyEncoder) {
        FL_WARN("RMT5 ch" << (int)channel_id << " calling rmt_transmit with "
               << isr_data->mSymbolBufferSize << " symbols");

        rmt_transmit_config_t tx_config = {
            .loop_count = 0,  // No looping, one-shot
            .flags = {
                .eot_level = 0,  // LOW level after transmission (reset state)
            }
        };

        // Pass pre-converted symbols to rmt_transmit()
        // The copy encoder will copy them to hardware memory with automatic refilling
        size_t buffer_bytes = isr_data->mSymbolBufferSize * sizeof(rmt_symbol_word_t);
        esp_err_t ret = rmt_transmit(isr_data->mChannel, isr_data->mCopyEncoder,
                                      isr_data->mSymbolBuffer, buffer_bytes, &tx_config);
        if (ret != ESP_OK) {
            FL_WARN("RMT5 ch" << (int)channel_id << " rmt_transmit failed: " << esp_err_to_name(ret));
        } else {
            FL_WARN("RMT5 ch" << (int)channel_id << " rmt_transmit succeeded - transmission started");
        }
    } else {
        FL_WARN("RMT5 ch" << (int)channel_id << " SKIPPING rmt_transmit - mChannel="
               << (void*)isr_data->mChannel << " mCopyEncoder=" << (void*)isr_data->mCopyEncoder);
    }

    // Note: Timer ISR is NOT needed with this approach - rmt_transmit() handles everything
    // We can disable the timer ISR eventually, but keeping it disabled for now
    // isr_data->enabled remains false

    // Debug: Check if transmission actually started
    uint32_t state_after = RMT5_GET_STATE(channel_id);
    uint32_t read_after = RMT5_GET_READ_ADDRESS(channel_id);
    FL_WARN("RMT5 ch" << (int)channel_id << " after START: state=" << state_after
           << " read_addr=" << read_after << " (0=idle, 1=sending, 2=reading)");
}

// Timer alarm callback - calls sharedGlobalISR_FillAll
bool IRAM_ATTR RmtWorkerIsrMgrTimerImpl::timer_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    // Call the fill-all ISR directly
    RmtWorkerIsrMgrTimerImpl::sharedGlobalISR_FillAll(nullptr);
    return false;  // No high-priority task woken
}

// Allocate interrupt for channel
bool RmtWorkerIsrMgrTimerImpl::allocateInterrupt(uint8_t channel_id) {
    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel ID during interrupt allocation: " << (int)channel_id);
        return false;
    }

    FL_LOG_RMT("RmtWorkerIsrMgr: Allocating timer interrupt for channel " << (int)channel_id);

    // Allocate and configure timer (only once for all channels)
    if (!sTimerInitialized) {
        FL_LOG_RMT("RmtWorkerIsrMgr: Initializing high-frequency timer for buffer fills");

        // Configure timer with high resolution
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = FASTLED_RMT5_TIMER_RESOLUTION_HZ,
            .intr_priority = 0,  // Use default priority (level 1)
            .flags = {
                .intr_shared = 0,  // Dedicated interrupt
            }
        };

        esp_err_t ret = gptimer_new_timer(&timer_config, &sTimerHandle);
        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to create timer: " << esp_err_to_name(ret));
            return false;
        }

        // Register alarm callback
        gptimer_event_callbacks_t cbs = {
            .on_alarm = timer_alarm_callback,
        };
        ret = gptimer_register_event_callbacks(sTimerHandle, &cbs, nullptr);
        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to register timer callback: " << esp_err_to_name(ret));
            gptimer_del_timer(sTimerHandle);
            sTimerHandle = nullptr;
            return false;
        }

        // Configure alarm to fire at configured interval
        gptimer_alarm_config_t alarm_config = {
            .alarm_count = FASTLED_RMT5_TIMER_INTERVAL_TICKS,
            .reload_count = 0,
            .flags = {
                .auto_reload_on_alarm = 1,  // Continuous periodic interrupts
            }
        };
        ret = gptimer_set_alarm_action(sTimerHandle, &alarm_config);
        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to set timer alarm: " << esp_err_to_name(ret));
            gptimer_del_timer(sTimerHandle);
            sTimerHandle = nullptr;
            return false;
        }

        // Enable and start timer
        ret = gptimer_enable(sTimerHandle);
        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to enable timer: " << esp_err_to_name(ret));
            gptimer_del_timer(sTimerHandle);
            sTimerHandle = nullptr;
            return false;
        }

        ret = gptimer_start(sTimerHandle);
        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to start timer: " << esp_err_to_name(ret));
            gptimer_disable(sTimerHandle);
            gptimer_del_timer(sTimerHandle);
            sTimerHandle = nullptr;
            return false;
        }

        sTimerInitialized = true;
        FL_LOG_RMT("RmtWorkerIsrMgr: Timer started successfully - "
                   << FASTLED_RMT5_TIMER_RESOLUTION_HZ << " Hz resolution, "
                   << FASTLED_RMT5_TIMER_INTERVAL_TICKS << " tick interval");
    }

    return true;
}

// Deallocate interrupt for channel
void RmtWorkerIsrMgrTimerImpl::deallocateInterrupt(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return;
    }

    // Timer continues running (shared across all channels)
    // No per-channel cleanup needed for timer-based approach
    FL_LOG_RMT("RmtWorkerIsrMgr: Deallocated channel " << (int)channel_id << " (timer continues running)");
}

// Shared global ISR variant - timer-driven unconditional fill for all enabled channels
// OPTIMIZATION: Called by high-frequency timer at sub-microsecond intervals
// This ISR fills all active channels and detects completion via buffer pointer inspection
// Key difference: No RMT interrupt handling - timer drives everything
void IRAM_ATTR RmtWorkerIsrMgrTimerImpl::sharedGlobalISR_FillAll(void* arg) {
    // Platform-specific bit layout validation
#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "RMT5 worker ISR not yet implemented for this ESP32 variant"
#endif

    // Debug: Track ISR call count per channel
    static DRAM_ATTR uint32_t isr_call_count[SOC_RMT_CHANNELS_PER_GROUP] = {0};

    // Unconditionally fill all enabled channels and check for completion
    // Timer-driven approach: no interrupt status checking needed
    for (uint8_t channel = 0; channel < sMaxChannel; channel++) {
        TimerIsrData* __restrict__ isr_data = &sIsrDataArray[channel];

        // Only process if channel is actively transmitting
        if (!isr_data->enabled) {
            continue;
        }

        // Track ISR calls for this channel
        uint32_t call_count = ++isr_call_count[channel];
        if (call_count % 10 == 1) {  // Log every 10th call (1st, 11th, 21st, etc.)
            FL_WARN("RMT5 ISR ch" << (int)channel << " call#" << call_count
                   << " cur=" << isr_data->cur_byte << "/" << isr_data->num_bytes);
        }

        // Fill buffer aggressively
        fillAll(isr_data);

        // Enhanced completion detection: Software tracking + hardware confirmation
        // OPTIMIZATION: Combines software buffer position tracking with hardware status
        // This provides more reliable completion detection than software-only approach

        // Software state: Check if we've processed all data
        bool software_complete = isr_data->cur_byte >= isr_data->num_bytes;
        software_complete = software_complete && (isr_data->nibble_state == 0);  // No partial nibble
        software_complete = software_complete && (isr_data->reset_ticks_remaining == 0);  // Reset pulse sent

        // Hardware state: Check if RMT hardware has consumed all data
        // Use multiple hardware indicators for robust detection:
        // 1. RMT5_IS_MEM_EMPTY - Hardware buffer empty (mem_empty_chn bit)
        // 2. RMT5_GET_RAW_TX_DONE_INT - Raw TX done interrupt (set even when interrupts disabled)
        bool hardware_buffer_empty = RMT5_IS_MEM_EMPTY(channel);
        bool hardware_tx_done = RMT5_GET_RAW_TX_DONE_INT(channel);

        // Debug: Log completion state periodically (every 100 ISR calls to avoid spam)
        static DRAM_ATTR uint32_t debug_counter[SOC_RMT_CHANNELS_PER_GROUP] = {0};
        if (software_complete && ++debug_counter[channel] % 100 == 0) {
            FL_WARN("RMT5 ch" << (int)channel << " completion check: sw=" << software_complete
                   << " hw_empty=" << hardware_buffer_empty << " hw_done=" << hardware_tx_done
                   << " reset_remaining=" << isr_data->reset_ticks_remaining);
        }

        // Only signal completion when software tracking AND hardware status both confirm completion
        // Require either buffer empty OR TX done interrupt - provides redundancy
        // This prevents race conditions where software thinks it's done but hardware is still active
        if (software_complete && (hardware_buffer_empty || hardware_tx_done)) {
            FL_WARN("RMT5 ch" << (int)channel << " COMPLETE: hw_empty=" << hardware_buffer_empty
                   << " hw_done=" << hardware_tx_done);

            // Clear the raw TX done interrupt flag to prevent false positives on next transmission
            if (hardware_tx_done) {
                RMT5_CLEAR_INTERRUPTS(channel, true, false);
            }

            // Signal completion by setting worker's completion flag to true
            *(isr_data->completed) = true;
            isr_data->enabled = false;  // Disable this channel

            // Reset debug counter
            debug_counter[channel] = 0;
        }
    }
}

} // namespace rmt5_timer
} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
