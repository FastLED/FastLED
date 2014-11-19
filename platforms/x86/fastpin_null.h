#ifndef __FASTPIN_NULL_H__
#define __FASTPIN_NULL_H__

template<uint8_t PIN> class FastPin {
	static RwReg sPinMask;
	static volatile RwReg *sPort;
	static volatile RoReg *sInPort;
	static void _init() {
	
	}
public:
	typedef volatile RwReg * data_ptr_t;
	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline static void setOutput() {}
	inline static void setInput() { }

	inline static void hi() __attribute__ ((always_inline)) { }
	inline static void lo() __attribute__ ((always_inline)) { }

	inline static void strobe() __attribute__ ((always_inline)) { }

	inline static void toggle() __attribute__ ((always_inline)) {  }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) {  }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { }

	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { }

	static port_t hival() __attribute__ ((always_inline)) {  }
	static port_t loval() __attribute__ ((always_inline)) { }
	static port_ptr_t  port() __attribute__ ((always_inline)) { }
	static port_t mask() __attribute__ ((always_inline)) {  }
};

template<uint8_t PIN> RwReg FastPin<PIN>::sPinMask;
template<uint8_t PIN> volatile RwReg *FastPin<PIN>::sPort;
template<uint8_t PIN> volatile RoReg *FastPin<PIN>::sInPort;

template<uint8_t PIN> class FastPinBB : public FastPin<PIN> {};


#endif // __FASTPIN_NULL_H__
