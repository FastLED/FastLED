/*
  FastLED — SAMD ISR Implementation
  -----------------------------------
  SAMD-specific implementation of the cross-platform ISR API.
  Supports SAMD21 (Cortex-M0+), SAMD51 (Cortex-M4F).

  Uses CMSIS NVIC functions and direct peripheral register access.

  License: MIT (FastLED)
*/

#pragma once

#if defined(__SAMD21__) || defined(__SAMD21G18A__) || defined(__SAMD21E18A__) || \
    defined(__SAMD21J18A__) || defined(__SAMD51__) || defined(__SAMD51G19A__) || \
    defined(__SAMD51J19A__) || defined(__SAMD51J20A__) || defined(__SAMD51P19A__) || \
    defined(__SAMD51P20A__) || defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)

#include "fl/isr.h"
#include "fl/compiler_control.h"
#include "fl/stl/assert.h"
#include "fl/dbg.h"

// Include SAMD SDK headers
FL_EXTERN_C_BEGIN
#include "sam.h"
FL_EXTERN_C_END

namespace fl {
namespace isr {
namespace platform {

// =============================================================================
// Platform-Specific Handle Storage
// =============================================================================

struct samd_isr_handle_data {
    Tc* timer_instance;              // Timer peripheral instance (TC3, TC4, TC5, etc.)
    uint8_t timer_index;             // Timer index (3-7 for SAMD21, 0-7 for SAMD51)
    IRQn_Type timer_irq;             // Timer IRQ number
    uint8_t eic_channel;             // EIC channel (0-15, or 0xFF if not EIC)
    uint8_t gpio_pin;                // GPIO pin number (0xFF if not GPIO)
    bool is_timer;                   // true = timer ISR, false = external ISR
    bool is_enabled;                 // Current enable state
    isr_handler_t user_handler;      // User handler function
    void* user_data;                 // User context

    samd_isr_handle_data()
        : timer_instance(nullptr)
        , timer_index(0xFF)
        , timer_irq(static_cast<IRQn_Type>(0))
        , eic_channel(0xFF)
        , gpio_pin(0xFF)
        , is_timer(false)
        , is_enabled(true)
        , user_handler(nullptr)
        , user_data(nullptr)
    {}
};

// Platform ID for SAMD
// Platform ID registry: 0=STUB, 1=ESP32, 2=AVR, 3=NRF52, 4=RP2040, 5=Teensy, 6=STM32, 7=SAMD, 255=NULL
constexpr uint8_t SAMD_PLATFORM_ID = 7;

// Timer configuration
#if defined(__SAMD21__)
// SAMD21: TC3, TC4, TC5 (sometimes TC6, TC7)
constexpr uint8_t MIN_TIMER_INDEX = 3;
constexpr uint8_t MAX_TIMER_INDEX = 5;  // Conservative, some boards have TC6/TC7
#elif defined(__SAMD51__)
// SAMD51: TC0-TC7
constexpr uint8_t MIN_TIMER_INDEX = 0;
constexpr uint8_t MAX_TIMER_INDEX = 7;
#else
// Default conservative range
constexpr uint8_t MIN_TIMER_INDEX = 3;
constexpr uint8_t MAX_TIMER_INDEX = 5;
#endif

constexpr uint8_t MAX_TIMER_INSTANCES = MAX_TIMER_INDEX - MIN_TIMER_INDEX + 1;

// EIC configuration
constexpr uint8_t MAX_EIC_CHANNELS = 16;

// Track allocated resources
static bool timer_allocated[MAX_TIMER_INDEX + 1] = {};
static bool eic_allocated[MAX_EIC_CHANNELS] = {};

// Storage for handles (to allow ISR to find user handler)
static samd_isr_handle_data* timer_handles[MAX_TIMER_INDEX + 1] = {};
static samd_isr_handle_data* eic_handles[MAX_EIC_CHANNELS] = {};

// =============================================================================
// Helper Functions
// =============================================================================

// Get timer instance pointer from index
static Tc* get_timer_instance(uint8_t index) {
    switch (index) {
#ifdef TC0
        case 0: return TC0;
#endif
#ifdef TC1
        case 1: return TC1;
#endif
#ifdef TC2
        case 2: return TC2;
#endif
#ifdef TC3
        case 3: return TC3;
#endif
#ifdef TC4
        case 4: return TC4;
#endif
#ifdef TC5
        case 5: return TC5;
#endif
#ifdef TC6
        case 6: return TC6;
#endif
#ifdef TC7
        case 7: return TC7;
#endif
        default: return nullptr;
    }
}

// Get timer IRQ from index
static IRQn_Type get_timer_irq(uint8_t index) {
    switch (index) {
#ifdef TC0_IRQn
        case 0: return TC0_IRQn;
#endif
#ifdef TC1_IRQn
        case 1: return TC1_IRQn;
#endif
#ifdef TC2_IRQn
        case 2: return TC2_IRQn;
#endif
#ifdef TC3_IRQn
        case 3: return TC3_IRQn;
#endif
#ifdef TC4_IRQn
        case 4: return TC4_IRQn;
#endif
#ifdef TC5_IRQn
        case 5: return TC5_IRQn;
#endif
#ifdef TC6_IRQn
        case 6: return TC6_IRQn;
#endif
#ifdef TC7_IRQn
        case 7: return TC7_IRQn;
#endif
        default: return static_cast<IRQn_Type>(0);
    }
}

// Allocate a free timer
static bool allocate_timer(uint8_t& timer_idx) {
    // Critical section: prevent interrupt from modifying allocation state
    __disable_irq();

    bool found = false;
    for (uint8_t i = MIN_TIMER_INDEX; i <= MAX_TIMER_INDEX; i++) {
        if (!timer_allocated[i]) {
            timer_allocated[i] = true;
            timer_idx = i;
            found = true;
            break;
        }
    }

    __enable_irq();
    return found;
}

// Free a timer
static void free_timer(uint8_t timer_idx) {
    if (timer_idx <= MAX_TIMER_INDEX) {
        // Critical section: prevent interrupt from accessing freed resources
        __disable_irq();
        timer_allocated[timer_idx] = false;
        timer_handles[timer_idx] = nullptr;
        __enable_irq();
    }
}

// Allocate a free EIC channel
static bool allocate_eic_channel(uint8_t& channel) {
    // Critical section: prevent interrupt from modifying allocation state
    __disable_irq();

    bool found = false;
    for (uint8_t i = 0; i < MAX_EIC_CHANNELS; i++) {
        if (!eic_allocated[i]) {
            eic_allocated[i] = true;
            channel = i;
            found = true;
            break;
        }
    }

    __enable_irq();
    return found;
}

// Free an EIC channel
static void free_eic_channel(uint8_t channel) {
    if (channel < MAX_EIC_CHANNELS) {
        // Critical section: prevent interrupt from accessing freed resources
        __disable_irq();
        eic_allocated[channel] = false;
        eic_handles[channel] = nullptr;
        __enable_irq();
    }
}

// Map ISR priority (1-7) to NVIC priority
// For SAMD21 (M0+): 4 priority levels (0-3), lower number = higher priority
// For SAMD51 (M4): 8 priority levels (0-7), lower number = higher priority
// ISR priority 1 (low) -> NVIC 3, ISR priority 7 (max) -> NVIC 0
static uint8_t map_priority_to_nvic(uint8_t isr_priority) {
    // Clamp to valid range
    if (isr_priority < 1) isr_priority = 1;

#if defined(__SAMD21__)
    // SAMD21 has 4 priority levels (0-3)
    // Map ISR 1-7 to NVIC 3-0
    if (isr_priority > 4) isr_priority = 4;
    return 4 - isr_priority;
#elif defined(__SAMD51__)
    // SAMD51 has 8 priority levels (0-7)
    // Map ISR 1-7 to NVIC 7-0
    if (isr_priority > 7) isr_priority = 7;
    return 8 - isr_priority;
#else
    // Default to 4 levels (conservative)
    if (isr_priority > 4) isr_priority = 4;
    return 4 - isr_priority;
#endif
}

// Check if timer is syncing (required for SAMD register writes)
static bool tc_is_syncing(Tc* tc) {
#if defined(__SAMD21__)
    return tc->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
#elif defined(__SAMD51__)
    return tc->COUNT16.SYNCBUSY.reg != 0;
#else
    return tc->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
#endif
}

// Wait for timer sync
static void tc_wait_sync(Tc* tc) {
    while (tc_is_syncing(tc)) {
        // Wait for sync
    }
}

// Reset timer
static void tc_reset(Tc* tc) {
    tc->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    tc_wait_sync(tc);
    while (tc->COUNT16.CTRLA.bit.SWRST) {
        // Wait for reset to complete
    }
}

// =============================================================================
// Timer ISR Handlers
// =============================================================================

// Common timer interrupt handler
static void timer_interrupt_handler(uint8_t timer_idx) {
    Tc* timer = get_timer_instance(timer_idx);
    if (!timer) return;

    // Check if MC0 (Match/Compare 0) interrupt occurred
    if (timer->COUNT16.INTFLAG.bit.MC0) {
        // Clear the interrupt flag
        timer->COUNT16.INTFLAG.bit.MC0 = 1;

        samd_isr_handle_data* handle = timer_handles[timer_idx];
        if (handle && handle->user_handler) {
            handle->user_handler(handle->user_data);
        }
    }
}

// Timer ISR wrappers for each possible TC instance
extern "C" {
#ifdef TC0_IRQn
    void TC0_Handler(void) {
        timer_interrupt_handler(0);
    }
#endif

#ifdef TC1_IRQn
    void TC1_Handler(void) {
        timer_interrupt_handler(1);
    }
#endif

#ifdef TC2_IRQn
    void TC2_Handler(void) {
        timer_interrupt_handler(2);
    }
#endif

#ifdef TC3_IRQn
    void TC3_Handler(void) {
        timer_interrupt_handler(3);
    }
#endif

#ifdef TC4_IRQn
    void TC4_Handler(void) {
        timer_interrupt_handler(4);
    }
#endif

#ifdef TC5_IRQn
    void TC5_Handler(void) {
        timer_interrupt_handler(5);
    }
#endif

#ifdef TC6_IRQn
    void TC6_Handler(void) {
        timer_interrupt_handler(6);
    }
#endif

#ifdef TC7_IRQn
    void TC7_Handler(void) {
        timer_interrupt_handler(7);
    }
#endif

    // EIC (External Interrupt Controller) handler
    void EIC_Handler(void) {
        for (uint8_t ch = 0; ch < MAX_EIC_CHANNELS; ch++) {
            uint32_t flag = 1UL << ch;

            if (EIC->INTFLAG.reg & flag) {
                // Clear the interrupt flag
                EIC->INTFLAG.reg = flag;

                samd_isr_handle_data* handle = eic_handles[ch];
                if (handle && handle->user_handler) {
                    handle->user_handler(handle->user_data);
                }
            }
        }
    }
}

// =============================================================================
// SAMD ISR Implementation (fl::isr::platform namespace)
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

    // Allocate a free timer
    uint8_t timer_idx = 0;
    if (!allocate_timer(timer_idx)) {
        FL_WARN("attachTimerHandler: no free timers");
        return -3;  // Out of resources
    }

    Tc* timer = get_timer_instance(timer_idx);
    if (!timer) {
        free_timer(timer_idx);
        FL_WARN("attachTimerHandler: invalid timer instance");
        return -4;  // Internal error
    }

    // Allocate handle data
    samd_isr_handle_data* handle_data = new samd_isr_handle_data();
    if (!handle_data) {
        free_timer(timer_idx);
        FL_WARN("attachTimerHandler: failed to allocate handle data");
        return -5;  // Out of memory
    }

    handle_data->is_timer = true;
    handle_data->timer_instance = timer;
    handle_data->timer_index = timer_idx;
    handle_data->timer_irq = get_timer_irq(timer_idx);
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Store handle for ISR access
    timer_handles[timer_idx] = handle_data;

    // Enable clock for timer
#if defined(__SAMD21__)
    // SAMD21: Use GCLK to route clock to TC peripheral
    // Most SAMD21 boards use GCLK_CLKCTRL_ID_TCC2_TC3 for TC3/TC4/TC5
    uint16_t gclk_id;
    if (timer_idx <= 5) {
        gclk_id = GCLK_CLKCTRL_ID_TCC2_TC3;
    } else {
        gclk_id = GCLK_CLKCTRL_ID_TCC2_TC3;  // Adjust if TC6/TC7 available
    }

    GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | gclk_id);
    while (GCLK->STATUS.bit.SYNCBUSY) {
        // Wait for sync
    }
#elif defined(__SAMD51__)
    // SAMD51: Use GCLK to route clock to TC peripheral
    GCLK->PCHCTRL[timer_idx + TC0_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
    while (!(GCLK->PCHCTRL[timer_idx + TC0_GCLK_ID].reg & GCLK_PCHCTRL_CHEN)) {
        // Wait for enable
    }
#endif

    // Reset timer to known state
    tc_reset(timer);

    // Configure as 16-bit counter
    timer->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16;
    tc_wait_sync(timer);

    // Set match frequency waveform mode
#if defined(__SAMD21__)
    // SAMD21: Waveform generation mode is part of CTRLA register
    timer->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
    tc_wait_sync(timer);
#elif defined(__SAMD51__)
    // SAMD51: Separate WAVE register
    timer->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;
    tc_wait_sync(timer);
#endif

    // Choose prescaler based on requested frequency
    // SystemCoreClock is typically 48 MHz (SAMD21) or 120 MHz (SAMD51)
    uint32_t prescaler_div;
    uint32_t prescaler_reg;

    if (config.frequency_hz >= 100000) {
        prescaler_div = 1;
        prescaler_reg = TC_CTRLA_PRESCALER_DIV1;
    } else if (config.frequency_hz >= 10000) {
        prescaler_div = 8;
        prescaler_reg = TC_CTRLA_PRESCALER_DIV8;
    } else if (config.frequency_hz >= 1000) {
        prescaler_div = 64;
        prescaler_reg = TC_CTRLA_PRESCALER_DIV64;
    } else if (config.frequency_hz >= 100) {
        prescaler_div = 256;
        prescaler_reg = TC_CTRLA_PRESCALER_DIV256;
    } else {
        prescaler_div = 1024;
        prescaler_reg = TC_CTRLA_PRESCALER_DIV1024;
    }

    timer->COUNT16.CTRLA.reg |= prescaler_reg;
    tc_wait_sync(timer);

    // Calculate compare value
    uint32_t timer_freq = SystemCoreClock / prescaler_div;
    uint32_t compare_value = timer_freq / config.frequency_hz;

    // Clamp to 16-bit range
    if (compare_value > 0xFFFF) {
        compare_value = 0xFFFF;
    }
    if (compare_value == 0) {
        compare_value = 1;
    }

    // Set compare value for match/compare channel 0
    timer->COUNT16.CC[0].reg = (uint16_t)compare_value;
    tc_wait_sync(timer);

    // Enable MC0 (Match/Compare 0) interrupt
    timer->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    // Configure NVIC
    uint8_t nvic_priority = map_priority_to_nvic(config.priority);
    NVIC_DisableIRQ(handle_data->timer_irq);
    NVIC_ClearPendingIRQ(handle_data->timer_irq);
    NVIC_SetPriority(handle_data->timer_irq, nvic_priority);
    NVIC_EnableIRQ(handle_data->timer_irq);

    // Enable timer
    timer->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    tc_wait_sync(timer);

    FL_DBG("Timer started at " << config.frequency_hz << " Hz on TC" << static_cast<int>(timer_idx));

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = SAMD_PLATFORM_ID;
    }

    return 0;  // Success
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        FL_WARN("attachExternalHandler: handler is null");
        return -1;  // Invalid parameter
    }

    // Allocate an EIC channel
    uint8_t eic_ch = 0;
    if (!allocate_eic_channel(eic_ch)) {
        FL_WARN("attachExternalHandler: no free EIC channels");
        return -3;  // Out of resources
    }

    // Allocate handle data
    samd_isr_handle_data* handle_data = new samd_isr_handle_data();
    if (!handle_data) {
        free_eic_channel(eic_ch);
        FL_WARN("attachExternalHandler: failed to allocate handle data");
        return -5;  // Out of memory
    }

    handle_data->is_timer = false;
    handle_data->eic_channel = eic_ch;
    handle_data->gpio_pin = pin;
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Store handle for ISR access
    eic_handles[eic_ch] = handle_data;

    // Enable EIC clock
#if defined(__SAMD21__)
    GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_EIC);
    while (GCLK->STATUS.bit.SYNCBUSY) {
        // Wait for sync
    }
#elif defined(__SAMD51__)
    GCLK->PCHCTRL[EIC_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
    while (!(GCLK->PCHCTRL[EIC_GCLK_ID].reg & GCLK_PCHCTRL_CHEN)) {
        // Wait for enable
    }
#endif

    // Configure GPIO pin for EIC function
    // Note: Pin-to-EIC-channel mapping is board-specific
    // For now, we use a simple mapping (this may need board-specific adjustments)
    uint8_t port = pin / 32;
    uint8_t pin_num = pin % 32;

    PORT->Group[port].PINCFG[pin_num].reg |= PORT_PINCFG_PMUXEN;

    // Set PMUX to function A (EIC) for the pin
    if (pin_num & 1) {
        PORT->Group[port].PMUX[pin_num >> 1].reg |= PORT_PMUX_PMUXO_A;
    } else {
        PORT->Group[port].PMUX[pin_num >> 1].reg |= PORT_PMUX_PMUXE_A;
    }

    // Enable EIC if not already enabled
#if defined(__SAMD21__)
    // SAMD21 uses CTRL instead of CTRLA, and STATUS.SYNCBUSY instead of SYNCBUSY
    if (!EIC->CTRL.bit.ENABLE) {
        EIC->CTRL.bit.ENABLE = 0;
        while (EIC->STATUS.bit.SYNCBUSY) {
            // Wait for sync
        }

        // Configure EIC
        EIC->CTRL.bit.ENABLE = 1;
        while (EIC->STATUS.bit.SYNCBUSY) {
            // Wait for sync
        }
    }
#elif defined(__SAMD51__)
    // SAMD51 uses CTRLA and dedicated SYNCBUSY register
    if (!EIC->CTRLA.bit.ENABLE) {
        EIC->CTRLA.bit.ENABLE = 0;
        while (EIC->SYNCBUSY.bit.ENABLE) {
            // Wait for sync
        }

        // Configure EIC
        EIC->CTRLA.bit.ENABLE = 1;
        while (EIC->SYNCBUSY.bit.ENABLE) {
            // Wait for sync
        }
    }
#endif

    // Determine sense configuration from flags
    uint8_t sense;
    if (config.flags & ISR_FLAG_EDGE_RISING) {
        sense = EIC_CONFIG_SENSE0_RISE_Val;
    } else if (config.flags & ISR_FLAG_EDGE_FALLING) {
        sense = EIC_CONFIG_SENSE0_FALL_Val;
    } else {
        sense = EIC_CONFIG_SENSE0_BOTH_Val;  // Default to both edges
    }

    // Configure EIC channel
    uint8_t config_idx = eic_ch / 8;  // Each CONFIG register handles 8 channels
    uint8_t sense_shift = (eic_ch % 8) * 4;  // 4 bits per channel

    EIC->CONFIG[config_idx].reg &= ~(0xF << sense_shift);
    EIC->CONFIG[config_idx].reg |= (sense << sense_shift);

    // Enable interrupt for this channel
    EIC->INTENSET.reg = (1UL << eic_ch);

    // Configure NVIC
    uint8_t nvic_priority = map_priority_to_nvic(config.priority);
    NVIC_DisableIRQ(EIC_IRQn);
    NVIC_ClearPendingIRQ(EIC_IRQn);
    NVIC_SetPriority(EIC_IRQn, nvic_priority);
    NVIC_EnableIRQ(EIC_IRQn);

    FL_DBG("EIC interrupt attached on pin " << static_cast<int>(pin)
           << " EIC channel " << static_cast<int>(eic_ch));

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = SAMD_PLATFORM_ID;
    }

    return 0;  // Success
}

int detach_handler(isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != SAMD_PLATFORM_ID) {
        FL_WARN("detachHandler: invalid handle");
        return -1;  // Invalid handle
    }

    samd_isr_handle_data* handle_data = static_cast<samd_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("detachHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        // Disable timer interrupt
        Tc* timer = handle_data->timer_instance;
        if (timer) {
            timer->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
            timer->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
            tc_wait_sync(timer);

            NVIC_DisableIRQ(handle_data->timer_irq);
            free_timer(handle_data->timer_index);
        }
    } else {
        // Disable EIC interrupt
        if (handle_data->eic_channel < MAX_EIC_CHANNELS) {
            EIC->INTENCLR.reg = (1UL << handle_data->eic_channel);
            free_eic_channel(handle_data->eic_channel);
        }
    }

    delete handle_data;
    handle.platform_handle = nullptr;
    handle.platform_id = 0;

    FL_DBG("Handler detached");
    return 0;  // Success
}

int enable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != SAMD_PLATFORM_ID) {
        FL_WARN("enableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    samd_isr_handle_data* handle_data = static_cast<samd_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("enableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        Tc* timer = handle_data->timer_instance;
        if (timer) {
            timer->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
            NVIC_EnableIRQ(handle_data->timer_irq);
            handle_data->is_enabled = true;
        }
    } else {
        EIC->INTENSET.reg = (1UL << handle_data->eic_channel);
        handle_data->is_enabled = true;
    }

    return 0;  // Success
}

int disable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != SAMD_PLATFORM_ID) {
        FL_WARN("disableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    samd_isr_handle_data* handle_data = static_cast<samd_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("disableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        Tc* timer = handle_data->timer_instance;
        if (timer) {
            timer->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
            NVIC_DisableIRQ(handle_data->timer_irq);
            handle_data->is_enabled = false;
        }
    } else {
        EIC->INTENCLR.reg = (1UL << handle_data->eic_channel);
        handle_data->is_enabled = false;
    }

    return 0;  // Success
}

bool is_handler_enabled(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != SAMD_PLATFORM_ID) {
        return false;
    }

    samd_isr_handle_data* handle_data = static_cast<samd_isr_handle_data*>(handle.platform_handle);
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
#if defined(__SAMD51__)
    return "SAMD51";
#elif defined(__SAMD21__)
    return "SAMD21";
#else
    return "SAMD";
#endif
}

uint32_t get_max_timer_frequency() {
#if defined(__SAMD51__)
    return 120000000;  // 120 MHz (SAMD51 max clock)
#elif defined(__SAMD21__)
    return 48000000;   // 48 MHz (SAMD21 max clock)
#else
    return 48000000;   // Conservative default
#endif
}

uint32_t get_min_timer_frequency() {
    return 1;  // 1 Hz (practical minimum)
}

uint8_t get_max_priority() {
#if defined(__SAMD51__)
    return 7;  // SAMD51 has 8 levels (0-7)
#elif defined(__SAMD21__)
    return 3;  // SAMD21 has 4 levels (0-3)
#else
    return 3;  // Conservative default
#endif
}

bool requires_assembly_handler(uint8_t priority) {
    // ARM Cortex-M0+ and Cortex-M4F: All priority levels support C handlers
    (void)priority;
    return false;
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ARM Cortex-M (SAMD)
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (SAMD)
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl

#endif // SAMD platform guards
