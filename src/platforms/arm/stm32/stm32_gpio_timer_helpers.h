/// @file stm32_gpio_timer_helpers.h
/// @brief Common GPIO and Timer helper functions for STM32 parallel SPI implementations
///
/// This header provides shared utility functions for GPIO pin configuration,
/// timer setup, and peripheral clock management used across all STM32 SPI implementations
/// (dual, quad, and octal).
///
/// These helpers abstract away the STM32 HAL complexity and provide a consistent
/// interface for pin mapping, clock configuration, and timer initialization.
///
/// Usage:
///   #include "platforms/arm/stm32/stm32_gpio_timer_helpers.h"
///   using namespace fl::stm32;  // In implementation files only
///   GPIO_TypeDef* port = getGPIOPort(pin);

#pragma once

#include "ftl/stdint.h"

// STM32 HAL types cannot be forward-declared because they are typedef'd.
// Include Arduino.h which pulls in the full STM32 HAL headers.
#include <Arduino.h>

namespace fl {
namespace stm32 {

// ============================================================================
// GPIO Helper Functions
// ============================================================================

/// @brief Get GPIO port from Arduino pin number
/// @param pin Arduino pin number
/// @return GPIO_TypeDef* or nullptr if invalid
GPIO_TypeDef* getGPIOPort(uint8_t pin);

/// @brief Get GPIO pin mask from Arduino pin number
/// @param pin Arduino pin number
/// @return GPIO pin mask (GPIO_PIN_x)
uint32_t getGPIOPin(uint8_t pin);

/// @brief Enable clock for a GPIO port
/// @param port GPIO port (GPIOA, GPIOB, etc.)
void enableGPIOClock(GPIO_TypeDef* port);

/// @brief Configure pin as general purpose output
/// @param pin Arduino pin number
/// @param speed GPIO speed (default: GPIO_SPEED_FREQ_HIGH)
/// @return true if successful
bool configurePinAsOutput(uint8_t pin, uint32_t speed = 0x02 /* GPIO_SPEED_FREQ_HIGH */);

/// @brief Configure pin as Timer alternate function (for clock generation)
/// @param pin Arduino pin number
/// @param timer TIM_TypeDef* timer peripheral
/// @param speed GPIO speed (default: FASTLED_GPIO_SPEED_MAX from capabilities.h)
/// @return true if successful
bool configurePinAsTimerAF(uint8_t pin, TIM_TypeDef* timer, uint32_t speed = 0x03 /* GPIO_SPEED_FREQ_VERY_HIGH */);

/// @brief Validate that a pin number is valid
/// @param pin Arduino pin number
/// @return true if valid
bool isValidPin(uint8_t pin);

// ============================================================================
// Timer Helper Functions
// ============================================================================

/// @brief Select timer peripheral based on bus_id
/// @param bus_id Logical bus identifier (0-3)
/// @return TIM_TypeDef* or nullptr if invalid
TIM_TypeDef* selectTimer(int bus_id);

/// @brief Enable RCC clock for a timer peripheral
/// @param timer Timer peripheral (TIM2, TIM3, etc.)
void enableTimerClock(TIM_TypeDef* timer);

/// @brief Get timer clock frequency for calculating prescaler
/// @param timer Timer peripheral
/// @return Timer clock frequency in Hz
/// @note Uses HAL functions to query actual clock configuration
uint32_t getTimerClockFreq(TIM_TypeDef* timer);

/// @brief Initialize timer for PWM clock generation
/// @param htim Timer handle (must be persistent, e.g., class member)
/// @param timer Timer peripheral
/// @param frequency_hz Desired PWM frequency in Hz (SPI clock rate)
/// @return true if successful
/// @note htim must remain valid for the lifetime of the timer usage
bool initTimerPWM(TIM_HandleTypeDef* htim, TIM_TypeDef* timer, uint32_t frequency_hz);

/// @brief Start timer PWM output
/// @param htim Timer handle (must be previously initialized)
/// @return true if successful
bool startTimer(TIM_HandleTypeDef* htim);

/// @brief Stop timer PWM output
/// @param htim Timer handle
void stopTimer(TIM_HandleTypeDef* htim);

/// @brief Get timer channel number for a pin
/// @param pin Arduino pin number
/// @param timer Timer peripheral
/// @return Timer channel (1-4) or 0 if not supported
uint8_t getTimerChannel(uint8_t pin, TIM_TypeDef* timer);

// ============================================================================
// DMA Helper Functions (Stream-based DMA for F2/F4/F7/H7/L4)
// ============================================================================
// These functions are only available when FASTLED_STM32_HAS_DMA_STREAMS is defined

/// @brief Enable RCC clock for DMA controller
/// @param dma DMA controller (DMA1 or DMA2)
void enableDMAClock(DMA_TypeDef* dma);

/// @brief Get DMA stream for a specific timer and lane
/// @param timer Timer peripheral
/// @param bus_id Bus identifier (0-3)
/// @param lane Lane number (0-7 for different data lanes)
/// @return DMA_Stream_TypeDef* or nullptr if not available
DMA_Stream_TypeDef* getDMAStream(TIM_TypeDef* timer, int bus_id, int lane);

/// @brief Get DMA channel number for timer update event
/// @param timer Timer peripheral
/// @return DMA channel number or 0xFF if not available
uint32_t getDMAChannel(TIM_TypeDef* timer);

/// @brief Get DMA controller for a stream
/// @param stream DMA stream pointer
/// @return DMA_TypeDef* (DMA1 or DMA2)
DMA_TypeDef* getDMAController(DMA_Stream_TypeDef* stream);

/// @brief Get stream index (0-7) within its controller
/// @param stream DMA stream pointer
/// @return Stream index (0-7) or 0xFF if invalid
uint8_t getStreamIndex(DMA_Stream_TypeDef* stream);

/// @brief Initialize DMA stream for memory-to-peripheral transfer
/// @param stream DMA stream
/// @param src Source memory address
/// @param dst Destination peripheral address (GPIO ODR register)
/// @param size Transfer size in bytes
/// @param channel DMA channel number for request routing
/// @return true if successful
bool initDMA(DMA_Stream_TypeDef* stream, const void* src, volatile void* dst, uint32_t size, uint32_t channel);

/// @brief Check if DMA transfer is complete
/// @param stream DMA stream
/// @return true if transfer complete, false if still busy
bool isDMAComplete(DMA_Stream_TypeDef* stream);

/// @brief Clear DMA transfer complete flags
/// @param stream DMA stream
void clearDMAFlags(DMA_Stream_TypeDef* stream);

/// @brief Stop DMA transfer
/// @param stream DMA stream
void stopDMA(DMA_Stream_TypeDef* stream);

}  // namespace stm32
}  // namespace fl
