/*
  FastLED — NRF52 ISR Implementation
  -----------------------------------
  NRF52-specific implementation of the cross-platform ISR API.
  Supports NRF52832, NRF52833, NRF52840 (ARM Cortex-M4F).

  Uses Nordic nrfx HAL (Hardware Abstraction Layer) for timer and GPIOTE interrupts.

  Thread Safety & Critical Sections:
  - This implementation uses direct NVIC access (assumes NO SoftDevice)
  - When BLE SoftDevice is enabled, replace NVIC_* calls with sd_nvic_* equivalents
  - Nordic SDK provides CRITICAL_REGION_ENTER/EXIT macros for critical sections
  - CRITICAL_REGION uses ARM CPSID/CPSIE instructions (disables all interrupts)
  - For finer control, use BASEPRI register on Cortex-M4 (selective interrupt masking)
  - Avoid long critical sections to prevent interrupt latency issues

  SoftDevice Compatibility:
  - SoftDevice reserves NVIC priorities 0, 1, and 4 for BLE stack
  - Application interrupts at priority 0-3 CANNOT call SoftDevice APIs
  - Application interrupts at priority 5-7 CAN call SoftDevice APIs
  - Use software interrupts (SWIRQ) to defer work from high-priority ISRs

  License: MIT (FastLED)
*/

#pragma once

#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833) || \
    defined(FL_IS_NRF52) || defined(FL_IS_NRF52832) || defined(FL_IS_NRF52833) || defined(FL_IS_NRF52840)

#include "fl/isr.h"
#include "fl/compiler_control.h"
#include "fl/stl/assert.h"
#include "fl/dbg.h"

// Include NRF52 SDK headers
FL_EXTERN_C_BEGIN
#include "nrf.h"
#include "nrf_timer.h"
#include "nrf_gpiote.h"
#include "nrf_gpio.h"
FL_EXTERN_C_END

namespace fl {
namespace isr {
namespace platform {

// =============================================================================
// Platform-Specific Handle Storage
// =============================================================================

struct nrf52_isr_handle_data {
    NRF_TIMER_Type* timer_instance;  // Timer peripheral instance (TIMER0-4)
    uint8_t timer_channel;           // Timer compare channel (0-5)
    IRQn_Type timer_irq;             // Timer IRQ number
    int8_t gpiote_channel;           // GPIOTE channel (-1 if not GPIO)
    uint8_t gpio_pin;                // GPIO pin number (0xFF if not GPIO)
    bool is_timer;                   // true = timer ISR, false = external ISR
    bool is_enabled;                 // Current enable state
    isr_handler_t user_handler;      // User handler function
    void* user_data;                 // User context

    nrf52_isr_handle_data()
        : timer_instance(nullptr)
        , timer_channel(0)
        , timer_irq(static_cast<IRQn_Type>(0))
        , gpiote_channel(-1)
        , gpio_pin(0xFF)
        , is_timer(false)
        , is_enabled(true)
        , user_handler(nullptr)
        , user_data(nullptr)
    {}
};

// Platform ID for NRF52
constexpr uint8_t NRF52_PLATFORM_ID = 3;

// Maximum number of timer instances (TIMER0-TIMER4)
constexpr uint8_t MAX_TIMER_INSTANCES = 5;

// Maximum GPIOTE channels
constexpr uint8_t MAX_GPIOTE_CHANNELS = 8;

// Track allocated timers and GPIOTE channels
static bool timer_allocated[MAX_TIMER_INSTANCES][6] = {};  // [instance][channel]
static bool gpiote_allocated[MAX_GPIOTE_CHANNELS] = {};

// Storage for timer handles (to allow ISR to find user handler)
static nrf52_isr_handle_data* timer_handles[MAX_TIMER_INSTANCES][6] = {};
static nrf52_isr_handle_data* gpiote_handles[MAX_GPIOTE_CHANNELS] = {};

// =============================================================================
// Helper Functions
// =============================================================================

// Get timer instance index (0-4) from pointer
static int get_timer_index(NRF_TIMER_Type* timer) {
    if (timer == NRF_TIMER0) return 0;
    if (timer == NRF_TIMER1) return 1;
    if (timer == NRF_TIMER2) return 2;
#ifdef NRF_TIMER3
    if (timer == NRF_TIMER3) return 3;
#endif
#ifdef NRF_TIMER4
    if (timer == NRF_TIMER4) return 4;
#endif
    return -1;
}

// Get timer pointer from index
static NRF_TIMER_Type* get_timer_instance(int index) {
    switch (index) {
        case 0: return NRF_TIMER0;
        case 1: return NRF_TIMER1;
        case 2: return NRF_TIMER2;
#ifdef NRF_TIMER3
        case 3: return NRF_TIMER3;
#endif
#ifdef NRF_TIMER4
        case 4: return NRF_TIMER4;
#endif
        default: return nullptr;
    }
}

// Get timer IRQ from index
static IRQn_Type get_timer_irq(int index) {
    switch (index) {
        case 0: return TIMER0_IRQn;
        case 1: return TIMER1_IRQn;
        case 2: return TIMER2_IRQn;
#ifdef TIMER3_IRQn
        case 3: return TIMER3_IRQn;
#endif
#ifdef TIMER4_IRQn
        case 4: return TIMER4_IRQn;
#endif
        default: return static_cast<IRQn_Type>(0);
    }
}

// Get max channels for timer instance
static uint8_t get_timer_max_channels(int index) {
    // TIMER0-2 have 4 channels, TIMER3-4 have 6 channels
    return (index >= 3) ? 6 : 4;
}

// Allocate a timer channel
static bool allocate_timer_channel(int& timer_idx, uint8_t& channel) {
    for (int i = 0; i < MAX_TIMER_INSTANCES; i++) {
        uint8_t max_channels = get_timer_max_channels(i);
        for (uint8_t ch = 0; ch < max_channels; ch++) {
            if (!timer_allocated[i][ch]) {
                timer_allocated[i][ch] = true;
                timer_idx = i;
                channel = ch;
                return true;
            }
        }
    }
    return false;  // No free timer channels
}

// Free a timer channel
static void free_timer_channel(int timer_idx, uint8_t channel) {
    if (timer_idx >= 0 && timer_idx < MAX_TIMER_INSTANCES && channel < 6) {
        timer_allocated[timer_idx][channel] = false;
        timer_handles[timer_idx][channel] = nullptr;
    }
}

// Allocate a GPIOTE channel
static int8_t allocate_gpiote_channel() {
    for (uint8_t i = 0; i < MAX_GPIOTE_CHANNELS; i++) {
        if (!gpiote_allocated[i]) {
            gpiote_allocated[i] = true;
            return i;
        }
    }
    return -1;  // No free GPIOTE channels
}

// Free a GPIOTE channel
static void free_gpiote_channel(int8_t channel) {
    if (channel >= 0 && channel < MAX_GPIOTE_CHANNELS) {
        gpiote_allocated[channel] = false;
        gpiote_handles[channel] = nullptr;
    }
}

// Map ISR priority (1-7) to NVIC priority (0-7, lower = higher)
// ISR priority 1 (low) -> NVIC 6, ISR priority 2 -> NVIC 5, ..., ISR priority 7 (max) -> NVIC 2
// Reserve NVIC 0-1 for SoftDevice (BLE stack)
//
// SoftDevice Compatibility Notes (when BLE is enabled):
// - SoftDevice uses NVIC priorities 0, 1, and 4
// - Application can safely use priorities 2, 3, 5, 6, 7
// - Interrupts at priority 0-3 CANNOT call SoftDevice APIs (use defer to lower priority)
// - Interrupts at priority 5-7 CAN call SoftDevice APIs
// - When SoftDevice is enabled, use sd_nvic_* functions instead of NVIC_* functions
// - This implementation assumes NO SoftDevice (uses direct NVIC access)
static uint8_t map_priority_to_nvic(uint8_t isr_priority) {
    // Clamp to valid range
    if (isr_priority < 1) isr_priority = 1;
    if (isr_priority > 7) isr_priority = 7;

    // Map: ISR 1->NVIC 6, ISR 2->NVIC 5, ..., ISR 7->NVIC 2
    // This mapping avoids NVIC 0-1 (SoftDevice high priority) and NVIC 7 (lowest)
    return 8 - isr_priority;
}

// =============================================================================
// Timer ISR Handlers
// =============================================================================

// Map channel index to timer event enum
static nrf_timer_event_t get_timer_event(uint8_t channel) {
    // Use proper enum values defined by Nordic SDK (offsetof-based)
    switch (channel) {
        case 0: return NRF_TIMER_EVENT_COMPARE0;
        case 1: return NRF_TIMER_EVENT_COMPARE1;
        case 2: return NRF_TIMER_EVENT_COMPARE2;
        case 3: return NRF_TIMER_EVENT_COMPARE3;
#ifdef NRF_TIMER_EVENT_COMPARE4
        case 4: return NRF_TIMER_EVENT_COMPARE4;
#endif
#ifdef NRF_TIMER_EVENT_COMPARE5
        case 5: return NRF_TIMER_EVENT_COMPARE5;
#endif
        default: return NRF_TIMER_EVENT_COMPARE0;  // Fallback (should never happen)
    }
}

// Common timer interrupt handler
static void timer_interrupt_handler(int timer_idx, uint8_t channel) {
    NRF_TIMER_Type* timer = get_timer_instance(timer_idx);
    if (!timer) return;

    nrf_timer_event_t event = get_timer_event(channel);

    // Check if this channel triggered the interrupt
    if (nrf_timer_event_check(timer, event)) {
        nrf_timer_event_clear(timer, event);

        nrf52_isr_handle_data* handle = timer_handles[timer_idx][channel];
        if (handle && handle->user_handler) {
            handle->user_handler(handle->user_data);
        }
    }
}

// Timer ISR wrappers for each instance
extern "C" {
    void TIMER0_IRQHandler(void) {
        for (uint8_t ch = 0; ch < 4; ch++) {
            if (timer_handles[0][ch]) {
                timer_interrupt_handler(0, ch);
            }
        }
    }

    void TIMER1_IRQHandler(void) {
        for (uint8_t ch = 0; ch < 4; ch++) {
            if (timer_handles[1][ch]) {
                timer_interrupt_handler(1, ch);
            }
        }
    }

    void TIMER2_IRQHandler(void) {
        for (uint8_t ch = 0; ch < 4; ch++) {
            if (timer_handles[2][ch]) {
                timer_interrupt_handler(2, ch);
            }
        }
    }

#ifdef TIMER3_IRQn
    void TIMER3_IRQHandler(void) {
        for (uint8_t ch = 0; ch < 6; ch++) {
            if (timer_handles[3][ch]) {
                timer_interrupt_handler(3, ch);
            }
        }
    }
#endif

#ifdef TIMER4_IRQn
    void TIMER4_IRQHandler(void) {
        for (uint8_t ch = 0; ch < 6; ch++) {
            if (timer_handles[4][ch]) {
                timer_interrupt_handler(4, ch);
            }
        }
    }
#endif

    // Map GPIOTE channel index to event enum
    static nrf_gpiote_event_t get_gpiote_event(uint8_t channel) {
        // Use proper enum values defined by Nordic SDK (offsetof-based)
        switch (channel) {
            case 0: return NRF_GPIOTE_EVENT_IN_0;
            case 1: return NRF_GPIOTE_EVENT_IN_1;
            case 2: return NRF_GPIOTE_EVENT_IN_2;
            case 3: return NRF_GPIOTE_EVENT_IN_3;
#if MAX_GPIOTE_CHANNELS > 4
            case 4: return NRF_GPIOTE_EVENT_IN_4;
            case 5: return NRF_GPIOTE_EVENT_IN_5;
            case 6: return NRF_GPIOTE_EVENT_IN_6;
            case 7: return NRF_GPIOTE_EVENT_IN_7;
#endif
            default: return NRF_GPIOTE_EVENT_IN_0;  // Fallback (should never happen)
        }
    }

    void GPIOTE_IRQHandler(void) {
        for (uint8_t ch = 0; ch < MAX_GPIOTE_CHANNELS; ch++) {
            nrf_gpiote_event_t event = get_gpiote_event(ch);

            if (nrf_gpiote_event_check(NRF_GPIOTE, event) && gpiote_handles[ch]) {
                nrf_gpiote_event_clear(NRF_GPIOTE, event);

                if (gpiote_handles[ch]->user_handler) {
                    gpiote_handles[ch]->user_handler(gpiote_handles[ch]->user_data);
                }
            }
        }
    }
}

// =============================================================================
// NRF52 ISR Implementation (fl::isr::platform namespace)
// =============================================================================

int attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        FL_WARN("attachTimerHandler: handler is null");
        return -1;  // Invalid parameter
    }

    if (config.frequency_hz == 0) {
        FL_WARN("attachTimerHandler: frequency_hz is 0");
        return -2;  // Invalid frequency
    }

    // Allocate a free timer channel
    int timer_idx = -1;
    uint8_t channel = 0;
    if (!allocate_timer_channel(timer_idx, channel)) {
        FL_WARN("attachTimerHandler: no free timer channels");
        return -3;  // Out of resources
    }

    NRF_TIMER_Type* timer = get_timer_instance(timer_idx);
    if (!timer) {
        free_timer_channel(timer_idx, channel);
        FL_WARN("attachTimerHandler: invalid timer instance");
        return -4;  // Internal error
    }

    // Allocate handle data
    nrf52_isr_handle_data* handle_data = new nrf52_isr_handle_data();
    if (!handle_data) {
        free_timer_channel(timer_idx, channel);
        FL_WARN("attachTimerHandler: failed to allocate handle data");
        return -5;  // Out of memory
    }

    handle_data->is_timer = true;
    handle_data->timer_instance = timer;
    handle_data->timer_channel = channel;
    handle_data->timer_irq = get_timer_irq(timer_idx);
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Store handle for ISR access
    timer_handles[timer_idx][channel] = handle_data;

    // Configure timer (only if this is the first channel on this timer)
    // WARNING: Timer configuration is shared across all channels on the same timer instance
    // Reconfiguring an already-running timer could affect other active channels
    bool timer_already_running = false;
    for (uint8_t ch = 0; ch < get_timer_max_channels(timer_idx); ch++) {
        if (ch != channel && timer_allocated[timer_idx][ch]) {
            timer_already_running = true;
            break;
        }
    }

    if (!timer_already_running) {
        nrf_timer_mode_set(timer, NRF_TIMER_MODE_TIMER);
        nrf_timer_bit_width_set(timer, NRF_TIMER_BIT_WIDTH_32);
    }

    // Choose frequency based on requested timer frequency
    // NRF52 timer can run at: 16MHz, 8MHz, 4MHz, 2MHz, 1MHz, 500kHz, 250kHz, 125kHz, 62.5kHz, 31.25kHz
    // WARNING: Frequency is shared across all channels on the same timer instance
    nrf_timer_frequency_t timer_freq;
    if (config.frequency_hz >= 1000000) {
        timer_freq = NRF_TIMER_FREQ_16MHz;  // 16 MHz for high-frequency timers
    } else if (config.frequency_hz >= 100000) {
        timer_freq = NRF_TIMER_FREQ_1MHz;   // 1 MHz for mid-range
    } else if (config.frequency_hz >= 10000) {
        timer_freq = NRF_TIMER_FREQ_125kHz; // 125 kHz for low-frequency
    } else {
        timer_freq = NRF_TIMER_FREQ_31250Hz; // 31.25 kHz for very low frequency
    }

    if (!timer_already_running) {
        nrf_timer_frequency_set(timer, timer_freq);
    }

    // Calculate compare value based on timer frequency
    uint32_t timer_base_freq;
    switch (timer_freq) {
        case NRF_TIMER_FREQ_16MHz:   timer_base_freq = 16000000; break;
        case NRF_TIMER_FREQ_8MHz:    timer_base_freq = 8000000; break;
        case NRF_TIMER_FREQ_4MHz:    timer_base_freq = 4000000; break;
        case NRF_TIMER_FREQ_2MHz:    timer_base_freq = 2000000; break;
        case NRF_TIMER_FREQ_1MHz:    timer_base_freq = 1000000; break;
        case NRF_TIMER_FREQ_500kHz:  timer_base_freq = 500000; break;
        case NRF_TIMER_FREQ_250kHz:  timer_base_freq = 250000; break;
        case NRF_TIMER_FREQ_125kHz:  timer_base_freq = 125000; break;
        case NRF_TIMER_FREQ_62500Hz: timer_base_freq = 62500; break;
        case NRF_TIMER_FREQ_31250Hz: timer_base_freq = 31250; break;
        default: timer_base_freq = 1000000; break;
    }

    uint32_t compare_value = timer_base_freq / config.frequency_hz;
    if (compare_value == 0) {
        compare_value = 1;  // Minimum value
    }

    // Set compare value using nrf_timer_cc_channel_t enum
    // SDK provides NRF_TIMER_CC_CHANNEL0..5 as the proper channel type
    nrf_timer_cc_channel_t cc_channel = static_cast<nrf_timer_cc_channel_t>(NRF_TIMER_CC_CHANNEL0 + channel);
    nrf_timer_cc_set(timer, cc_channel, compare_value);

    // Enable auto-reload unless one-shot mode requested
    if (!(config.flags & ISR_FLAG_ONE_SHOT)) {
        nrf_timer_shorts_enable(timer,
            static_cast<uint32_t>(NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK << channel));
    }

    // Enable interrupt for this channel
    nrf_timer_int_enable(timer,
        static_cast<uint32_t>(NRF_TIMER_INT_COMPARE0_MASK << channel));

    // Set NVIC priority and enable IRQ
    uint8_t nvic_priority = map_priority_to_nvic(config.priority);
    NVIC_SetPriority(handle_data->timer_irq, nvic_priority);
    NVIC_EnableIRQ(handle_data->timer_irq);

    // Start timer (only if not already running)
    if (!timer_already_running) {
        nrf_timer_task_trigger(timer, NRF_TIMER_TASK_START);
    }

    FL_DBG("Timer started at " << config.frequency_hz << " Hz on TIMER" << timer_idx
           << " channel " << static_cast<int>(channel));

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = NRF52_PLATFORM_ID;
    }

    return 0;  // Success
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        FL_WARN("attachExternalHandler: handler is null");
        return -1;  // Invalid parameter
    }

    // Allocate a GPIOTE channel
    int8_t gpiote_ch = allocate_gpiote_channel();
    if (gpiote_ch < 0) {
        FL_WARN("attachExternalHandler: no free GPIOTE channels");
        return -3;  // Out of resources
    }

    // Allocate handle data
    nrf52_isr_handle_data* handle_data = new nrf52_isr_handle_data();
    if (!handle_data) {
        free_gpiote_channel(gpiote_ch);
        FL_WARN("attachExternalHandler: failed to allocate handle data");
        return -5;  // Out of memory
    }

    handle_data->is_timer = false;
    handle_data->gpiote_channel = gpiote_ch;
    handle_data->gpio_pin = pin;
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Store handle for ISR access
    gpiote_handles[gpiote_ch] = handle_data;

    // Configure GPIO pin as input
    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);

    // Determine GPIOTE polarity from flags
    nrf_gpiote_polarity_t polarity;
    if (config.flags & ISR_FLAG_EDGE_RISING) {
        polarity = NRF_GPIOTE_POLARITY_LOTOHI;
    } else if (config.flags & ISR_FLAG_EDGE_FALLING) {
        polarity = NRF_GPIOTE_POLARITY_HITOLO;
    } else {
        polarity = NRF_GPIOTE_POLARITY_TOGGLE;  // Default to both edges
    }

    // Configure GPIOTE channel
    // SDK 15.x+ HAL functions require NRF_GPIOTE register pointer as first arg
    nrf_gpiote_event_configure(NRF_GPIOTE, static_cast<uint32_t>(gpiote_ch), pin, polarity);
    nrf_gpiote_event_enable(NRF_GPIOTE, static_cast<uint32_t>(gpiote_ch));

    // Enable GPIOTE interrupt
    nrf_gpiote_int_enable(NRF_GPIOTE, 1U << gpiote_ch);

    // Set NVIC priority and enable IRQ
    uint8_t nvic_priority = map_priority_to_nvic(config.priority);
    NVIC_SetPriority(GPIOTE_IRQn, nvic_priority);
    NVIC_EnableIRQ(GPIOTE_IRQn);

    FL_DBG("GPIO interrupt attached on pin " << static_cast<int>(pin)
           << " GPIOTE channel " << static_cast<int>(gpiote_ch));

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = NRF52_PLATFORM_ID;
    }

    return 0;  // Success
}

int detach_handler(isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != NRF52_PLATFORM_ID) {
        FL_WARN("detachHandler: invalid handle");
        return -1;  // Invalid handle
    }

    nrf52_isr_handle_data* handle_data = static_cast<nrf52_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("detachHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        // Disable timer interrupt
        int timer_idx = get_timer_index(handle_data->timer_instance);
        if (timer_idx >= 0) {
            nrf_timer_int_disable(handle_data->timer_instance,
                static_cast<uint32_t>(NRF_TIMER_INT_COMPARE0_MASK << handle_data->timer_channel));

            free_timer_channel(timer_idx, handle_data->timer_channel);
        }
    } else {
        // Disable GPIOTE interrupt
        if (handle_data->gpiote_channel >= 0) {
            nrf_gpiote_event_disable(NRF_GPIOTE, static_cast<uint32_t>(handle_data->gpiote_channel));
            nrf_gpiote_int_disable(NRF_GPIOTE, 1U << handle_data->gpiote_channel);

            free_gpiote_channel(handle_data->gpiote_channel);
        }
    }

    delete handle_data;
    handle.platform_handle = nullptr;
    handle.platform_id = 0;

    FL_DBG("Handler detached");
    return 0;  // Success
}

int enable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != NRF52_PLATFORM_ID) {
        FL_WARN("enableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    nrf52_isr_handle_data* handle_data = static_cast<nrf52_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("enableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        nrf_timer_int_enable(handle_data->timer_instance,
            static_cast<uint32_t>(NRF_TIMER_INT_COMPARE0_MASK << handle_data->timer_channel));
        handle_data->is_enabled = true;
    } else {
        nrf_gpiote_event_enable(NRF_GPIOTE, static_cast<uint32_t>(handle_data->gpiote_channel));
        nrf_gpiote_int_enable(NRF_GPIOTE, 1U << handle_data->gpiote_channel);
        handle_data->is_enabled = true;
    }

    return 0;  // Success
}

int disable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != NRF52_PLATFORM_ID) {
        FL_WARN("disableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    nrf52_isr_handle_data* handle_data = static_cast<nrf52_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("disableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        nrf_timer_int_disable(handle_data->timer_instance,
            static_cast<uint32_t>(NRF_TIMER_INT_COMPARE0_MASK << handle_data->timer_channel));
        handle_data->is_enabled = false;
    } else {
        nrf_gpiote_event_disable(NRF_GPIOTE, static_cast<uint32_t>(handle_data->gpiote_channel));
        nrf_gpiote_int_disable(NRF_GPIOTE, 1U << handle_data->gpiote_channel);
        handle_data->is_enabled = false;
    }

    return 0;  // Success
}

bool is_handler_enabled(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != NRF52_PLATFORM_ID) {
        return false;
    }

    nrf52_isr_handle_data* handle_data = static_cast<nrf52_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        return false;
    }

    return handle_data->is_enabled;
}

const char* get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "Invalid parameter";
        case -2: return "Invalid frequency";
        case -3: return "Out of resources";
        case -4: return "Internal error";
        case -5: return "Out of memory";
        default: return "Unknown error";
    }
}

const char* get_platform_name() {
#if defined(FL_IS_NRF52840) || defined(NRF52840_XXAA)
    return "NRF52840";
#elif defined(FL_IS_NRF52833) || defined(NRF52833_XXAA)
    return "NRF52833";
#elif defined(FL_IS_NRF52832) || defined(NRF52832_XXAA)
    return "NRF52832";
#else
    return "NRF52";
#endif
}

uint32_t get_max_timer_frequency() {
    return 16000000;  // 16 MHz (maximum timer frequency)
}

uint32_t get_min_timer_frequency() {
    return 1;  // 1 Hz (practical minimum)
}

uint8_t get_max_priority() {
    // Return 7 (highest user priority in ISR numbering)
    // Note: NVIC priorities 0-1 reserved for SoftDevice when BLE is active
    return 7;
}

bool requires_assembly_handler(uint8_t priority) {
    // ARM Cortex-M4F: All priority levels support C handlers
    (void)priority;
    return false;
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ARM Cortex-M (NRF52)
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (NRF52)
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl

#endif // NRF52 platform guards
