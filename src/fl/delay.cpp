/// @file fl/delay.cpp
/// Implementation for delay utilities

// Define this before including headers to prevent duplicate specializations in delay_cycles_generic.h
#define FL_DELAY_CPP_SPECIALIZATIONS

#include "fl/stdint.h"
#include "fl/delay.h"
#include "fl/types.h"
#include "fl/compiler_control.h"

// ============================================================================
// Platform-specific includes (centralized in platforms/delay.h)
// ============================================================================

#include "platforms/delay.h"

// ============================================================================
// Platform-provided delay functions (from Arduino or platform layer)
// ============================================================================
#ifdef ARDUINO
#include "Arduino.h"  // ok include
#endif

namespace fl {

// ============================================================================
// Runtime delayNanoseconds implementations
// ============================================================================

void delayNanoseconds(fl::u32 ns) {
  // Call platform-specific implementation with auto-detected frequency
  delayNanoseconds_impl(ns);
}

void delayNanoseconds(fl::u32 ns, fl::u32 hz) {
  // Call platform-specific implementation with explicit frequency
  delayNanoseconds_impl(ns, hz);
}

// ============================================================================
// delaycycles template specializations
// ============================================================================
// These pre-instantiated specializations optimize compilation time and code size
// for common cycle counts.

// Specializations for small negative and zero cycles
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() {}

// Specializations for small positive cycles
// Note: These MUST NOT use FASTLED_FORCE_INLINE because with LTO enabled,
// inline functions don't generate external symbols that the linker can resolve.
// Using just inline or no storage class allows the linker to find these.
template<> void delaycycles<1>() { FL_NOP; }
template<> void delaycycles<2>() { FL_NOP2; }
template<> void delaycycles<3>() {
  FL_NOP;
  FL_NOP2;
}
template<> void delaycycles<4>() {
  FL_NOP2;
  FL_NOP2;
}
template<> void delaycycles<5>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP;
}

template<> void delaycycles<6>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP2;
}
template<> void delaycycles<7>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP2;
  FL_NOP;
}
template<> void delaycycles<8>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP2;
  FL_NOP2;
}

// ============================================================================
// Millisecond and Microsecond delay implementations
// ============================================================================

void delayMillis(u32 ms) {
#ifdef ARDUINO
  ::delay((unsigned long)ms);
#endif
}

void delayMicroseconds(u32 us) {
#ifdef ARDUINO
  ::delayMicroseconds((unsigned int)us);
#endif
}



}  // namespace fl

