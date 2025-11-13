#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"

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
    RMT.chnconf0[channel_id].apb_mem_rst_chn = 0
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
 * Read interrupt status register atomically
 * @return Interrupt status bits (each bit represents a channel or threshold)
 */
#define RMT5_READ_INTERRUPT_STATUS() (RMT.int_st.val)

/**
 * Clear interrupt flags for channel
 *
 * @param channel_id Hardware RMT channel
 * @param clear_done Clear TX done interrupt (bit: channel_id)
 * @param clear_threshold Clear threshold interrupt (bit: 8 + channel_id)
 */
#define RMT5_CLEAR_INTERRUPTS(channel_id, clear_done, clear_threshold) \
    { \
        uint32_t clear_mask = 0; \
        if (clear_done) { \
            clear_mask |= (1 << (channel_id)); \
        } \
        if (clear_threshold) { \
            clear_mask |= (1 << (8 + (channel_id))); \
        } \
        if (clear_mask) { \
            RMT.int_clr.val = clear_mask; \
        } \
    }

/**
 * Enable interrupts for channel
 *
 * @param channel_id Hardware RMT channel
 * @param enable_done Enable TX done interrupt
 * @param enable_threshold Enable threshold interrupt
 */
#define RMT5_ENABLE_INTERRUPTS(channel_id, enable_done, enable_threshold) \
    { \
        uint32_t enable_mask = 0; \
        if (enable_done) { \
            enable_mask |= (1 << (channel_id)); \
        } \
        if (enable_threshold) { \
            enable_mask |= (1 << (8 + (channel_id))); \
        } \
        if (enable_mask) { \
            RMT.int_ena.val |= enable_mask; \
        } \
    }

/**
 * Enable threshold interrupt for channel using direct register access
 * Used during interrupt allocation
 *
 * @param channel_id Hardware RMT channel
 */
#define RMT5_ENABLE_THRESHOLD_INTERRUPT(channel_id) \
    { \
        uint32_t thresh_int_bit = 8 + (channel_id); \
        RMT.int_ena.val |= (1 << thresh_int_bit); \
    }

// === Transmission Control ===

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
    RMT.chnconf0[channel_id].conf_update_chn = 1; \
    RMT.chnconf0[channel_id].tx_start_chn = 1
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT5_START_TRANSMISSION(channel_id) \
    RMT.tx_conf[channel_id].conf_update = 1; \
    RMT.tx_conf[channel_id].tx_start = 1
#else
#error "RMT5 device not yet implemented for this ESP32 variant"
#endif

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

#endif // FASTLED_RMT5

#endif // ESP32
