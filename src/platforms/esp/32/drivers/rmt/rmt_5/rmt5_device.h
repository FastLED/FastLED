#pragma once

// ok no namespace fl

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/stdint.h"

FL_EXTERN_C_BEGIN
#include "soc/rmt_struct.h"
#include "esp_attr.h"
FL_EXTERN_C_END

/**
 * RMT5 Device Hardware Abstraction Macros
 *
 * Purpose:
 * - Zero-overhead platform-abstracted RMT register access
 * - Macros guarantee inline expansion with no function call overhead
 * - Direct access to global RMT singleton (ESP-IDF hardware abstraction)
 *
 * Design:
 * - Pure preprocessor macros for guaranteed inlining in ISR context
 * - Platform detection via CONFIG_IDF_TARGET_* macros
 * - Uppercase naming convention for macro visibility
 */

// === Memory Reset Operations ===

/**
 * Reset RMT channel memory read pointer
 * Must be called before starting transmission
 *
 * @param channel_id Hardware RMT channel (0 to SOC_RMT_CHANNELS_PER_GROUP-1)
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_RESET_MEMORY_READ_POINTER(channel_id) \
    RMT.conf_ch[channel_id].conf1.mem_rd_rst = 1; \
    RMT.conf_ch[channel_id].conf1.mem_rd_rst = 0; \
    RMT.conf_ch[channel_id].conf1.apb_mem_rst = 1; \
    RMT.conf_ch[channel_id].conf1.apb_mem_rst = 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_RESET_MEMORY_READ_POINTER(channel_id) \
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 1; \
    RMT.chnconf0[channel_id].mem_rd_rst_chn = 0; \
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 1; \
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 0; \
    RMT.chnconf0[channel_id].conf_update_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_RESET_MEMORY_READ_POINTER(channel_id) \
    RMT.tx_conf[channel_id].mem_rd_rst = 1; \
    RMT.tx_conf[channel_id].mem_rd_rst = 0; \
    RMT.tx_conf[channel_id].mem_rst = 1; \
    RMT.tx_conf[channel_id].mem_rst = 0
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

// === Interrupt Control ===

/**
 * Get TX done interrupt bit position
 * Platform-specific bit positions in interrupt status registers
 *
 * CRITICAL: Interrupt bit positions vary by platform!
 * - ESP32 Classic & S2: TX done = channel * 3
 * - ESP32-S3/C3/C6/H2/C5/P4: TX done = channel
 *
 * @param channel_id Hardware RMT channel
 * @return Bit position in interrupt registers
 */
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
#define RMT5_TX_DONE_BIT(channel_id) ((channel_id) * 3)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_TX_DONE_BIT(channel_id) (channel_id)
#else
#define RMT5_TX_DONE_BIT(channel_id) (channel_id)
#endif

/**
 * Get TX threshold interrupt bit position
 * Platform-specific bit positions in interrupt status registers
 *
 * CRITICAL: Interrupt bit positions vary by platform!
 * - ESP32 Classic: threshold = channel + 24
 * - ESP32-S2: threshold = channel + 12
 * - ESP32-S3/C3/C6/H2/C5/P4: threshold = channel + 8
 *
 * @param channel_id Hardware RMT channel
 * @return Bit position in interrupt registers
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_TX_THRESHOLD_BIT(channel_id) ((channel_id) + 24)
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define RMT5_TX_THRESHOLD_BIT(channel_id) ((channel_id) + 12)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_TX_THRESHOLD_BIT(channel_id) ((channel_id) + 8)
#else
#define RMT5_TX_THRESHOLD_BIT(channel_id) ((channel_id) + 8)
#endif

/**
 * Read interrupt status register atomically
 * @return Interrupt status bits (each bit represents a channel or threshold)
 */
#define RMT5_READ_INTERRUPT_STATUS() (RMT.int_st.val)

/**
 * Clear interrupt flags for channel
 *
 * @param channel_id Hardware RMT channel
 * @param clear_done Clear TX done interrupt
 * @param clear_threshold Clear threshold interrupt
 */
#define RMT5_CLEAR_INTERRUPTS(channel_id, clear_done, clear_threshold) \
    do { \
        uint32_t clear_mask = 0; \
        if (clear_done) { \
            clear_mask |= (1 << RMT5_TX_DONE_BIT(channel_id)); \
        } \
        if (clear_threshold) { \
            clear_mask |= (1 << RMT5_TX_THRESHOLD_BIT(channel_id)); \
        } \
        if (clear_mask) { \
            RMT.int_clr.val = clear_mask; \
        } \
    } while(0)

/**
 * Enable interrupts for channel
 *
 * @param channel_id Hardware RMT channel
 * @param enable_done Enable TX done interrupt
 * @param enable_threshold Enable threshold interrupt
 */
#define RMT5_ENABLE_INTERRUPTS(channel_id, enable_done, enable_threshold) \
    do { \
        uint32_t enable_mask = 0; \
        if (enable_done) { \
            enable_mask |= (1 << RMT5_TX_DONE_BIT(channel_id)); \
        } \
        if (enable_threshold) { \
            enable_mask |= (1 << RMT5_TX_THRESHOLD_BIT(channel_id)); \
        } \
        if (enable_mask) { \
            RMT.int_ena.val |= enable_mask; \
        } \
    } while(0)

/**
 * Enable threshold interrupt for channel using direct register access
 * Used during interrupt allocation
 *
 * @param channel_id Hardware RMT channel
 */
#define RMT5_ENABLE_THRESHOLD_INTERRUPT(channel_id) \
    do { \
        RMT.int_ena.val |= (1 << RMT5_TX_THRESHOLD_BIT(channel_id)); \
    } while(0)

/**
 * Disable threshold interrupt for channel using direct register access
 * Used during interrupt deallocation
 *
 * @param channel_id Hardware RMT channel
 */
#define RMT5_DISABLE_THRESHOLD_INTERRUPT(channel_id) \
    do { \
        RMT.int_ena.val &= ~(1 << RMT5_TX_THRESHOLD_BIT(channel_id)); \
    } while(0)

// === Transmission Control ===

/**
 * Enable RMT channel for transmission
 * Must be called before channel can transmit (alternative to rmt_enable() API)
 *
 * @param channel_id Hardware RMT channel
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_ENABLE_TX_CHANNEL(channel_id) \
    RMT.conf_ch[channel_id].conf1.tx_conti_mode = 0; \
    RMT.conf_ch[channel_id].conf1.mem_tx_wrap_en = 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_ENABLE_TX_CHANNEL(channel_id) \
    RMT.chnconf0[channel_id].mem_tx_wrap_en_chn = 0; \
    RMT.chnconf0[channel_id].conf_update_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_ENABLE_TX_CHANNEL(channel_id) \
    RMT.tx_conf[channel_id].mem_tx_wrap_en = 0; \
    RMT.tx_conf[channel_id].conf_update = 1
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Start RMT transmission for channel
 * Triggers hardware to begin reading from RMT memory and transmitting
 *
 * @param channel_id Hardware RMT channel
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_START_TRANSMISSION(channel_id) \
    RMT.conf_ch[channel_id].conf1.tx_start = 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_START_TRANSMISSION(channel_id) \
    RMT.chnconf0[channel_id].tx_start_chn = 1; \
    RMT.chnconf0[channel_id].conf_update_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_START_TRANSMISSION(channel_id) \
    RMT.tx_conf[channel_id].conf_update = 1; \
    RMT.tx_conf[channel_id].tx_start = 1
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Stop RMT transmission for channel
 * Forces hardware to stop transmitting and return to idle state
 *
 * @param channel_id Hardware RMT channel
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_STOP_TRANSMISSION(channel_id) \
    RMT.conf_ch[channel_id].conf1.tx_start = 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_STOP_TRANSMISSION(channel_id) \
    RMT.chnconf0[channel_id].tx_start_chn = 0; \
    RMT.chnconf0[channel_id].conf_update_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_STOP_TRANSMISSION(channel_id) \
    RMT.tx_conf[channel_id].tx_start = 0; \
    RMT.tx_conf[channel_id].conf_update = 1
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Hard reset RMT channel to force state machine back to idle
 * Toggles memory read reset bit to force hardware state machine reset
 * This is needed when the state machine gets stuck (ESP32-C6 quirk)
 *
 * @param channel_id Hardware RMT channel
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_HARD_RESET_CHANNEL(channel_id) \
    do { \
        RMT.conf_ch[channel_id].conf1.mem_rd_rst = 1; \
        RMT.conf_ch[channel_id].conf1.mem_rd_rst = 0; \
    } while(0)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_HARD_RESET_CHANNEL(channel_id) \
    do { \
        RMT.chnconf0[channel_id].mem_rd_rst_chn = 1; \
        RMT.chnconf0[channel_id].conf_update_chn = 1; \
        RMT.chnconf0[channel_id].mem_rd_rst_chn = 0; \
        RMT.chnconf0[channel_id].conf_update_chn = 1; \
    } while(0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_HARD_RESET_CHANNEL(channel_id) \
    do { \
        RMT.tx_conf[channel_id].mem_rd_rst = 1; \
        RMT.tx_conf[channel_id].conf_update = 1; \
        RMT.tx_conf[channel_id].mem_rd_rst = 0; \
        RMT.tx_conf[channel_id].conf_update = 1; \
    } while(0)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

// === State Inspection ===

/**
 * Get RMT channel state machine state
 * Returns: 0=idle, 1=sending, 2=reading memory, 3=reserved
 *
 * @param channel_id Hardware RMT channel
 * @return State value (0-3)
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_GET_STATE(channel_id) \
    ((RMT.status_ch[channel_id] >> 22) & 0x7)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_GET_STATE(channel_id) \
    (RMT.chnstatus[channel_id].state_chn)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_GET_STATE(channel_id) \
    (RMT.tx_status[channel_id].state)
#else
#define RMT5_GET_STATE(channel_id) 0
#endif

/**
 * Check if RMT channel is idle
 * Returns true if state machine is in idle state (0)
 *
 * @param channel_id Hardware RMT channel
 * @return true if idle, false otherwise
 */
#define RMT5_IS_IDLE(channel_id) \
    (RMT5_GET_STATE(channel_id) == 0)

// === Threshold Configuration ===

/**
 * Set TX threshold limit for channel
 * When RMT memory fill level drops below this, threshold interrupt fires
 *
 * @param channel_id Hardware RMT channel
 * @param threshold Threshold value (number of RMT items)
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_THRESHOLD_LIMIT(channel_id, threshold) \
    RMT.tx_lim_ch[channel_id].limit = (threshold)
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define RMT5_SET_THRESHOLD_LIMIT(channel_id, threshold) \
    RMT.chn_tx_lim[channel_id].tx_lim_chn = (threshold)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_THRESHOLD_LIMIT(channel_id, threshold) \
    RMT.tx_lim[channel_id].limit = (threshold)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_THRESHOLD_LIMIT(channel_id, threshold) \
    RMT.chn_tx_lim[channel_id].tx_lim_chn = (threshold)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

// === Status Reading ===

/**
 * Get current RMT memory read address for channel
 * Returns the memory address offset currently being read by the transmitter
 * This indicates the current position in the RMT memory buffer during transmission
 *
 * Futre optimization for isr buffer refill.
 *
 * @param channel_id Hardware RMT channel
 * @return Current read address offset (number of RMT items from buffer start)

 *| Platform | Register Access | Bit Width | Range |
 *|----------|----------------|-----------|-------|
 *| ESP32 (classic) | `RMT.status_ch[channel_id]` bits [21:12] | 10 bits | 0-1023 |
 *| ESP32-S3 | `RMT.chnstatus[channel_id].mem_raddr_ex_chn` | 10 bits | 0-1023 |
 *| ESP32-C3 | `RMT.tx_status[channel_id].mem_raddr_ex` | 9 bits | 0-511 |
 *| ESP32-C6 | `RMT.chnstatus[channel_id].mem_raddr_ex_chn` | 9 bits | 0-511 |
 *| ESP32-H2 | `RMT.chnstatus[channel_id].mem_raddr_ex_chn` | 9 bits | 0-511 |
 *| ESP32-C5 | `RMT.chnstatus[channel_id].mem_raddr_ex_chn` | 9 bits | 0-511 |
 *| ESP32-P4 | `RMT.chnstatus[channel_id].mem_raddr_ex_chn` | 10 bits | 0-1023 |
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32 classic: status_ch is plain uint32_t, extract bits [21:12]
#define RMT5_GET_READ_ADDRESS(channel_id) \
    ((RMT.status_ch[channel_id] >> 12) & 0x3FF)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)
// ESP32-S3, ESP32-P4: chnstatus has mem_raddr_ex_chn field (10 bits)
#define RMT5_GET_READ_ADDRESS(channel_id) \
    (RMT.chnstatus[channel_id].mem_raddr_ex_chn)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3: tx_status has mem_raddr_ex field (9 bits)
#define RMT5_GET_READ_ADDRESS(channel_id) \
    (RMT.tx_status[channel_id].mem_raddr_ex)
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
// ESP32-C6, ESP32-H2, ESP32-C5: chnstatus has mem_raddr_ex_chn field (9 bits)
#define RMT5_GET_READ_ADDRESS(channel_id) \
    (RMT.chnstatus[channel_id].mem_raddr_ex_chn)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Get RMT memory read address (alias for RMT5_GET_READ_ADDRESS)
 * Returns current position of hardware read pointer
 *
 * @param channel_id Hardware RMT channel
 * @return Read address (0 to buffer_size-1)
 */
#define RMT5_GET_MEM_READ_ADDR(channel_id) RMT5_GET_READ_ADDRESS(channel_id)

/**
 * Check if RMT memory buffer is empty
 * Returns true when hardware has consumed all data
 *
 * @param channel_id Hardware RMT channel
 * @return true if buffer empty, false otherwise
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32: Extract from status_ch bit 25 (mem_empty)
#define RMT5_IS_MEM_EMPTY(channel_id) \
    (((RMT.status_ch[channel_id] >> 25) & 0x1) != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
// ESP32-S3/C6/H2/C5/P4: chnstatus.mem_empty_chn
#define RMT5_IS_MEM_EMPTY(channel_id) \
    (RMT.chnstatus[channel_id].mem_empty_chn != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3: tx_status.mem_empty
#define RMT5_IS_MEM_EMPTY(channel_id) \
    (RMT.tx_status[channel_id].mem_empty != 0)
#else
#define RMT5_IS_MEM_EMPTY(channel_id) 0
#endif

/**
 * Check if APB memory write error occurred
 * Available on newer platforms (S3, C3, C6, H2, C5, P4)
 *
 * @param channel_id Hardware RMT channel
 * @return true if write error occurred, false otherwise
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32: Not available
#define RMT5_HAS_MEM_WR_ERROR(channel_id) 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
// ESP32-S3/C6/H2/C5/P4: chnstatus.apb_mem_wr_err_chn
#define RMT5_HAS_MEM_WR_ERROR(channel_id) \
    (RMT.chnstatus[channel_id].apb_mem_wr_err_chn != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3: tx_status.apb_mem_wr_err
#define RMT5_HAS_MEM_WR_ERROR(channel_id) \
    (RMT.tx_status[channel_id].apb_mem_wr_err != 0)
#else
#define RMT5_HAS_MEM_WR_ERROR(channel_id) 0
#endif

/**
 * Check if APB memory read error occurred
 * Available on newer platforms (S3, C3, C6, H2, C5, P4)
 *
 * @param channel_id Hardware RMT channel
 * @return true if read error occurred, false otherwise
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32: Not available
#define RMT5_HAS_MEM_RD_ERROR(channel_id) 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
// ESP32-S3/C6/H2/C5/P4: chnstatus.apb_mem_rd_err_chn
#define RMT5_HAS_MEM_RD_ERROR(channel_id) \
    (RMT.chnstatus[channel_id].apb_mem_rd_err_chn != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3: tx_status.apb_mem_rd_err
#define RMT5_HAS_MEM_RD_ERROR(channel_id) \
    (RMT.tx_status[channel_id].apb_mem_rd_err != 0)
#else
#define RMT5_HAS_MEM_RD_ERROR(channel_id) 0
#endif

// === Configuration ===

/**
 * Set idle output level (GPIO state when idle)
 *
 * @param channel_id Hardware RMT channel
 * @param level 0 or 1
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_IDLE_OUTPUT_LEVEL(channel_id, level) \
    RMT.conf_ch[channel_id].conf1.idle_out_lv = (level)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_IDLE_OUTPUT_LEVEL(channel_id, level) \
    RMT.chnconf0[channel_id].idle_out_lv_chn = (level)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_IDLE_OUTPUT_LEVEL(channel_id, level) \
    RMT.tx_conf[channel_id].idle_out_lv = (level)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Enable/disable idle output
 *
 * @param channel_id Hardware RMT channel
 * @param enable true to enable, false to disable
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_IDLE_OUTPUT_ENABLE(channel_id, enable) \
    RMT.conf_ch[channel_id].conf1.idle_out_en = (enable)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_IDLE_OUTPUT_ENABLE(channel_id, enable) \
    RMT.chnconf0[channel_id].idle_out_en_chn = (enable)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_IDLE_OUTPUT_ENABLE(channel_id, enable) \
    RMT.tx_conf[channel_id].idle_out_en = (enable)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Set RMT clock divider
 * RMT clock = APB_CLK / (div_cnt + 1)
 *
 * @param channel_id Hardware RMT channel
 * @param divider Clock divider value (0-255)
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_CLOCK_DIVIDER(channel_id, divider) \
    RMT.conf_ch[channel_id].conf0.div_cnt = (divider)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_CLOCK_DIVIDER(channel_id, divider) \
    RMT.chnconf0[channel_id].div_cnt_chn = (divider)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_CLOCK_DIVIDER(channel_id, divider) \
    RMT.tx_conf[channel_id].div_cnt = (divider)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Enable/disable carrier wave
 * Used for IR transmission protocols
 *
 * @param channel_id Hardware RMT channel
 * @param enable true to enable carrier, false to disable
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_CARRIER_ENABLE(channel_id, enable) \
    RMT.conf_ch[channel_id].conf0.carrier_en = (enable)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_CARRIER_ENABLE(channel_id, enable) \
    RMT.chnconf0[channel_id].carrier_en_chn = (enable)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_CARRIER_ENABLE(channel_id, enable) \
    RMT.tx_conf[channel_id].carrier_en = (enable)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Set carrier output level
 *
 * @param channel_id Hardware RMT channel
 * @param level 0 or 1
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_CARRIER_OUTPUT_LEVEL(channel_id, level) \
    RMT.conf_ch[channel_id].conf0.carrier_out_lv = (level)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_CARRIER_OUTPUT_LEVEL(channel_id, level) \
    RMT.chnconf0[channel_id].carrier_out_lv_chn = (level)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_CARRIER_OUTPUT_LEVEL(channel_id, level) \
    RMT.tx_conf[channel_id].carrier_out_lv = (level)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

// === Configuration Update ===

/**
 * Trigger configuration update
 * On newer platforms (S3, C3, C6, H2, C5, P4), configuration changes
 * require setting conf_update bit to take effect
 *
 * @param channel_id Hardware RMT channel
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32: No-op (not required)
#define RMT5_UPDATE_CONFIG(channel_id) ((void)0)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_UPDATE_CONFIG(channel_id) \
    RMT.chnconf0[channel_id].conf_update_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_UPDATE_CONFIG(channel_id) \
    RMT.tx_conf[channel_id].conf_update = 1
#else
#define RMT5_UPDATE_CONFIG(channel_id) ((void)0)
#endif

// === Interrupt Status Reading ===

/**
 * Get raw TX done interrupt status (unmasked)
 *
 * @param channel_id Hardware RMT channel
 * @return true if interrupt is pending, false otherwise
 */
#define RMT5_GET_RAW_TX_DONE_INT(channel_id) \
    ((RMT.int_raw.val & (1 << RMT5_TX_DONE_BIT(channel_id))) != 0)

/**
 * Get raw TX threshold interrupt status (unmasked)
 *
 * @param channel_id Hardware RMT channel
 * @return true if interrupt is pending, false otherwise
 */
#define RMT5_GET_RAW_TX_THRESHOLD_INT(channel_id) \
    ((RMT.int_raw.val & (1 << RMT5_TX_THRESHOLD_BIT(channel_id))) != 0)

/**
 * Check if TX done interrupt is active (masked by enable)
 * This checks int_st register which reflects actual interrupt trigger state
 *
 * @param channel_id Hardware RMT channel
 * @return true if TX done interrupt is active, false otherwise
 */
#define RMT5_IS_TX_DONE(channel_id) \
    ((RMT.int_st.val & (1 << RMT5_TX_DONE_BIT(channel_id))) != 0)

/**
 * Check if TX threshold interrupt is active (masked by enable)
 * This checks int_st register which reflects actual interrupt trigger state
 *
 * @param channel_id Hardware RMT channel
 * @return true if TX threshold interrupt is active, false otherwise
 */
#define RMT5_IS_TX_THRESHOLD(channel_id) \
    ((RMT.int_st.val & (1 << RMT5_TX_THRESHOLD_BIT(channel_id))) != 0)

// === Advanced Control ===

/**
 * Get continuous transmission mode status
 * Returns true if continuous mode is enabled
 *
 * @param channel_id Hardware RMT channel
 * @return true if continuous mode enabled, false otherwise
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_GET_CONTINUOUS_MODE(channel_id) \
    (RMT.conf_ch[channel_id].conf1.tx_conti_mode != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_GET_CONTINUOUS_MODE(channel_id) \
    (RMT.chnconf0[channel_id].tx_conti_mode_chn != 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_GET_CONTINUOUS_MODE(channel_id) \
    (RMT.tx_conf[channel_id].tx_conti_mode != 0)
#else
#define RMT5_GET_CONTINUOUS_MODE(channel_id) 0
#endif

/**
 * Set continuous transmission mode
 * When enabled, transmission loops continuously
 *
 * @param channel_id Hardware RMT channel
 * @param enable true to enable continuous mode, false for one-shot
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_CONTINUOUS_MODE(channel_id, enable) \
    RMT.conf_ch[channel_id].conf1.tx_conti_mode = (enable)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_CONTINUOUS_MODE(channel_id, enable) \
    do { \
        RMT.chnconf0[channel_id].tx_conti_mode_chn = (enable); \
        RMT.chnconf0[channel_id].conf_update_chn = 1; \
    } while(0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_CONTINUOUS_MODE(channel_id, enable) \
    do { \
        RMT.tx_conf[channel_id].tx_conti_mode = (enable); \
        RMT.tx_conf[channel_id].conf_update = 1; \
    } while(0)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

/**
 * Set reference clock source
 *
 * @param channel_id Hardware RMT channel
 * @param always_on true for APB clock, false for REF_TICK
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
#define RMT5_SET_REF_ALWAYS_ON(channel_id, always_on) \
    RMT.conf_ch[channel_id].conf1.ref_always_on = (always_on)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT5_SET_REF_ALWAYS_ON(channel_id, always_on) \
    do { \
        RMT.chnconf0[channel_id].ref_always_on_chn = (always_on); \
        RMT.chnconf0[channel_id].conf_update_chn = 1; \
    } while(0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_SET_REF_ALWAYS_ON(channel_id, always_on) \
    do { \
        RMT.tx_conf[channel_id].ref_always_on = (always_on); \
        RMT.tx_conf[channel_id].conf_update = 1; \
    } while(0)
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

#endif // FASTLED_RMT5

#endif // ESP32
