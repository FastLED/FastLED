/// @file stm32_gpio_timer_helpers.cpp
/// @brief Shared GPIO and Timer helper implementations for STM32
///
/// This file centralizes common GPIO and Timer utility functions used across
/// STM32 implementations.
///
/// IMPORTANT: This file requires STM32 HAL types (GPIO_TypeDef, TIM_TypeDef, etc.)
/// which are available when STM32duino core is present.

// Include platform detection header for FL_IS_STM32
#include "platforms/arm/stm32/is_stm32.h"
#include "fl/has_include.h"

// Platform guard using standardized FL_IS_STM32 macro
#if defined(FL_IS_STM32)

#include "platforms/arm/stm32/stm32_capabilities.h"

// STM32duino core headers - provides HAL includes and pin mapping functions
// IMPORTANT: The Maple framework (used by stm32f103cb maple_mini) does NOT have
// stm32_def.h or HAL types. We set FL_STM32_HAS_HAL only when the HAL is available.
// All other STM32duino-specific headers are only included when HAL is available,
// because on Maple the pins_arduino.h includes variant.h which has undefined macros.
#if FL_HAS_INCLUDE("stm32_def.h")
#include "stm32_def.h"  // STM32duino core - provides HAL includes
#define FL_STM32_HAS_HAL  // Marker that STM32 HAL types are available
#if FL_HAS_INCLUDE("pins_arduino.h")
#include "pins_arduino.h"  // Arduino pin mapping - provides digitalPinToPinName macro
#endif
#if FL_HAS_INCLUDE("PeripheralPins.h")
#include "PeripheralPins.h"  // Pin mapping tables
#endif
#if FL_HAS_INCLUDE("pinmap.h")
#include "pinmap.h"  // Pin mapping functions
#endif
#endif  // FL_HAS_INCLUDE("stm32_def.h")

#include "platforms/arm/stm32/stm32_capabilities.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"

namespace fl {
namespace stm32 {

// All function definitions below require STM32 HAL types (GPIO_TypeDef, TIM_TypeDef, etc.)
// which are only available when STM32duino core is present (not on Maple framework).
#ifdef FL_STM32_HAS_HAL

// ============================================================================
// GPIO Helper Functions
// ============================================================================

GPIO_TypeDef* getGPIOPort(uint8_t pin) {
#ifdef HAL_GPIO_MODULE_ENABLED
    PinName pin_name = digitalPinToPinName(pin);
    if (pin_name == NC) {
        return nullptr;
    }

    // STM32H7 Mbed core doesn't provide get_GPIO_Port/STM_PORT macros
    // Use direct port extraction from PinName
#if defined(FL_IS_STM32_H7)
    // PinName format: (port << 4) | pin_number
    // Extract port number and map to GPIO_TypeDef*
    uint32_t port_idx = STM_PORT(pin_name);
    static GPIO_TypeDef* const gpio_ports[] = {
        GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH,
#ifdef GPIOI
        GPIOI,
#endif
#ifdef GPIOJ
        GPIOJ,
#endif
#ifdef GPIOK
        GPIOK
#endif
    };
    if (port_idx < sizeof(gpio_ports) / sizeof(gpio_ports[0])) {
        return gpio_ports[port_idx];
    }
    return nullptr;
#else
    return get_GPIO_Port(STM_PORT(pin_name));
#endif
#else
    (void)pin;
    return nullptr;
#endif
}

uint32_t getGPIOPin(uint8_t pin) {
#ifdef HAL_GPIO_MODULE_ENABLED
    PinName pin_name = digitalPinToPinName(pin);
    if (pin_name == NC) {
        return 0;
    }

    // STM32H7 Mbed core doesn't provide STM_GPIO_PIN macro
    // Use direct pin extraction from PinName
#if defined(FL_IS_STM32_H7)
    // PinName format: (port << 4) | pin_number
    // Extract pin number and convert to GPIO_PIN_x define
    uint32_t pin_num = STM_PIN(pin_name);
    return (1 << pin_num);  // Convert to GPIO_PIN_0, GPIO_PIN_1, etc.
#else
    return STM_GPIO_PIN(pin_name);
#endif
#else
    (void)pin;
    return 0;
#endif
}

void enableGPIOClock(GPIO_TypeDef* port) {
#ifdef HAL_GPIO_MODULE_ENABLED
    if (port == nullptr) {
        return;
    }

    // Enable GPIO clock based on port
#ifdef GPIOA
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOB
    if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOC
    if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOD
    if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOE
    if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOF
    if (port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOG
    if (port == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOH
    if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOI
    if (port == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOJ
    if (port == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        return;
    }
#endif
#ifdef GPIOK
    if (port == GPIOK) {
        __HAL_RCC_GPIOK_CLK_ENABLE();
        return;
    }
#endif
#else
    (void)port;
#endif
}

bool configurePinAsOutput(uint8_t pin, uint32_t speed) {
#ifdef HAL_GPIO_MODULE_ENABLED
    GPIO_TypeDef* port = getGPIOPort(pin);
    uint32_t pin_mask = getGPIOPin(pin);

    if (port == nullptr || pin_mask == 0) {
        return false;
    }

    // Enable GPIO clock
    enableGPIOClock(port);

    // Configure as output
    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = pin_mask;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = speed;

    HAL_GPIO_Init(port, &GPIO_InitStruct);
    return true;
#else
    (void)pin;
    (void)speed;
    return false;
#endif
}

bool configurePinAsTimerAF(uint8_t pin, TIM_TypeDef* timer, uint32_t speed) {
#ifdef HAL_GPIO_MODULE_ENABLED
    GPIO_TypeDef* port = getGPIOPort(pin);
    uint32_t pin_mask = getGPIOPin(pin);
    PinName pin_name = digitalPinToPinName(pin);

    if (port == nullptr || pin_mask == 0 || pin_name == NC) {
        return false;
    }

    // Enable GPIO clock
    enableGPIOClock(port);

#if defined(FASTLED_STM32_GPIO_USES_AFIO_REMAP)
    // STM32F1: Use AFIO remapping
    // Enable AFIO clock
    __HAL_RCC_AFIO_CLK_ENABLE();

    // Configure pin as alternate function push-pull
    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = pin_mask;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = speed;

    HAL_GPIO_Init(port, &GPIO_InitStruct);

    // Note: F1 remapping is timer-specific and usually done globally
    // Applications may need to call __HAL_AFIO_REMAP_TIMx_ENABLE() separately
    // based on specific pin/timer combinations
    return true;

#elif defined(FASTLED_STM32_GPIO_USES_AF_NUMBERS)
    // STM32F2/F4/F7/H7/L4/G4/U5: Use AF numbers
    // Find the AF number for this pin and timer using STM32duino pinmap
    // Note: H7 uses PinMap_PWM instead of PinMap_TIM
    #if defined(FL_IS_STM32_H7)
        uint32_t af_mode = pinmap_find_function(pin_name, PinMap_PWM);
    #else
        uint32_t af_mode = pinmap_find_function(pin_name, PinMap_TIM);
    #endif

    if (af_mode == (uint32_t)NC) {
        // Pin doesn't support Timer AF
        FL_WARN("STM32: Pin " << pin << " does not support Timer AF");
        return false;
    }

    // Configure as alternate function
    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = pin_mask;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = speed;
    GPIO_InitStruct.Alternate = af_mode;

    HAL_GPIO_Init(port, &GPIO_InitStruct);
    return true;
#else
    // Unknown platform
    return false;
#endif

#else
    (void)pin;
    (void)timer;
    (void)speed;
    return false;
#endif
}

bool isValidPin(uint8_t pin) {
#ifdef HAL_GPIO_MODULE_ENABLED
    PinName pin_name = digitalPinToPinName(pin);
    return pin_name != NC;
#else
    (void)pin;
    return false;
#endif
}

// ============================================================================
// Timer Helper Functions
// ============================================================================
// These functions are only needed for hardware SPI implementations

#if defined(FL_STM32_HAS_SPI_HW_2) || defined(FL_STM32_HAS_SPI_HW_4) || defined(FL_STM32_HAS_SPI_HW_8)

TIM_TypeDef* selectTimer(int bus_id) {
#if defined(HAL_TIM_MODULE_ENABLED)
    // Timer allocation strategy (based on HARDWARE_RESOURCES.md):
    // Bus 0: TIM2 (32-bit, general-purpose, widely available)
    // Bus 1: TIM3 (16-bit, general-purpose)
    // Bus 2: TIM4 (16-bit, general-purpose)
    // Bus 3: TIM5 (32-bit, available on F4/F7/H7)

    switch (bus_id) {
#ifdef TIM2
        case 0: return TIM2;
#endif
#ifdef TIM3
        case 1: return TIM3;
#endif
#ifdef TIM4
        case 2: return TIM4;
#endif
#ifdef TIM5
        case 3: return TIM5;
#endif
        default: return nullptr;
    }
#else
    (void)bus_id;
    return nullptr;
#endif
}

void enableTimerClock(TIM_TypeDef* timer) {
#if defined(HAL_TIM_MODULE_ENABLED)
    if (timer == nullptr) {
        return;
    }

    // Enable timer clock based on peripheral
#ifdef TIM2
    if (timer == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        return;
    }
#endif
#ifdef TIM3
    if (timer == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
        return;
    }
#endif
#ifdef TIM4
    if (timer == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        return;
    }
#endif
#ifdef TIM5
    if (timer == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
        return;
    }
#endif
#ifdef TIM8
    if (timer == TIM8) {
        __HAL_RCC_TIM8_CLK_ENABLE();
        return;
    }
#endif
#else
    (void)timer;
#endif
}

uint32_t getTimerClockFreq(TIM_TypeDef* timer) {
#if defined(HAL_TIM_MODULE_ENABLED) && defined(HAL_RCC_MODULE_ENABLED)
    if (timer == nullptr) {
        return 0;
    }

    // Timer clock source depends on APB bus and RCC configuration
    // TIM2, TIM3, TIM4, TIM5, TIM6, TIM7, TIM12-14 are on APB1
    // TIM1, TIM8, TIM9-11, TIM15-17 are on APB2

    // Determine which APB bus this timer is on
    bool is_apb1 = (timer == TIM2 || timer == TIM3 || timer == TIM4
#ifdef FASTLED_STM32_HAS_TIM5
        || timer == TIM5
#endif
#ifdef TIM6
        || timer == TIM6
#endif
#ifdef TIM7
        || timer == TIM7
#endif
    );

    // Get APB clock frequency using HAL
    uint32_t apb_freq = is_apb1 ? HAL_RCC_GetPCLK1Freq() : HAL_RCC_GetPCLK2Freq();

    // Timer clock = APB clock × 2 if APB prescaler != 1 (per STM32 reference manual)
    // Check RCC prescaler bits to determine if 2x multiplier applies
    uint32_t timer_freq = apb_freq;

#if defined(STM32F1xx) || defined(STM32F2xx) || defined(STM32F4xx) || defined(STM32F7xx)
    // F1/F2/F4/F7: Check PPRE bits in RCC->CFGR
    if (is_apb1) {
        // Check APB1 prescaler (PPRE1 bits [10:8])
        uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> 8;
        if (ppre1 >= 4) {  // Prescaler != 1 (values 4-7 mean /2, /4, /8, /16)
            timer_freq = apb_freq * 2;
        }
    } else {
        // Check APB2 prescaler (PPRE2 bits [13:11])
        uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> 11;
        if (ppre2 >= 4) {  // Prescaler != 1
            timer_freq = apb_freq * 2;
        }
    }
#elif defined(STM32H7xx)
    // H7: Check D2PPRE bits in RCC->D2CFGR (more complex clock tree)
    if (is_apb1) {
        uint32_t d2ppre1 = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) >> RCC_D2CFGR_D2PPRE1_Pos;
        if (d2ppre1 >= 4) {
            timer_freq = apb_freq * 2;
        }
    } else {
        uint32_t d2ppre2 = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE2) >> RCC_D2CFGR_D2PPRE2_Pos;
        if (d2ppre2 >= 4) {
            timer_freq = apb_freq * 2;
        }
    }
#elif defined(STM32L4xx) || defined(STM32G4xx)
    // L4/G4: Similar to F4 but with different register names
    if (is_apb1) {
        uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
        if (ppre1 >= 4) {
            timer_freq = apb_freq * 2;
        }
    } else {
        uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
        if (ppre2 >= 4) {
            timer_freq = apb_freq * 2;
        }
    }
#else
    // Generic fallback: assume 2x multiplier (most common configuration)
    timer_freq = apb_freq * 2;
#endif

    return timer_freq;

#else
    (void)timer;
    return 0;
#endif
}

bool initTimerPWM(TIM_HandleTypeDef* htim, TIM_TypeDef* timer, uint32_t frequency_hz) {
#if defined(HAL_TIM_MODULE_ENABLED)
    if (htim == nullptr || timer == nullptr || frequency_hz == 0) {
        return false;
    }

    // Enable timer clock
    enableTimerClock(timer);

    // Get timer clock frequency
    uint32_t timer_clock = getTimerClockFreq(timer);
    if (timer_clock == 0) {
        FL_WARN("STM32: Failed to get timer clock frequency");
        return false;
    }

    // Calculate period (ARR register value)
    // Period = (Timer_Clock / Desired_Frequency) - 1
    uint32_t period = (timer_clock / frequency_hz) - 1;

    // Validate period is within timer range
    // 16-bit timers: max 65535, 32-bit timers: max 4294967295
    bool is_32bit = (timer == TIM2
#ifdef FASTLED_STM32_HAS_TIM5
        || timer == TIM5
#endif
    );
    uint32_t max_period = is_32bit ? 0xFFFFFFFF : 0xFFFF;

    if (period > max_period) {
        FL_WARN("STM32: Timer period " << period << " exceeds max " << max_period);
        return false;
    }

    // Configure timer base using provided handle
    fl::memset(htim, 0, sizeof(TIM_HandleTypeDef));  // Clear handle
    htim->Instance = timer;
    htim->Init.Prescaler = 0;  // No prescaling for maximum frequency
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = period;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        FL_WARN("STM32: Timer base init failed");
        return false;
    }

    // Configure PWM mode on channel 1 (50% duty cycle)
    TIM_OC_InitTypeDef sConfigOC = {};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = period / 2;  // 50% duty cycle
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        FL_WARN("STM32: Timer PWM config failed");
        return false;
    }

    return true;
#else
    (void)htim;
    (void)timer;
    (void)frequency_hz;
    return false;
#endif
}

bool startTimer(TIM_HandleTypeDef* htim) {
#if defined(HAL_TIM_MODULE_ENABLED)
    if (htim == nullptr || htim->Instance == nullptr) {
        return false;
    }

    // Start PWM on channel 1
    if (HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1) != HAL_OK) {
        FL_WARN("STM32: Failed to start timer PWM");
        return false;
    }

    return true;
#else
    (void)htim;
    return false;
#endif
}

void stopTimer(TIM_HandleTypeDef* htim) {
#if defined(HAL_TIM_MODULE_ENABLED)
    if (htim == nullptr || htim->Instance == nullptr) {
        return;
    }

    // Stop PWM on channel 1
    HAL_TIM_PWM_Stop(htim, TIM_CHANNEL_1);
#else
    (void)htim;
#endif
}

uint8_t getTimerChannel(uint8_t pin, TIM_TypeDef* timer) {
#if defined(HAL_TIM_MODULE_ENABLED) && defined(FASTLED_STM32_GPIO_USES_AF_NUMBERS)
    // For F2/F4/F7/H7/L4/G4/U5: Use pinmap_pinAF to determine channel
    PinName pin_name = digitalPinToPinName(pin);
    if (pin_name == NC) {
        return 0;
    }

    // Try to find which channel this pin supports for this timer
    // This is platform-specific - pinmap_pinAF tells us the AF number,
    // but we need to check the pin map to determine the channel
    // For now, default to channel 1 (most common for clock output)
    // TODO: Implement proper channel detection using pin maps

    (void)timer;  // Unused for now
    return 1;  // Default to channel 1

#elif defined(FASTLED_STM32_GPIO_USES_AFIO_REMAP)
    // For F1: Channel mapping depends on AFIO remap configuration
    // Default to channel 1 for simplicity
    (void)pin;
    (void)timer;
    return 1;

#else
    (void)pin;
    (void)timer;
    return 0;
#endif
}

#endif  // FL_STM32_HAS_SPI_HW_*

// ============================================================================
// DMA Helper Functions (Stream-based DMA for F2/F4/F7/H7/L4)
// ============================================================================
// These functions are only available when DMA streams are supported

#if defined(FASTLED_STM32_HAS_DMA_STREAMS) && (defined(FL_STM32_HAS_SPI_HW_2) || defined(FL_STM32_HAS_SPI_HW_4) || defined(FL_STM32_HAS_SPI_HW_8))

void enableDMAClock(DMA_TypeDef* dma) {
#if defined(HAL_DMA_MODULE_ENABLED)
    if (dma == nullptr) {
        return;
    }

#ifdef DMA1
    if (dma == DMA1) {
        __HAL_RCC_DMA1_CLK_ENABLE();
        return;
    }
#endif
#ifdef DMA2
    if (dma == DMA2) {
        __HAL_RCC_DMA2_CLK_ENABLE();
        return;
    }
#endif
#else
    (void)dma;
#endif
}

DMA_Stream_TypeDef* getDMAStream(TIM_TypeDef* timer, int bus_id, int lane) {
#if defined(FASTLED_STM32_HAS_DMA_STREAMS)
    // STM32F4/F7/H7 DMA allocation strategy (from HARDWARE_RESOURCES.md):
    // Dual-SPI uses 2 streams per bus
    // Bus 0 (TIM2): DMA1 Stream 1, 7
    // Bus 1 (TIM3): DMA1 Stream 2, 4
    // Bus 2 (TIM4): DMA1 Stream 6, 3
    // Bus 3 (TIM5): DMA1 Stream 0, 5

#ifdef DMA1_Stream0
    if (timer == TIM2) {
        // TIM2_UP on DMA1: Stream 1 (Channel 3), Stream 7 (Channel 3)
        // See STM32F4 RM0090 Table 42
        return (lane == 0) ? DMA1_Stream1 : DMA1_Stream7;
    }
#ifdef TIM3
    if (timer == TIM3) {
        // TIM3_UP on DMA1: Stream 2 (Channel 5), Stream 4 (Channel 5)
        // See STM32F4 RM0090 Table 42
        return (lane == 0) ? DMA1_Stream2 : DMA1_Stream4;
    }
#endif
#ifdef TIM4
    if (timer == TIM4) {
        // TIM4_UP on DMA1: Stream 6 (Channel 2), Stream 3 (Channel 2) - note reversed order
        // See STM32F4 RM0090 Table 42
        return (lane == 0) ? DMA1_Stream6 : DMA1_Stream3;
    }
#endif
#ifdef TIM5
    if (timer == TIM5) {
        // TIM5_UP on DMA1: Stream 0 (Channel 6), Stream 5 (Channel 6) - note Stream 6 also valid
        // See STM32F4 RM0090 Table 42
        return (lane == 0) ? DMA1_Stream0 : DMA1_Stream5;
    }
#endif
#endif
#else
    (void)timer;
    (void)bus_id;
    (void)lane;
#endif
    return nullptr;
}

uint32_t getDMAChannel(TIM_TypeDef* timer) {
#if defined(FASTLED_STM32_HAS_DMA_STREAMS)

#if defined(FL_IS_STM32_H7)
    // STM32H7 uses DMAMUX - return DMA request ID instead of channel
    // See STM32H7 reference manual, Table "DMAMUX1 request mapping"
#ifdef TIM2
    if (timer == TIM2) {
        return DMA_REQUEST_TIM2_UP;  // DMAMUX request ID for TIM2_UP
    }
#endif
#ifdef TIM3
    if (timer == TIM3) {
        return DMA_REQUEST_TIM3_UP;  // DMAMUX request ID for TIM3_UP
    }
#endif
#ifdef TIM4
    if (timer == TIM4) {
        return DMA_REQUEST_TIM4_UP;  // DMAMUX request ID for TIM4_UP
    }
#endif
#ifdef TIM5
    if (timer == TIM5) {
        return DMA_REQUEST_TIM5_UP;  // DMAMUX request ID for TIM5_UP
    }
#endif

#else
    // STM32F2/F4/F7/L4: Use fixed DMA channel numbers
    // TIM2_UP: Channel 3
    // TIM3_UP: Channel 5
    // TIM4_UP: Channel 2
    // TIM5_UP: Channel 6

#ifdef TIM2
    if (timer == TIM2) {
        return DMA_CHANNEL_3;
    }
#endif
#ifdef TIM3
    if (timer == TIM3) {
        return DMA_CHANNEL_5;
    }
#endif
#ifdef TIM4
    if (timer == TIM4) {
        return DMA_CHANNEL_2;
    }
#endif
#ifdef TIM5
    if (timer == TIM5) {
        return DMA_CHANNEL_6;
    }
#endif
#endif  // FL_IS_STM32_H7

#else
    (void)timer;
#endif
    return 0xFF;  // Invalid channel
}

DMA_TypeDef* getDMAController(DMA_Stream_TypeDef* stream) {
#if defined(FASTLED_STM32_HAS_DMA_STREAMS)
#ifdef DMA1_Stream0
    if (stream >= DMA1_Stream0 && stream <= DMA1_Stream7) {
        return DMA1;
    }
#endif
#ifdef DMA2_Stream0
    if (stream >= DMA2_Stream0 && stream <= DMA2_Stream7) {
        return DMA2;
    }
#endif
#else
    (void)stream;
#endif
    return nullptr;
}

uint8_t getStreamIndex(DMA_Stream_TypeDef* stream) {
#if defined(FASTLED_STM32_HAS_DMA_STREAMS)
    // Use lookup table to avoid pointer arithmetic
    // (pointer arithmetic fails with DMA stream structures)
#ifdef DMA1_Stream0
    static const DMA_Stream_TypeDef* dma1_streams[] = {
        DMA1_Stream0, DMA1_Stream1, DMA1_Stream2, DMA1_Stream3,
        DMA1_Stream4, DMA1_Stream5, DMA1_Stream6, DMA1_Stream7
    };
    for (uint8_t i = 0; i < 8; i++) {
        if (stream == dma1_streams[i]) {
            return i;
        }
    }
#endif
#ifdef DMA2_Stream0
    static const DMA_Stream_TypeDef* dma2_streams[] = {
        DMA2_Stream0, DMA2_Stream1, DMA2_Stream2, DMA2_Stream3,
        DMA2_Stream4, DMA2_Stream5, DMA2_Stream6, DMA2_Stream7
    };
    for (uint8_t i = 0; i < 8; i++) {
        if (stream == dma2_streams[i]) {
            return i;
        }
    }
#endif
#else
    (void)stream;
#endif
    return 0xFF;  // Invalid stream
}

bool initDMA(DMA_Stream_TypeDef* stream, const void* src, volatile void* dst, uint32_t size, uint32_t channel) {
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    if (stream == nullptr || src == nullptr || dst == nullptr || size == 0) {
        return false;
    }

    // Get DMA controller and enable clock
    DMA_TypeDef* dma = getDMAController(stream);
    if (dma == nullptr) {
        return false;
    }
    enableDMAClock(dma);

    // Create DMA handle
    DMA_HandleTypeDef hdma = {};
    hdma.Instance = stream;

    // STM32H7 uses Request (DMAMUX), F4/F7 use Channel
#if defined(FL_IS_STM32_H7)
    hdma.Init.Request = channel;  // On H7, this is actually a DMAMUX request ID
#else
    hdma.Init.Channel = channel;
#endif

    hdma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma.Init.PeriphInc = DMA_PINC_DISABLE;     // Fixed peripheral address
    hdma.Init.MemInc = DMA_MINC_ENABLE;          // Increment memory address
    hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma.Init.Mode = DMA_NORMAL;                 // Not circular
    hdma.Init.Priority = DMA_PRIORITY_HIGH;
    hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma) != HAL_OK) {
        FL_WARN("STM32: DMA init failed");
        return false;
    }

    // Configure transfer addresses and size
    if (HAL_DMA_Start(&hdma, (uint32_t)src, (uint32_t)dst, size) != HAL_OK) {
        FL_WARN("STM32: DMA start failed");
        return false;
    }

    FL_DBG("STM32: DMA stream configured successfully");
    return true;
#else
    (void)stream;
    (void)src;
    (void)dst;
    (void)size;
    (void)channel;
    return false;
#endif
}

bool isDMAComplete(DMA_Stream_TypeDef* stream) {
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    if (stream == nullptr) {
        return true;
    }

    // Check if stream is enabled (disabled when transfer completes)
    if ((stream->CR & DMA_SxCR_EN) == 0) {
        return true;
    }

    // Check transfer complete flag
    DMA_TypeDef* dma = getDMAController(stream);
    uint8_t stream_idx = getStreamIndex(stream);

    if (dma == nullptr || stream_idx == 0xFF) {
        return true;  // Assume complete if invalid
    }

#if defined(FL_IS_STM32_H7)
    // STM32H7: Use direct register access instead of LL functions
    // Check the Transfer Complete Interrupt Flag (TCIFx) in LISR/HISR
    if (stream_idx < 4) {
        // Streams 0-3: Use LISR (Low Interrupt Status Register)
        // Bit positions: Stream 0: bit 5, Stream 1: bit 11, Stream 2: bit 21, Stream 3: bit 27
        static const uint32_t tc_flags[] = {
            DMA_LISR_TCIF0, DMA_LISR_TCIF1, DMA_LISR_TCIF2, DMA_LISR_TCIF3
        };
        return (dma->LISR & tc_flags[stream_idx]) != 0;
    } else {
        // Streams 4-7: Use HISR (High Interrupt Status Register)
        // Bit positions: Stream 4: bit 5, Stream 5: bit 11, Stream 6: bit 21, Stream 7: bit 27
        static const uint32_t tc_flags[] = {
            DMA_HISR_TCIF4, DMA_HISR_TCIF5, DMA_HISR_TCIF6, DMA_HISR_TCIF7
        };
        return (dma->HISR & tc_flags[stream_idx - 4]) != 0;
    }
#else
    // STM32F2/F4/F7/L4: Use LL driver functions for flag checking (stream-specific)
    // Create function pointer array for TC flag checks
    typedef uint32_t (*FlagFunc)(DMA_TypeDef*);
    static const FlagFunc tc_funcs[] = {
        LL_DMA_IsActiveFlag_TC0, LL_DMA_IsActiveFlag_TC1,
        LL_DMA_IsActiveFlag_TC2, LL_DMA_IsActiveFlag_TC3,
        LL_DMA_IsActiveFlag_TC4, LL_DMA_IsActiveFlag_TC5,
        LL_DMA_IsActiveFlag_TC6, LL_DMA_IsActiveFlag_TC7
    };

    if (stream_idx < 8) {
        return tc_funcs[stream_idx](dma) != 0;
    }

    return true;
#endif

#else
    (void)stream;
    return true;
#endif
}

void clearDMAFlags(DMA_Stream_TypeDef* stream) {
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    if (stream == nullptr) {
        return;
    }

    DMA_TypeDef* dma = getDMAController(stream);
    uint8_t stream_idx = getStreamIndex(stream);

    if (dma == nullptr || stream_idx == 0xFF) {
        return;
    }

#if defined(FL_IS_STM32_H7)
    // STM32H7: Use direct register access to clear flags
    // Clear all interrupt flags for this stream (TC, HT, TE, DME, FE)
    if (stream_idx < 4) {
        // Streams 0-3: Use LIFCR (Low Interrupt Flag Clear Register)
        static const uint32_t clear_flags[] = {
            DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0,
            DMA_LIFCR_CTCIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CFEIF1,
            DMA_LIFCR_CTCIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CFEIF2,
            DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3
        };
        dma->LIFCR = clear_flags[stream_idx];
    } else {
        // Streams 4-7: Use HIFCR (High Interrupt Flag Clear Register)
        static const uint32_t clear_flags[] = {
            DMA_HIFCR_CTCIF4 | DMA_HIFCR_CHTIF4 | DMA_HIFCR_CTEIF4 | DMA_HIFCR_CDMEIF4 | DMA_HIFCR_CFEIF4,
            DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5,
            DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6,
            DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 | DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CFEIF7
        };
        dma->HIFCR = clear_flags[stream_idx - 4];
    }
#else
    // STM32F2/F4/F7/L4: Use LL driver functions to clear flags
    typedef void (*ClearFunc)(DMA_TypeDef*);
    static const ClearFunc clear_funcs[] = {
        LL_DMA_ClearFlag_TC0, LL_DMA_ClearFlag_TC1,
        LL_DMA_ClearFlag_TC2, LL_DMA_ClearFlag_TC3,
        LL_DMA_ClearFlag_TC4, LL_DMA_ClearFlag_TC5,
        LL_DMA_ClearFlag_TC6, LL_DMA_ClearFlag_TC7
    };

    if (stream_idx < 8) {
        clear_funcs[stream_idx](dma);
    }
#endif

#else
    (void)stream;
#endif
}

void stopDMA(DMA_Stream_TypeDef* stream) {
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    if (stream == nullptr) {
        return;
    }

    // Disable stream
    stream->CR &= ~DMA_SxCR_EN;

    // Wait for stream to be disabled
    while (stream->CR & DMA_SxCR_EN) {
        // Wait
    }

    // Clear flags
    clearDMAFlags(stream);

    FL_DBG("STM32: DMA stream stopped");
#else
    (void)stream;
#endif
}

#endif  // FASTLED_STM32_HAS_DMA_STREAMS && FL_STM32_HAS_SPI_HW_*

#endif  // FL_STM32_HAS_HAL

}  // namespace stm32
}  // namespace fl

#endif  // FL_IS_STM32
