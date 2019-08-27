#ifdef GPIOA
_FL_IO(A,0);
#endif
#ifdef GPIOB
_FL_IO(B,1);
#endif
#ifdef GPIOC
_FL_IO(C,2);
#endif
#ifdef GPIOD
_FL_IO(D,3);
#endif
#ifdef GPIOE
_FL_IO(E,4);
#endif
#ifdef GPIOF
_FL_IO(F,5);
#endif
#ifdef GPIOG
_FL_IO(G,6);
#endif


// Actual pin definitions
#if defined(SPARK) // Sparkfun STM32F103 based board

#define MAX_PIN 19
_FL_DEFPIN(0, 7, B);
_FL_DEFPIN(1, 6, B);
_FL_DEFPIN(2, 5, B);
_FL_DEFPIN(3, 4, B);
_FL_DEFPIN(4, 3, B);
_FL_DEFPIN(5, 15, A);
_FL_DEFPIN(6, 14, A);
_FL_DEFPIN(7, 13, A);
_FL_DEFPIN(8, 8, A);
_FL_DEFPIN(9, 9, A);
_FL_DEFPIN(10, 0, A);
_FL_DEFPIN(11, 1, A);
_FL_DEFPIN(12, 4, A);
_FL_DEFPIN(13, 5, A);
_FL_DEFPIN(14, 6, A);
_FL_DEFPIN(15, 7, A);
_FL_DEFPIN(16, 0, B);
_FL_DEFPIN(17, 1, B);
_FL_DEFPIN(18, 3, A);
_FL_DEFPIN(19, 2, A);


#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

#endif // SPARK

#if defined(__STM32F1__) // Generic STM32F103 aka "Blue Pill"

#define MAX_PIN 46

_FL_DEFPIN(10, 0, A);	// PA0 - PA7
_FL_DEFPIN(11, 1, A);
_FL_DEFPIN(12, 2, A);
_FL_DEFPIN(13, 3, A);
_FL_DEFPIN(14, 4, A);
_FL_DEFPIN(15, 5, A);
_FL_DEFPIN(16, 6, A);
_FL_DEFPIN(17, 7, A);
_FL_DEFPIN(29, 8, A);	// PA8 - PA15
_FL_DEFPIN(30, 9, A);
_FL_DEFPIN(31, 10, A);
_FL_DEFPIN(32, 11, A);
_FL_DEFPIN(33, 12, A);
_FL_DEFPIN(34, 13, A);
_FL_DEFPIN(37, 14, A);
_FL_DEFPIN(38, 15, A);

_FL_DEFPIN(18, 0, B);	// PB0 - PB11
_FL_DEFPIN(19, 1, B);
_FL_DEFPIN(20, 2, B);
_FL_DEFPIN(39, 3, B);
_FL_DEFPIN(40, 4, B);
_FL_DEFPIN(41, 5, B);
_FL_DEFPIN(42, 6, B);
_FL_DEFPIN(43, 7, B);
_FL_DEFPIN(45, 8, B);
_FL_DEFPIN(46, 9, B);
_FL_DEFPIN(21, 10, B);
_FL_DEFPIN(22, 11, B);

_FL_DEFPIN(2, 13, C);	// PC13 - PC15
_FL_DEFPIN(3, 14, C);
_FL_DEFPIN(4, 15, C);

#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN

#define HAS_HARDWARE_PIN_SUPPORT

#endif // __STM32F1__