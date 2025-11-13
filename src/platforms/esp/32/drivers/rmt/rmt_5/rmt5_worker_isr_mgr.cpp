#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_isr_mgr.h"
#include "rmt5_worker.h"
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
class RmtWorkerIsrMgrImpl : public RmtWorkerIsrMgr {
public:
    RmtWorkerIsrMgrImpl();
    ~RmtWorkerIsrMgrImpl() override = default;

    // Implement pure virtual interface methods
    Result<RmtIsrHandle, RmtRegisterError> registerChannel(
        uint8_t channel_id,
        volatile bool* available_flag,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const ChipsetTiming& timing
    ) override;

    void unregisterChannel(const RmtIsrHandle& handle) override;

    void startTransmission(const RmtIsrHandle& handle) override;

    // Internal helper methods
    bool allocateInterrupt(uint8_t channel_id);
    void deallocateInterrupt(uint8_t channel_id);
    bool isChannelOccupied(uint8_t channel_id) const;
    RmtWorkerIsrData* getIsrData(uint8_t channel_id);

    // Static ISR methods (internal implementation details)
    static void IRAM_ATTR fillNextHalf(uint8_t channel_id);
    static void resetBufferState(uint8_t channel_id);
    static void tx_start(uint8_t channel_id);
    static void IRAM_ATTR sharedGlobalISR(void* arg);
    static FASTLED_FORCE_INLINE void IRAM_ATTR convertByteToRmt(
        uint8_t byte_val,
        const rmt_nibble_lut_t& lut,
        volatile rmt_item32_t* out
    );

private:
    // Helper: Convert nanoseconds to RMT ticks
    // Works for any clock frequency defined by FASTLED_RMT5_CLOCK_HZ
    static inline uint16_t ns_to_ticks(uint32_t ns) {
        // Formula: ticks = (ns * CLOCK_HZ) / 1,000,000,000
        // To avoid overflow for large ns values, we rearrange:
        // ticks = ns / (1,000,000,000 / CLOCK_HZ)
        constexpr uint32_t ONE_GHZ = 1_000_000_000UL;
        constexpr uint32_t ns_per_tick = ONE_GHZ / FASTLED_RMT5_CLOCK_HZ;
        constexpr uint32_t ns_per_tick_half = ns_per_tick / 2;

        // Add half a tick for rounding
        return static_cast<uint16_t>((ns + ns_per_tick_half) / ns_per_tick);
    }

    // ISR data pool - one per hardware channel
    // DRAM attribute for ISR access
    RmtWorkerIsrData mIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP];

    // Interrupt allocation tracking per channel
    bool mInterruptAllocated[SOC_RMT_CHANNELS_PER_GROUP];

    // Global interrupt handle (shared by all channels)
    intr_handle_t mGlobalInterruptHandle;

    // Maximum channel number
    uint8_t mMaxChannel;

    // Allow static methods to access implementation
    friend class RmtWorkerIsrMgr;
};

RmtWorkerIsrMgrImpl::RmtWorkerIsrMgrImpl()
    : mGlobalInterruptHandle(nullptr)
    , mMaxChannel(SOC_RMT_CHANNELS_PER_GROUP)
{
    // ISR data array is default-constructed
    // All mAvailableFlag pointers are nullptr (available state)

    // Initialize interrupt tracking
    for (int i = 0; i < SOC_RMT_CHANNELS_PER_GROUP; i++) {
        mInterruptAllocated[i] = false;
    }

    FL_LOG_RMT("RmtWorkerIsrMgr: Initialized with " << SOC_RMT_CHANNELS_PER_GROUP << " ISR data slots");
}

RmtWorkerIsrMgr& RmtWorkerIsrMgr::getInstance() {
    static RmtWorkerIsrMgrImpl instance;
    return instance;
}

Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgrImpl::registerChannel(
    uint8_t channel_id,
    volatile bool* available_flag,
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

    // Validate availability flag pointer
    if (available_flag == nullptr) {
        FL_WARN("RmtWorkerIsrMgr: Null availability flag pointer for channel " << (int)channel_id);
        return Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::INVALID_CHANNEL,
            "Null availability flag pointer"
        );
    }

    RmtWorkerIsrData* isr_data = &mIsrDataArray[channel_id];

    // Check if channel is already occupied
    if (isr_data->mAvailableFlag != nullptr) {
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

    // Convert timing from nanoseconds to RMT ticks
    uint16_t t1_ticks = ns_to_ticks(timing.T1);
    uint16_t t2_ticks = ns_to_ticks(timing.T2);
    uint16_t t3_ticks = ns_to_ticks(timing.T3);

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
    isr_data->config(available_flag, channel_id, rmt_mem_start, pixel_data_ptr, num_bytes, nibble_lut);

    FL_LOG_RMT("RmtWorkerIsrMgr: Registered and configured worker on channel " << (int)channel_id);

    // Return success with handle
    return Result<RmtIsrHandle, RmtRegisterError>::success(RmtIsrHandle(channel_id));
}

void RmtWorkerIsrMgrImpl::unregisterChannel(const RmtIsrHandle& handle) {
    uint8_t channel_id = handle.channel_id;

    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel_id=" << (int)channel_id << " during unregister");
        return;
    }

    RmtWorkerIsrData* isr_data = &mIsrDataArray[channel_id];

    // Clear availability flag pointer to mark as available
    isr_data->mAvailableFlag = nullptr;

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

void RmtWorkerIsrMgrImpl::startTransmission(const RmtIsrHandle& handle) {
    tx_start(handle.channel_id);
}

bool RmtWorkerIsrMgrImpl::isChannelOccupied(uint8_t channel_id) const {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return false;
    }
    return mIsrDataArray[channel_id].mAvailableFlag != nullptr;
}

RmtWorkerIsrData* RmtWorkerIsrMgrImpl::getIsrData(uint8_t channel_id) {
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        return nullptr;
    }
    return &mIsrDataArray[channel_id];
}

// Convert byte to RMT symbols using nibble lookup table (inline helper)
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorkerIsrMgrImpl::convertByteToRmt(
    uint8_t byte_val,
    const rmt_nibble_lut_t& lut,
    volatile rmt_item32_t* out
) {
    // Copy 4 RMT items from LUT for high nibble (bits 7-4)
    const rmt_item32_t* high_lut = lut[byte_val >> 4];
    out[0].val = high_lut[0].val;
    out[1].val = high_lut[1].val;
    out[2].val = high_lut[2].val;
    out[3].val = high_lut[3].val;

    // Copy 4 RMT items from LUT for low nibble (bits 3-0)
    const rmt_item32_t* low_lut = lut[byte_val & 0x0F];
    out[4].val = low_lut[0].val;
    out[5].val = low_lut[1].val;
    out[6].val = low_lut[2].val;
    out[7].val = low_lut[3].val;
}
FL_DISABLE_WARNING_POP

// Fill next half of RMT buffer (interrupt context)
// OPTIMIZATION: Matches RMT4 approach - no defensive checks, trust the buffer sizing math
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorkerIsrMgrImpl::fillNextHalf(uint8_t channel_id) {
    // Get ISR data for this channel from implementation
    RmtWorkerIsrMgrImpl& impl = static_cast<RmtWorkerIsrMgrImpl&>(getInstance());
    RmtWorkerIsrData* isr_data = impl.getIsrData(channel_id);

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    // Since we're in ISR context and own the buffer state, we can safely cache and update once
    FASTLED_REGISTER int cur = isr_data->mCur;
    FASTLED_REGISTER int num_bytes = isr_data->mNumBytes;
    const uint8_t* pixel_data = isr_data->mPixelData;
    // Cache LUT reference for ISR performance (avoid repeated member access)
    const rmt_nibble_lut_t& lut = isr_data->mNibbleLut;
    // Remember that volatile writes are super fast, volatile reads are super slow.
    volatile FASTLED_REGISTER rmt_item32_t* pItem = isr_data->mRMT_mem_ptr;

    // Calculate buffer constants (matching RmtWorker values)
    #ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
    #define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
    #endif
    #ifndef FASTLED_RMT_MEM_BLOCKS
    #define FASTLED_RMT_MEM_BLOCKS 2
    #endif
    constexpr int MAX_PULSES = FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS;
    constexpr int PULSES_PER_FILL = MAX_PULSES / 2;  // Half buffer

    // Fill PULSES_PER_FILL / 8 bytes (since each byte = 8 pulses)
    // Note: Boundary checking removed - if buffer sizing is correct, overflow is impossible
    // Buffer size: MAX_PULSES = PULSES_PER_FILL * 2 (ping-pong halves)
    constexpr int i_end = PULSES_PER_FILL / 8;

    for (FASTLED_REGISTER int i = 0; i < i_end; i++) {
        if (cur < num_bytes) {
            convertByteToRmt(pixel_data[cur], lut, pItem);
            pItem += 8;
            cur++;
        } else {
            // End marker - zero duration signals end of transmission
            pItem->val = 0;
            pItem++;
        }
    }

    // Write back updated position (single volatile write instead of many)
    isr_data->mCur = cur;

    // Flip to other half (matches RMT4 pattern)
    isr_data->mWhichHalf++;
    if (isr_data->mWhichHalf == 2) {
        pItem = isr_data->mRMT_mem_start;
        isr_data->mWhichHalf = 0;
    }
    isr_data->mRMT_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP

// Reset buffer state after filling both halves
void RmtWorkerIsrMgrImpl::resetBufferState(uint8_t channel_id) {
    RmtWorkerIsrMgrImpl& impl = static_cast<RmtWorkerIsrMgrImpl&>(getInstance());
    RmtWorkerIsrData* isr_data = impl.getIsrData(channel_id);
    isr_data->mWhichHalf = 0;
    isr_data->mRMT_mem_ptr = isr_data->mRMT_mem_start;
}

// Start RMT transmission (called from main thread, not ISR)
void RmtWorkerIsrMgrImpl::tx_start(uint8_t channel_id) {
    // Fill both halves of ping-pong buffer initially (like RMT4)
    fillNextHalf(channel_id);  // Fill half 0
    fillNextHalf(channel_id);  // Fill half 1

    // Reset buffer state before starting transmission
    resetBufferState(channel_id);

    // Use direct register access like RMT4
    // This is platform-specific and based on RMT4's approach
#if defined(CONFIG_IDF_TARGET_ESP32)
    // Reset RMT memory read pointer
    RMT.conf_ch[channel_id].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel_id].conf1.mem_rd_rst = 0;
    RMT.conf_ch[channel_id].conf1.apb_mem_rst = 1;
    RMT.conf_ch[channel_id].conf1.apb_mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + channel_id;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << channel_id) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << channel_id) | (1 << thresh_bit);

    // Start transmission
    RMT.conf_ch[channel_id].conf1.tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // Reset RMT memory read pointer
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + channel_id;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << channel_id) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << channel_id) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[channel_id].conf_update_chn = 1;
    RMT.chnconf0[channel_id].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // Reset RMT memory read pointer
    RMT.tx_conf[channel_id].mem_rd_rst = 1;
    RMT.tx_conf[channel_id].mem_rd_rst = 0;
    RMT.tx_conf[channel_id].mem_rst = 1;
    RMT.tx_conf[channel_id].mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + channel_id;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << channel_id) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << channel_id) | (1 << thresh_bit);

    // Start transmission
    RMT.tx_conf[channel_id].conf_update = 1;
    RMT.tx_conf[channel_id].tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    // Reset RMT memory read pointer
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + channel_id;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << channel_id) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << channel_id) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[channel_id].conf_update_chn = 1;
    RMT.chnconf0[channel_id].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    // Reset RMT memory read pointer
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + channel_id;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << channel_id) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << channel_id) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[channel_id].conf_update_chn = 1;
    RMT.chnconf0[channel_id].tx_start_chn = 1;
#else
#error "RMT5 worker not yet implemented for this ESP32 variant"
#endif
}

// Allocate interrupt for channel
bool RmtWorkerIsrMgrImpl::allocateInterrupt(uint8_t channel_id) {
    // Validate channel ID
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("RmtWorkerIsrMgr: Invalid channel ID during interrupt allocation: " << (int)channel_id);
        return false;
    }

    // Check if already allocated
    if (mInterruptAllocated[channel_id]) {
        FL_LOG_RMT("RmtWorkerIsrMgr: Interrupt already allocated for channel " << (int)channel_id);
        return true;
    }

    FL_LOG_RMT("RmtWorkerIsrMgr: Allocating interrupt for channel " << (int)channel_id);

    // Enable threshold interrupt for this channel using direct register access
    uint32_t thresh_int_bit = 8 + channel_id;  // Bits 8-11 for channels 0-3
    RMT.int_ena.val |= (1 << thresh_int_bit);

    // Allocate shared global ISR (only once for all channels, like RMT4)
    if (mGlobalInterruptHandle == nullptr) {
        FL_LOG_RMT("RmtWorkerIsrMgr: Allocating shared global ISR for all RMT channels");

        esp_err_t ret = esp_intr_alloc(
            ETS_RMT_INTR_SOURCE,
            ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
            &RmtWorkerIsrMgrImpl::sharedGlobalISR,
            nullptr,  // No user data - ISR uses manager's worker registry
            &mGlobalInterruptHandle
        );

        if (ret != ESP_OK) {
            FL_WARN("RmtWorkerIsrMgr: Failed to allocate shared ISR: " << esp_err_to_name(ret) << " (0x" << (int)ret << ")");
            mWorkerRegistry[channel_id] = nullptr;  // Clean up registration
            return false;
        }

        FL_LOG_RMT("RmtWorkerIsrMgr: Shared global ISR allocated successfully (Level 3, ETS_RMT_INTR_SOURCE)");
    }

    // Mark as allocated
    mInterruptAllocated[channel_id] = true;
    return true;
}

// Deallocate interrupt for channel
void RmtWorkerIsrMgrImpl::deallocateInterrupt(uint8_t channel_id) {
    if (channel_id < SOC_RMT_CHANNELS_PER_GROUP && mInterruptAllocated[channel_id]) {
        mInterruptAllocated[channel_id] = false;
        FL_LOG_RMT("RmtWorkerIsrMgr: Deallocated interrupt for channel " << (int)channel_id);
    }
}

// Shared global ISR - processes all channels in one pass (like RMT4)
void IRAM_ATTR RmtWorkerIsrMgrImpl::sharedGlobalISR(void* arg) {
    // Get manager implementation
    RmtWorkerIsrMgrImpl& impl = static_cast<RmtWorkerIsrMgrImpl&>(getInstance());

    // Read interrupt status once - captures all pending channel interrupts atomically
    uint32_t intr_st = RMT.int_st.val;

    // Process all channels in a single pass
    for (uint8_t channel = 0; channel < impl.mMaxChannel; channel++) {
        RmtWorkerIsrData* isr_data = &impl.mIsrDataArray[channel];

        // Skip inactive channels
        if (isr_data->mAvailableFlag == nullptr) {
            continue;
        }

        // Platform-specific bit positions (from RMT4)
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
        int tx_done_bit = channel;
        int tx_next_bit = channel + 8;  // Threshold interrupt
#else
#error "RMT5 worker ISR not yet implemented for this ESP32 variant"
#endif

        // Check threshold interrupt (buffer half empty) - refill needed
        if (intr_st & (1 << tx_next_bit)) {
            fillNextHalf(channel);
            RMT.int_clr.val = (1 << tx_next_bit);
        }

        // Check done interrupt (transmission complete)
        if (intr_st & (1 << tx_done_bit)) {
            // Signal completion by setting worker's availability flag to true
            // This allows the worker to detect completion and unregister
            if (isr_data->mAvailableFlag != nullptr) {
                *(isr_data->mAvailableFlag) = true;
            }
            RMT.int_clr.val = (1 << tx_done_bit);
        }
    }
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
