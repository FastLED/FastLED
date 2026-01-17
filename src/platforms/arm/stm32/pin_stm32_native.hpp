#pragma once

/// @file platforms/arm/stm32/pin_stm32_native.hpp
/// STM32 HAL GPIO implementation (non-Arduino path)
///
/// Provides STM32 HAL-based GPIO functions for non-Arduino builds.
/// Uses STM32duino's pin mapping functions to convert Arduino pin numbers
/// to STM32 HAL GPIO ports and pins.
///
/// IMPORTANT: This file requires STM32 HAL to be available via STM32duino core headers.

#include "fl/has_include.h"

// STM32duino core headers - provides HAL includes and pin mapping functions
// These headers provide: PinName, NC, digitalPinToPinName(), STM_PORT(), STM_GPIO_PIN(),
// STM_PIN_CHANNEL(), STM_PIN_AFNUM(), pinmap_find_function(), pinmap_peripheral(),
// PinMap_ADC, PinMap_TIM/PinMap_PWM, and all STM32 HAL types
//
// IMPORTANT: The Maple framework (used by stm32f103cb maple_mini) does NOT have
// stm32_def.h or HAL types. All STM32duino-specific headers are only included when
// stm32_def.h is available, because on Maple the pins_arduino.h includes variant.h
// which has undefined macros (BOARD_SPI*_PIN).
#if FL_HAS_INCLUDE("stm32_def.h")
#include "stm32_def.h"       // STM32 HAL types and definitions
// Only include STM32duino-specific headers when HAL is available
#if FL_HAS_INCLUDE("pins_arduino.h")
#include "pins_arduino.h"    // Arduino pin mapping - provides digitalPinToPinName macro
#endif
#if FL_HAS_INCLUDE("PeripheralPins.h")
#include "PeripheralPins.h"  // Pin mapping tables (PinMap_ADC, PinMap_TIM, etc.)
#endif
#if FL_HAS_INCLUDE("pinmap.h")
#include "pinmap.h"          // Pin mapping functions (digitalPinToPinName, pinmap_find_function, etc.)
#define FL_STM32_HAS_PINMAP 1  // Marker that pinmap functions are available
#endif
#endif  // FL_HAS_INCLUDE("stm32_def.h")

#include "fl/warn.h"
#include "fl/dbg.h"
#include "platforms/arm/stm32/stm32_gpio_timer_helpers.h"  // GPIO and Timer helper functions

namespace fl {
namespace platform {

// ============================================================================
// Pin Mode Control
// ============================================================================

inline void pinMode(int pin, PinMode mode) {
#ifdef HAL_GPIO_MODULE_ENABLED
    // Use native helper functions instead of Arduino-specific digitalPinToPinName()
    GPIO_TypeDef* port = fl::stm32::getGPIOPort(pin);
    uint32_t pin_mask = fl::stm32::getGPIOPin(pin);

    if (port == nullptr || pin_mask == 0) {
        FL_WARN("STM32: Invalid pin " << pin);
        return;
    }

    // Enable GPIO clock (based on port)
#ifdef GPIOA
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
#ifdef GPIOB
    if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
#ifdef GPIOC
    if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
#endif
#ifdef GPIOD
    if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef GPIOE
    if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef GPIOF
    if (port == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef GPIOG
    if (port == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#ifdef GPIOH
    if (port == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
#ifdef GPIOI
    if (port == GPIOI) __HAL_RCC_GPIOI_CLK_ENABLE();
#endif
#ifdef GPIOJ
    if (port == GPIOJ) __HAL_RCC_GPIOJ_CLK_ENABLE();
#endif
#ifdef GPIOK
    if (port == GPIOK) __HAL_RCC_GPIOK_CLK_ENABLE();
#endif

    // Configure GPIO based on mode
    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = pin_mask;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    // Map fl::PinMode to STM32 HAL modes
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    switch (mode) {
        case PinMode::Input:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            break;
        case PinMode::Output:
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            break;
        case PinMode::InputPullup:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLUP;
            break;
        case PinMode::InputPulldown:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
            break;
        default:
            FL_WARN("STM32: Unknown pin mode " << static_cast<int>(mode) << " for pin " << pin);
            return;
    }

    HAL_GPIO_Init(port, &GPIO_InitStruct);
#else
    (void)pin;
    (void)mode;
    FL_WARN("STM32: HAL_GPIO_MODULE not enabled");
#endif
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, PinValue val) {
#ifdef HAL_GPIO_MODULE_ENABLED
    // Use native helper functions instead of Arduino-specific digitalPinToPinName()
    GPIO_TypeDef* port = fl::stm32::getGPIOPort(pin);
    uint32_t pin_mask = fl::stm32::getGPIOPin(pin);

    if (port == nullptr || pin_mask == 0) {
        return;
    }

    // Write pin state using HAL
    // Translate fl::PinValue to GPIO_PinState
    HAL_GPIO_WritePin(port, pin_mask, (val == PinValue::High) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    (void)pin;
    (void)val;
#endif
}

inline PinValue digitalRead(int pin) {
#ifdef HAL_GPIO_MODULE_ENABLED
    // Use native helper functions instead of Arduino-specific digitalPinToPinName()
    GPIO_TypeDef* port = fl::stm32::getGPIOPort(pin);
    uint32_t pin_mask = fl::stm32::getGPIOPin(pin);

    if (port == nullptr || pin_mask == 0) {
        return PinValue::Low;
    }

    // Read pin state using HAL
    return (HAL_GPIO_ReadPin(port, pin_mask) == GPIO_PIN_SET) ? PinValue::High : PinValue::Low;
#else
    (void)pin;
    return PinValue::Low;
#endif
}

// ============================================================================
// Analog I/O
// ============================================================================

inline uint16_t analogRead(int pin) {
    // NOTE: analogRead requires STM32duino pinmap functions for ADC channel mapping
    // This is because there's no direct way to map Arduino pin numbers to ADC channels
    // without the pinmap tables provided by STM32duino core.
    // For non-Arduino STM32 builds, ADC must be configured manually using HAL APIs.
    (void)pin;
    FL_WARN("STM32: analogRead not available without STM32duino core");
    return 0;
#if 0  // Disabled: requires STM32duino pinmap functions
#if defined(HAL_ADC_MODULE_ENABLED) && defined(FL_STM32_HAS_PINMAP)
    // Implementation based on STM32duino analog.cpp adc_read_value()
    // Uses HAL ADC APIs for single-shot 12-bit ADC conversion

    // Note: ADC pinmapping requires STM32duino pinmap functions (pinmap_find_function, pinmap_peripheral)
    // These are only available when STM32duino core is present
    PinName pin_name = digitalPinToPinName(pin);
    if (pin_name == NC) {
        FL_WARN("STM32: Invalid pin " << pin);
        return 0;
    }

    // Find ADC instance and channel for this pin using STM32duino pinmap
    uint32_t function = pinmap_find_function(pin_name, PinMap_ADC);
    if (function == (uint32_t)NC) {
        FL_WARN("STM32: Pin " << pin << " does not support ADC");
        return 0;
    }

    // Extract ADC instance and channel from pinmap function
    ADC_TypeDef* adc_instance = (ADC_TypeDef*)pinmap_peripheral(pin_name, PinMap_ADC);
    uint32_t adc_channel = STM_PIN_CHANNEL(function);

    if (adc_instance == nullptr) {
        FL_WARN("STM32: Failed to get ADC instance for pin " << pin);
        return 0;
    }

    // Configure ADC handle for single conversion
    ADC_HandleTypeDef AdcHandle = {};
    AdcHandle.Instance = adc_instance;
    AdcHandle.State = HAL_ADC_STATE_RESET;
    AdcHandle.DMA_Handle = nullptr;
    AdcHandle.Lock = HAL_UNLOCKED;

    // ADC initialization parameters (12-bit resolution, single conversion)
    // Note: STM32F1 has different ADC struct members than F2/F4/F7
#if defined(STM32F1) || defined(STM32F1xx) || defined(STM32F10X_MD)
    // STM32F1 ADC configuration (no ClockPrescaler, Resolution, EOCSelection, etc.)
    AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.ScanConvMode = DISABLE;
    AdcHandle.Init.ContinuousConvMode = DISABLE;
    AdcHandle.Init.NbrOfConversion = 1;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
#else
    // STM32F2/F4/F7/L4/H7 ADC configuration
    AdcHandle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    AdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.ScanConvMode = DISABLE;
    AdcHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    AdcHandle.Init.ContinuousConvMode = DISABLE;
    AdcHandle.Init.NbrOfConversion = 1;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandle.Init.DMAContinuousRequests = DISABLE;
#endif

    // Initialize ADC peripheral
    if (HAL_ADC_Init(&AdcHandle) != HAL_OK) {
        FL_WARN("STM32: ADC initialization failed");
        return 0;
    }

    // Configure ADC channel
    ADC_ChannelConfTypeDef AdcChannelConf = {};
    AdcChannelConf.Channel = adc_channel;
    AdcChannelConf.Rank = ADC_REGULAR_RANK_1;

    // Sampling time - use moderate sampling time for balanced speed/accuracy
#if defined(ADC_SAMPLETIME_15CYCLES)
    AdcChannelConf.SamplingTime = ADC_SAMPLETIME_15CYCLES;
#elif defined(ADC_SAMPLETIME_19CYCLES_5)
    AdcChannelConf.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
#elif defined(ADC_SAMPLETIME_13CYCLES_5)
    AdcChannelConf.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
#else
    AdcChannelConf.SamplingTime = ADC_SAMPLETIME_15CYCLES;  // Default fallback
#endif

    if (HAL_ADC_ConfigChannel(&AdcHandle, &AdcChannelConf) != HAL_OK) {
        FL_WARN("STM32: ADC channel configuration failed");
        HAL_ADC_DeInit(&AdcHandle);
        return 0;
    }

    // Start ADC conversion
    if (HAL_ADC_Start(&AdcHandle) != HAL_OK) {
        FL_WARN("STM32: ADC start failed");
        HAL_ADC_DeInit(&AdcHandle);
        return 0;
    }

    // Poll for conversion complete (10ms timeout)
    if (HAL_ADC_PollForConversion(&AdcHandle, 10) != HAL_OK) {
        FL_WARN("STM32: ADC conversion timeout");
        HAL_ADC_Stop(&AdcHandle);
        HAL_ADC_DeInit(&AdcHandle);
        return 0;
    }

    // Read converted value
    uint16_t value = 0;
    if ((HAL_ADC_GetState(&AdcHandle) & HAL_ADC_STATE_REG_EOC) == HAL_ADC_STATE_REG_EOC) {
        value = static_cast<uint16_t>(HAL_ADC_GetValue(&AdcHandle));
    }

    // Stop and deinitialize ADC
    HAL_ADC_Stop(&AdcHandle);
    HAL_ADC_DeInit(&AdcHandle);

    // Scale 12-bit value to 10-bit Arduino-compatible value (0-1023)
    return static_cast<uint16_t>(value >> 2);  // 4096 -> 1024 range

#else
    (void)pin;
    return 0;
#endif  // HAL_ADC_MODULE_ENABLED && FL_STM32_HAS_PINMAP
#endif  // #if 0 - disabled code
}

inline void analogWrite(int pin, uint16_t val) {
    // NOTE: analogWrite requires STM32duino pinmap functions for Timer channel mapping
    // This is because there's no direct way to map Arduino pin numbers to Timer channels
    // without the pinmap tables provided by STM32duino core.
    // For non-Arduino STM32 builds, PWM must be configured manually using HAL TIM APIs.
    (void)pin;
    (void)val;
    FL_WARN("STM32: analogWrite not available without STM32duino core");
#if 0  // Disabled: requires STM32duino pinmap functions
#if defined(HAL_TIM_MODULE_ENABLED) && defined(FL_STM32_HAS_PINMAP)
    // Implementation based on STM32duino analog.cpp pwm_start()
    // Uses HAL Timer PWM APIs for 8-bit PWM output (Arduino-compatible)

    // Note: PWM pinmapping requires STM32duino pinmap functions (pinmap_find_function, pinmap_peripheral)
    // These are only available when STM32duino core is present
    PinName pin_name = digitalPinToPinName(pin);
    if (pin_name == NC) {
        FL_WARN("STM32: Invalid pin " << pin);
        return;
    }

    // Clamp value to 8-bit range (0-255) for Arduino compatibility
    if (val > 255) val = 255;

    // Find Timer instance and channel for this pin using STM32duino pinmap
    uint32_t function = pinmap_find_function(pin_name, PinMap_TIM);
    if (function == (uint32_t)NC) {
        // Pin doesn't support PWM - fall back to digital write
        fl::pinMode(pin, PinMode::Output);
        fl::digitalWrite(pin, val >= 128 ? PinValue::High : PinValue::Low);
        return;
    }

    // Extract timer instance and channel from pinmap
    TIM_TypeDef* timer_instance = (TIM_TypeDef*)pinmap_peripheral(pin_name, PinMap_TIM);
    uint32_t timer_channel = STM_PIN_CHANNEL(function);

    if (timer_instance == nullptr) {
        FL_WARN("STM32: Failed to get Timer instance for pin " << pin);
        return;
    }

    // Convert STM32 channel number to HAL channel constant
    uint32_t hal_channel;
    switch (timer_channel) {
        case 1: hal_channel = TIM_CHANNEL_1; break;
        case 2: hal_channel = TIM_CHANNEL_2; break;
        case 3: hal_channel = TIM_CHANNEL_3; break;
        case 4: hal_channel = TIM_CHANNEL_4; break;
        default:
            FL_WARN("STM32: Invalid timer channel " << timer_channel);
            return;
    }

    // Configure GPIO pin as Timer alternate function
    // This enables the timer to control the pin for PWM output
    GPIO_TypeDef* port = get_GPIO_Port(STM_PORT(pin_name));
    uint32_t pin_mask = STM_GPIO_PIN(pin_name);

    if (port == nullptr) {
        FL_WARN("STM32: Failed to get GPIO port for pin " << pin);
        return;
    }

    // Enable GPIO clock
    fl::stm32::enableGPIOClock(port);

    // Configure pin as alternate function for timer
    GPIO_InitTypeDef GPIO_InitStruct = {};
    GPIO_InitStruct.Pin = pin_mask;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

#if defined(FASTLED_STM32_GPIO_USES_AF_NUMBERS)
    // F2/F4/F7/H7/L4/G4: Use AF number from pinmap
    GPIO_InitStruct.Alternate = STM_PIN_AFNUM(function);
#endif

    HAL_GPIO_Init(port, &GPIO_InitStruct);

    // Enable timer clock
    fl::stm32::enableTimerClock(timer_instance);

    // Get timer clock frequency for PWM calculation
    uint32_t timer_clock = fl::stm32::getTimerClockFreq(timer_instance);
    if (timer_clock == 0) {
        FL_WARN("STM32: Failed to get timer clock frequency");
        return;
    }

    // Calculate timer period for 490Hz PWM (Arduino default frequency)
    // Period = (Timer_Clock / PWM_Frequency) - 1
    const uint32_t PWM_FREQUENCY = 490;  // Hz (Arduino default)
    uint32_t period = (timer_clock / PWM_FREQUENCY) - 1;

    // Validate period fits in timer (16-bit or 32-bit)
    bool is_32bit = (timer_instance == TIM2
#ifdef FASTLED_STM32_HAS_TIM5
        || timer_instance == TIM5
#endif
    );
    uint32_t max_period = is_32bit ? 0xFFFFFFFF : 0xFFFF;

    if (period > max_period) {
        // Frequency too low, use prescaler
        // For simplicity, just cap at max period (reduces frequency)
        period = max_period;
    }

    // Static storage for timer handles (required to persist across calls)
    // Use a simple array indexed by timer number (TIM2=0, TIM3=1, TIM4=2, TIM5=3)
    // This allows multiple timers to be used simultaneously
    static TIM_HandleTypeDef htim_handles[4] = {};

    // Determine timer index (TIM2=0, TIM3=1, TIM4=2, TIM5=3)
    int timer_idx = -1;
    if (timer_instance == TIM2) timer_idx = 0;
#ifdef TIM3
    else if (timer_instance == TIM3) timer_idx = 1;
#endif
#ifdef TIM4
    else if (timer_instance == TIM4) timer_idx = 2;
#endif
#ifdef FASTLED_STM32_HAS_TIM5
    else if (timer_instance == TIM5) timer_idx = 3;
#endif

    if (timer_idx < 0) {
        FL_WARN("STM32: Unsupported timer instance for PWM");
        return;
    }

    TIM_HandleTypeDef* htim = &htim_handles[timer_idx];
    htim->Instance = timer_instance;
    htim->Init.Prescaler = 0;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = period;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    // Initialize timer base
    if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        FL_WARN("STM32: Timer base initialization failed");
        return;
    }

    // Configure PWM channel
    TIM_OC_InitTypeDef sConfigOC = {};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    // Calculate pulse width (duty cycle) from 8-bit value
    // val: 0-255 -> pulse: 0-period
    sConfigOC.Pulse = (static_cast<uint32_t>(val) * period) / 255;

    if (HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, hal_channel) != HAL_OK) {
        FL_WARN("STM32: Timer PWM configuration failed");
        return;
    }

    // Start PWM output
    if (HAL_TIM_PWM_Start(htim, hal_channel) != HAL_OK) {
        FL_WARN("STM32: Timer PWM start failed");
        return;
    }

#else
    (void)pin;
    (void)val;
#endif  // HAL_TIM_MODULE_ENABLED && FL_STM32_HAS_PINMAP
#endif  // #if 0 - disabled code
}

inline void setPwm16(int pin, uint16_t val) {
    // STM32 16-bit PWM implementation would configure Timer PWM with period=65535
    // For simplified implementation, scale 16-bit value to 8-bit and use analogWrite
    // Full 16-bit implementation would require HAL timer reconfiguration
    analogWrite(pin, val >> 8);
}

inline void setAdcRange(AdcRange range) {
    // STM32 ADC reference voltage configuration
    // On STM32, the ADC reference voltage is typically hardware-configured:
    // - VDDA (analog supply voltage) is the default reference
    // - VREF+ pin can be used on some MCUs with external reference
    // - Internal reference voltage (~1.2V) available on some families
    //
    // fl::AdcRange modes:
    // - Default: Use default reference (VDDA on STM32)
    // - Range0_1V1: Use internal reference (~1.2V on STM32)
    // - External: Use external reference on VREF+ pin
    //
    // STM32 HAL doesn't provide a simple API to switch reference voltage
    // at runtime without full ADC reconfiguration. This would require:
    // 1. Stopping all ADC operations
    // 2. Reconfiguring ADC common registers
    // 3. Restarting ADC calibration
    //
    // For compatibility, we accept the range parameter but don't implement
    // dynamic switching. Users should configure VREF hardware appropriately.

    (void)range;
    FL_DBG("STM32: setAdcRange not dynamically configurable - using hardware VREF");
}

}  // namespace platform
}  // namespace fl
