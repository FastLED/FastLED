# STM32 Parallel SPI Hardware Resource Allocation Guide

## Overview

This document defines the hardware resource allocation strategy for STM32 parallel SPI implementations. It covers Timer, DMA, and GPIO resource requirements, allocation strategies, and platform-specific considerations for all STM32 variants.

## Table of Contents

1. [Resource Requirements Summary](#resource-requirements-summary)
2. [Timer Peripheral Allocation](#timer-peripheral-allocation)
3. [DMA Channel Allocation](#dma-channel-allocation)
4. [GPIO Pin Requirements](#gpio-pin-requirements)
5. [Platform-Specific Notes](#platform-specific-notes)
6. [Resource Conflict Resolution](#resource-conflict-resolution)
7. [Configuration Examples](#configuration-examples)

---

## Resource Requirements Summary

### Per-Mode Requirements

| Mode | Timer | DMA Channels | GPIO Pins | Notes |
|------|-------|--------------|-----------|-------|
| **Single-SPI** | 0 | 0 | 2 (CLK, MOSI) | Uses hardware SPI peripheral or software bitbang |
| **Dual-SPI** | 1 | 2 | 3 (CLK, D0, D1) | 2x throughput vs single-lane |
| **Quad-SPI** | 1 | 4 | 5 (CLK, D0-D3) | 4x throughput vs single-lane |
| **Octal-SPI** | 1 | 8 | 9 (CLK, D0-D7) | 8x throughput vs single-lane |

### Multi-Bus Scenarios

**Example 1: Dual + Quad + Single**
- 2 Timer peripherals
- 6 DMA channels (2 + 4)
- 8 GPIO pins (3 + 5)
- Good balance for mixed LED strip configurations

**Example 2: Two Octal Buses**
- 2 Timer peripherals
- 16 DMA channels (8 + 8)
- 18 GPIO pins (9 + 9)
- Maximum throughput, uses all DMA resources on most STM32

**Example 3: Four Dual Buses**
- 4 Timer peripherals
- 8 DMA channels (2 × 4)
- 12 GPIO pins (3 × 4)
- Best for many independent LED strips

---

## Timer Peripheral Allocation

### Timer Selection Strategy

**Priority Order:**
1. TIM2 - General-purpose, 32-bit on most STM32
2. TIM3 - General-purpose, 16-bit
3. TIM4 - General-purpose, 16-bit
4. TIM5 - General-purpose, 32-bit (on F4/F7/H7)
5. TIM8 - Advanced timer (if not used for motor control)

**Avoid:**
- TIM1 - Often used for critical PWM applications
- TIM6/TIM7 - Basic timers without output compare (can't drive GPIO)
- TIM9-TIM14 - Limited availability, low-level functions

### Timer Configuration

**Mode:** PWM Generation Mode 1
- **Purpose:** Generate clock signal for parallel SPI
- **Output:** GPIO pin configured as Alternate Function (Timer CHx)
- **Frequency:** 1-10 MHz typical for LED strips (configurable)

**Register Settings:**
```c
// Pseudo-code for Timer setup
TIM_HandleTypeDef htim;
htim.Instance = TIMx;  // TIM2, TIM3, TIM4, etc.
htim.Init.Prescaler = 0;  // No prescaling for maximum frequency
htim.Init.CounterMode = TIM_COUNTERMODE_UP;
htim.Init.Period = (Timer_Clock / desired_frequency) - 1;
htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

// PWM Channel Configuration
TIM_OC_InitTypeDef sConfigOC;
sConfigOC.OCMode = TIM_OCMODE_PWM1;
sConfigOC.Pulse = htim.Init.Period / 2;  // 50% duty cycle
sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
```

**Clock Frequency Calculation:**
- STM32F1: APB1 timer clock typically 72 MHz
- STM32F4: APB1 timer clock typically 84-90 MHz
- STM32H7: APB1 timer clock typically 100-200 MHz
- For 5 MHz SPI clock: `Period = (Timer_Clock / 5000000) - 1`

### Timer-to-GPIO Pin Mapping

**TIM2 Channels (Example for STM32F4):**
- CH1: PA0, PA5, PA15
- CH2: PA1, PB3
- CH3: PA2, PB10
- CH4: PA3, PB11

**TIM3 Channels (Example for STM32F4):**
- CH1: PA6, PB4, PC6
- CH2: PA7, PB5, PC7
- CH3: PB0, PC8
- CH4: PB1, PC9

**TIM4 Channels (Example for STM32F4):**
- CH1: PB6, PD12
- CH2: PB7, PD13
- CH3: PB8, PD14
- CH4: PB9, PD15

**Note:** Pin availability varies by STM32 package (QFP48, LQFP64, LQFP100, etc.)

---

## DMA Channel Allocation

### DMA Controllers Overview

**STM32F1/F4/G4/L4:**
- 2 DMA controllers (DMA1, DMA2)
- 8 streams/channels each
- **Total: 16 DMA channels**

**STM32H7:**
- 2 MDMA controllers
- 16 channels each
- DMAMUX for flexible request routing
- **Total: 32 DMA channels** (more flexible)

**STM32U5:**
- 4 GPDMA controllers
- Up to 16 channels per controller
- **Total: 64 DMA channels** (highly flexible)

### DMA Allocation Strategy

**Priority Scheme:**

1. **Reserve for Critical Functions:**
   - UART RX/TX (if needed for debugging)
   - ADC (if analog sensors used)
   - SPI/I2C (if communication peripherals needed)

2. **Allocate for Parallel SPI:**
   - Use contiguous DMA channels per bus when possible
   - Prefer DMA2 channels (often faster on STM32F4)

**Example Allocation (STM32F4):**
```
DMA1:
  Stream 0-3: Reserved for peripherals (UART, I2C, etc.)
  Stream 4-7: Available for Parallel SPI

DMA2:
  Stream 0-7: Available for Parallel SPI (preferred)
```

**Dual-SPI Bus Allocation:**
- Bus 0: DMA2 Stream 0-1 (2 channels)
- Bus 1: DMA2 Stream 2-3 (2 channels)
- Bus 2: DMA2 Stream 4-5 (2 channels)
- Bus 3: DMA2 Stream 6-7 (2 channels)

**Quad-SPI Bus Allocation:**
- Bus 0: DMA2 Stream 0-3 (4 channels)
- Bus 1: DMA2 Stream 4-7 (4 channels)

**Octal-SPI Bus Allocation:**
- Bus 0: DMA2 Stream 0-7 (8 channels)
- Bus 1: DMA1 Stream 0-7 (8 channels)

### DMA Configuration

**Transfer Mode:** Memory to Memory (or Memory to Peripheral)
- **Source:** DMA buffer in RAM (allocated by driver)
- **Destination:** GPIO ODR register (for bitbang) or peripheral register
- **Trigger:** Timer Update Event
- **Mode:** Normal (not circular) - transfer once per frame

**Register Settings:**
```c
// Pseudo-code for DMA setup
DMA_HandleTypeDef hdma;
hdma.Instance = DMA2_Stream0;  // Or appropriate stream
hdma.Init.Channel = DMA_CHANNEL_0;  // Or appropriate channel/request
hdma.Init.Direction = DMA_MEMORY_TO_PERIPH;
hdma.Init.PeriphInc = DMA_PINC_DISABLE;  // Fixed peripheral address
hdma.Init.MemInc = DMA_MINC_ENABLE;      // Increment memory address
hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
hdma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
hdma.Init.Mode = DMA_NORMAL;  // Not circular
hdma.Init.Priority = DMA_PRIORITY_HIGH;  // Or VERY_HIGH for time-critical
hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
```

**DMA-Timer Linkage:**
```c
// Link DMA to Timer Update event
__HAL_LINKDMA(&htim, hdma[TIM_DMA_ID_UPDATE], &hdma);

// Enable Timer DMA request
__HAL_TIM_ENABLE_DMA(&htim, TIM_DMA_UPDATE);

// Start DMA transfer
HAL_DMA_Start(&hdma, (uint32_t)buffer, (uint32_t)&GPIOx->ODR, buffer_size);

// Start Timer
HAL_TIM_PWM_Start(&htim, TIM_CHANNEL_1);
```

---

## GPIO Pin Requirements

### Pin Speed Settings

**Clock Pin (Timer Output):**
- **Speed:** GPIO_SPEED_FREQ_VERY_HIGH (100+ MHz on STM32F4/H7)
- **Mode:** Alternate Function (Timer CHx)
- **Pull:** None (external pull-up/down if needed)
- **Output Type:** Push-Pull

**Data Pins (DMA-Controlled):**
- **Speed:** GPIO_SPEED_FREQ_HIGH (50+ MHz) or VERY_HIGH
- **Mode:** Output (Push-Pull)
- **Pull:** None
- **Output Type:** Push-Pull

### Pin Assignment Recommendations

**Port Grouping (Optional but Recommended):**
- Use same GPIO port for all data pins when possible
- Allows single 32-bit write to ODR register
- Reduces DMA overhead (not critical for LED timing)

**Example: Octal-SPI on Port B:**
```
Clock: PA5 (TIM2_CH1)
Data Lanes: PB0-PB7 (8 consecutive pins)
```

**Alternate: Scattered Pins:**
```
Clock: PA5 (TIM2_CH1)
Data0: PA0
Data1: PA1
Data2: PB0
Data3: PB1
Data4: PC0
Data5: PC1
Data6: PD0
Data7: PD1
```

**GPIO Configuration:**
```c
GPIO_InitTypeDef GPIO_InitStruct = {0};

// Clock pin (Timer Alternate Function)
GPIO_InitStruct.Pin = CLOCK_PIN;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;  // Or appropriate AF
HAL_GPIO_Init(CLK_GPIO_PORT, &GPIO_InitStruct);

// Data pins (GPIO Output)
for (int i = 0; i < num_lanes; i++) {
    GPIO_InitStruct.Pin = data_pins[i];
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DATA_GPIO_PORT[i], &GPIO_InitStruct);
}
```

---

## Platform-Specific Notes

### STM32F1 (e.g., Blue Pill)

**Clock:** 72 MHz max
**Timers:** TIM1-4 available (TIM1 advanced, TIM2-4 general-purpose)
**DMA:** 2 controllers × 7 channels = 14 total
**GPIO Speed:** 50 MHz max
**Recommendation:** Dual-SPI or Quad-SPI (limited DMA channels)

**Example Configuration:**
- 1x Quad-SPI bus (TIM2 + DMA1 CH1-4)
- 1x Dual-SPI bus (TIM3 + DMA1 CH5-6)
- Leaves DMA1 CH7 free for other uses

### STM32F4 (e.g., F401, F411, F429)

**Clock:** 84-180 MHz
**Timers:** TIM1-14 available
**DMA:** 2 controllers × 8 streams = 16 total
**GPIO Speed:** 100 MHz typical
**Recommendation:** Best balance - can run 2x Octal-SPI or 4x Quad-SPI

**Example Configuration (High Throughput):**
- 1x Octal-SPI bus (TIM2 + DMA2 Stream 0-7)
- 1x Octal-SPI bus (TIM3 + DMA1 Stream 0-7)
- Total: 16 data lanes across 2 buses

**Example Configuration (Flexibility):**
- 2x Quad-SPI buses (TIM2/TIM3 + DMA2 Stream 0-7)
- 4x Dual-SPI buses possible with DMA1 + DMA2

### STM32L4 (Low Power)

**Clock:** 80 MHz max
**Timers:** TIM1-2, TIM15-17 available
**DMA:** 2 controllers × 7 channels = 14 total
**GPIO Speed:** 80 MHz max
**QSPI Peripheral:** Yes (L476, L486) - native 4-lane SPI alternative
**Recommendation:** Use QSPI peripheral for Quad-SPI when available, or Dual-SPI with GPIO+Timer+DMA

**Example Configuration:**
- 1x QSPI peripheral (hardware quad-lane SPI)
- 1x Dual-SPI bus (TIM2 + DMA1 CH1-2)

### STM32H7 (High Performance)

**Clock:** 400-480 MHz
**Timers:** TIM1-17 available
**DMA:** 2 MDMA + BDMA + DMAMUX
**DMA Channels:** 16 per MDMA (32 total) + flexible routing
**GPIO Speed:** 100 MHz max
**QSPI/OSPI Peripheral:** Yes - native multi-lane SPI
**Recommendation:** Best platform for parallel SPI - can run 4x Octal-SPI buses

**Example Configuration (Maximum Throughput):**
- 2x OSPI peripherals (hardware octal-lane SPI)
- 2x Octal-SPI buses via GPIO+Timer+DMA
- Total: 32 data lanes across 4 buses

**DMAMUX Advantage:**
- Flexible routing of DMA requests to any channel
- Eliminates fixed DMA-peripheral mappings
- Simplifies multi-bus allocation

### STM32G4 (Motor Control Focus)

**Clock:** 170 MHz max
**Timers:** TIM1-8, TIM15-17 available
**DMA:** 2 controllers × 8 channels = 16 total
**GPIO Speed:** 170 MHz max (very fast)
**Recommendation:** Excellent for parallel SPI - high GPIO speed helps

**Example Configuration:**
- 2x Quad-SPI buses (TIM2/TIM3 + DMA1/DMA2)
- Or 4x Dual-SPI buses for flexibility

### STM32U5 (Ultra Low Power)

**Clock:** 160 MHz max
**Timers:** TIM1-8, TIM15-17 available
**DMA:** 4 GPDMA controllers × 16 channels = 64 total
**GPIO Speed:** 160 MHz max
**OSPI Peripheral:** Yes - native octal-lane SPI
**Recommendation:** Most flexible DMA allocation - can run 8x Octal-SPI buses theoretically

**Example Configuration:**
- Limited only by Timer/GPIO availability
- Can easily support 4x Quad-SPI or 2x Octal-SPI + 4x Dual-SPI

---

## Resource Conflict Resolution

### Common Conflicts

**1. Timer Already Used for Other PWM:**
- **Solution:** Use alternate timer (TIM3 instead of TIM2)
- **Check:** Review pin alternate function table

**2. DMA Channel Already Used:**
- **Solution:** Reallocate DMA streams/channels
- **STM32F4:** Use different DMA controller
- **STM32H7/U5:** Use DMAMUX to route to free channel

**3. GPIO Pin Already Used:**
- **Solution:** Select alternate pin for same Timer channel
- **Example:** TIM2_CH1 can be PA0, PA5, or PA15 (STM32F4)

**4. Insufficient DMA Channels:**
- **Solution:** Reduce parallel SPI bus count or lane count
- **Example:** Use 2x Quad instead of 1x Octal + 1x Quad

### Debugging Resource Issues

**Check Available Resources:**
```c
// Verify Timer clock enabled
__HAL_RCC_TIM2_IS_CLK_ENABLED()

// Verify DMA clock enabled
__HAL_RCC_DMA1_IS_CLK_ENABLED()
__HAL_RCC_DMA2_IS_CLK_ENABLED()

// Verify GPIO clock enabled
__HAL_RCC_GPIOA_IS_CLK_ENABLED()
```

**Verify Pin Alternate Functions:**
- Consult STM32 datasheet "Alternate Function Mapping" table
- Ensure selected pin supports desired Timer channel
- Check for conflicts with other peripherals (USART, SPI, I2C)

---

## Configuration Examples

### Example 1: Simple Dual-SPI Setup (STM32F4)

**Hardware:**
- 1x LED strip (WS2812B, 150 LEDs)
- STM32F401 (Black Pill board)

**Resources:**
- Timer: TIM2
- DMA: DMA2 Stream 0-1
- GPIO: PA5 (clock), PA0 (D0), PA1 (D1)

**Code Snippet:**
```cpp
// In user code
#include "FastLED.h"

#define NUM_LEDS 150
#define DATA_PIN_0 PA0
#define DATA_PIN_1 PA1
#define CLOCK_PIN PA5

CRGB leds[NUM_LEDS];

void setup() {
    // FastLED will automatically detect and use Dual-SPI
    FastLED.addLeds<WS2812, DATA_PIN_0>(leds, NUM_LEDS);
}
```

### Example 2: Quad-SPI for High LED Count (STM32F4)

**Hardware:**
- 4x LED strips (WS2812B, 100 LEDs each = 400 total)
- STM32F411 (WeAct board)

**Resources:**
- Timer: TIM2
- DMA: DMA2 Stream 0-3
- GPIO: PA5 (clock), PA0 (D0), PA1 (D1), PA2 (D2), PA3 (D3)

**Throughput:**
- Single-SPI: ~800 kHz → 25 µs per LED × 400 LEDs = 10 ms/frame
- Quad-SPI: ~3.2 MHz effective → 6.25 µs per LED × 400 LEDs = 2.5 ms/frame

### Example 3: Dual Octal-SPI (STM32H7)

**Hardware:**
- 16x LED strips (APA102, 50 LEDs each = 800 total)
- STM32H743 (Nucleo-H743ZI)

**Resources:**
- Timer: TIM2, TIM3
- DMA: MDMA1 CH0-7 (Bus 0), MDMA2 CH0-7 (Bus 1)
- GPIO:
  - Bus 0: PA5 (clock), PB0-7 (D0-D7)
  - Bus 1: PC6 (clock), PD0-7 (D0-D7)

**Configuration:**
```cpp
// FastLED automatically detects and allocates 2 octal-SPI buses
// No special configuration needed - just add LED strips
```

### Example 4: Mixed Mode (STM32F4)

**Hardware:**
- 2x LED strips on Quad-SPI (WS2812B)
- 2x LED strips on Dual-SPI (SK6812)
- 1x LED strip on Single-SPI (APA102)

**Resources:**
- Quad-SPI: TIM2 + DMA2 Stream 0-3 + PA5/PA0-3
- Dual-SPI: TIM3 + DMA1 Stream 0-1 + PB4/PB0-1
- Single-SPI: Hardware SPI1 + PA7 (MOSI) + PA5 (SCK)

**Total DMA Usage:** 6 channels out of 16 available

---

## Hardware Integration Checklist

When implementing hardware support for a specific STM32 variant:

- [ ] Identify available Timer peripherals (TIM2-5 preferred)
- [ ] Identify available DMA channels (16 total on F1/F4/G4)
- [ ] Map Timer channels to GPIO pins (consult datasheet)
- [ ] Enable RCC clocks for Timer, DMA, GPIO
- [ ] Configure Timer for PWM mode (clock generation)
- [ ] Configure DMA channels (Memory → GPIO ODR)
- [ ] Link DMA requests to Timer Update event
- [ ] Implement DMA completion polling/interrupt
- [ ] Test with logic analyzer (verify clock + data timing)
- [ ] Validate LED strip control (visual test)
- [ ] Performance benchmark (measure frame rate)
- [ ] Document configuration in platform-specific guide

---

## References

- **STM32 Reference Manuals:** RM0008 (F1), RM0090 (F4), RM0433 (H7), RM0440 (G4), RM0456 (U5)
- **STM32 Datasheets:** Refer to specific variant datasheet for pin mappings
- **FastLED Documentation:** See `src/platforms/shared/spi_hw_*.h` for interface definitions
- **STM32 HAL Documentation:** `stm32XXxx_hal_tim.h`, `stm32XXxx_hal_dma.h`, `stm32XXxx_hal_gpio.h`

---

**Document Version:** 1.0
**Last Updated:** 2025-10-16
**Iteration:** 9
