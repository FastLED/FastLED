#ifndef __INC_LED_SYSDEFS_ARM_RENESAS_H
#define __INC_LED_SYSDEFS_ARM_RENESAS_H

#define FASTLED_ARM

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// reusing/abusing cli/sei defs for due
#define cli()  __disable_irq();
#define sei() __enable_irq();

#define FASTLED_NO_PINMAP

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;

#endif
