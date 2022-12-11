#ifndef __FASTPIN_ARM_NRF51_H
#define __FASTPIN_ARM_NRF51_H

#if defined(NRF51)
/// Template definition for teensy 3.0 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction
#if 0
template<uint8_t PIN, uint32_t _MASK, typename _DIRSET, typename _DIRCLR, typename _OUTSET, typename _OUTCLR, typename _OUT> class _ARMPIN {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    inline static void setOutput() { _DIRSET::r() = _MASK; }
    inline static void setInput() { _DIRCLR::r() = _MASK; }

    inline static void hi() __attribute__ ((always_inline)) { _OUTSET::r() = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _OUTCLR::r() = _MASK; }
    inline static void set(REGISTER port_t val) __attribute__ ((always_inline)) { _OUT::r() = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { _OUT::r() ^= _MASK; }

    inline static void hi(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(REGISTER port_ptr_t port, REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return _OUT::r() | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return _OUT::r() & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_OUT::r(); }
    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define ADDR(X) *(volatile uint32_t*)X
#define NR_GPIO_ADDR(base,offset) (*(volatile uint32_t *))((uint32_t)(base + offset))
#define NR_DIRSET ADDR(0x50000518UL) // NR_GPIO_ADDR(NRF_GPIO_BASE, 0x518)
#define NR_DIRCLR ADDR(0x5000051CUL) // NR_GPIO_ADDR(NRF_GPIO_BASE, 0x51C)
#define NR_OUTSET ADDR(0x50000508UL) // NR_GPIO_ADDR(NRF_GPIO_BASE, 0x508)
#define NR_OUTCLR ADDR(0x5000050CUL) // NR_GPIO_ADDR(NRF_GPIO_BASE, 0x50C)
#define NR_OUT ADDR(0x50000504UL) // NR_GPIO_ADDR(NRF_GPIO_BASE, 0x504)

#define _RD32_NRF(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline reg32_t r() { return T; }};

_RD32_NRF(NR_DIRSET);
_RD32_NRF(NR_DIRCLR);
_RD32_NRF(NR_OUTSET);
_RD32_NRF(NR_OUTCLR);
_RD32_NRF(NR_OUT);

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _ARMPIN<PIN, 1 << PIN, \
    _R(NR_DIRSET), _R(NR_DIRCLR), _R(NR_OUTSET), _R(NR_OUTCLR), _R(NR_OUT)> {};
#else

typedef struct {                                    /*!< GPIO Structure                                                        */
    // __I  uint32_t  RESERVED0[321];
    __IO uint32_t  OUT;                               /*!< Write GPIO port.                                                      */
    __IO uint32_t  OUTSET;                            /*!< Set individual bits in GPIO port.                                     */
    __IO uint32_t  OUTCLR;                            /*!< Clear individual bits in GPIO port.                                   */
    __I  uint32_t  IN;                                /*!< Read GPIO port.                                                       */
    __IO uint32_t  DIR;                               /*!< Direction of GPIO pins.                                               */
    __IO uint32_t  DIRSET;                            /*!< DIR set register.                                                     */
    __IO uint32_t  DIRCLR;                            /*!< DIR clear register.                                                   */
    __I  uint32_t  RESERVED1[120];
    __IO uint32_t  PIN_CNF[32];                       /*!< Configuration of GPIO pins.                                           */
} FL_NRF_GPIO_Type;

#define FL_NRF_GPIO_BASE                   0x50000504UL
#define FL_NRF_GPIO                        ((FL_NRF_GPIO_Type           *) FL_NRF_GPIO_BASE)

template<uint8_t PIN, uint32_t _MASK> class _ARMPIN {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    inline static void setOutput() { FL_NRF_GPIO->DIRSET = _MASK; }
    inline static void setInput() { FL_NRF_GPIO->DIRCLR = _MASK; }

    inline static void hi() __attribute__ ((always_inline)) { FL_NRF_GPIO->OUTSET = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { FL_NRF_GPIO->OUTCLR= _MASK; }
    inline static void set(REGISTER port_t val) __attribute__ ((always_inline)) { FL_NRF_GPIO->OUT = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { FL_NRF_GPIO->OUT ^= _MASK; }

    inline static void hi(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(REGISTER port_ptr_t port, REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return FL_NRF_GPIO->OUT | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return FL_NRF_GPIO->OUT & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &FL_NRF_GPIO->OUT; }
    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__ ((always_inline)) { return (FL_NRF_GPIO->IN & _MASK) != 0; }
};


#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _ARMPIN<PIN, 1 << PIN> {};
#endif

// Actual pin definitions
#define MAX_PIN 31
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);

#define HAS_HARDWARE_PIN_SUPPORT

#endif

#endif
