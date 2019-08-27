#define cli()  __disable_irq(); 
#define sei() __enable_irq(); 

#undef F_CPU

#ifndef F_CPU
 #define F_CPU 72000000
#endif
