#if defined(STM32F10X_MD)

 #include <application.h>

 #define FASTLED_NAMESPACE_BEGIN namespace NSFastLED {
 #define FASTLED_NAMESPACE_END }
 #define FASTLED_USING_NAMESPACE using namespace NSFastLED;

 // reusing/abusing cli/sei defs for due
 #define cli()  __disable_irq(); __disable_fault_irq();
 #define sei() __enable_irq(); __enable_fault_irq();

#elif defined (__STM32F1__)

 #include "cm3_regs.h"

 #define cli() nvic_globalirq_disable()
 #define sei() nvic_globalirq_enable()
 #endif

#ifndef F_CPU
 #define F_CPU 72000000
#endif