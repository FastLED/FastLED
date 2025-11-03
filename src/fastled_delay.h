#pragma once

#ifndef __INC_FASTLED_DELAY_H
#define __INC_FASTLED_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"
#include "fl/delay.h"

// Note: micros() is used in inline templates below but not declared here.
// It will be provided by platform headers (Arduino.h, etc.) included via led_sysdefs.h
// when FastLED.h includes this file. The actual declaration varies by platform.

/// @file fastled_delay.h
/// Utility functions and classes for managing delay cycles
/// @deprecated Use fl/delay.h for new code




#if (!defined(NO_MINIMUM_WAIT) || (NO_MINIMUM_WAIT==0))

/// Class to ensure that a minimum amount of time has kicked since the last time run - and delay if not enough time has passed yet. 
/// @tparam WAIT The amount of time to wait, in microseconds
template<int WAIT> class CMinWait {
	/// Timestamp of the last time this was run, in microseconds
	fl::u16 mLastMicros;

public:
	/// Constructor
	CMinWait() { mLastMicros = 0; }

	/// Blocking delay until WAIT time since mark() has passed
	void wait() {
		fl::u16 diff;
		do {
			diff = (micros() & 0xFFFF) - mLastMicros;
		} while(diff < WAIT);
	}

	/// Reset the timestamp that marks the start of the wait period
	void mark() { mLastMicros = micros() & 0xFFFF; }
};

#else

// if you keep your own FPS (and therefore don't call show() too quickly for pixels to latch), you may not want a minimum wait.
template<int WAIT> class CMinWait {
public:
	CMinWait() { }
	void wait() { }
	void mark() {}
};

#endif


////////////////////////////////////////////////////////////////////////////////////////////
///
/// @name Clock cycle counted delay loop (DEPRECATED - use fl:: namespace)
///
/// @{
/// @deprecated Use fl::delaycycles() from fl/delay.h instead
///
/// For backwards compatibility, delaycycles is available through fl/delay.h
/// which is included at the top of this file. New code should use
/// fl::delaycycles<CYCLES>() from fl/delay.h.
///
/// Note: The using declarations below are commented out to avoid conflicts
/// with the generic delaycycles template implementation that gets included
/// via platforms/delay.h. Users should access delaycycles via the fl:: namespace.

// using fl::delaycycles;
// using fl::delaycycles_min1;

/// @}


/// @name Some timing related macros/definitions
/// @{

// Macro to convert from nano-seconds to clocks and clocks to nano-seconds
// #define NS(_NS) (_NS / (1000 / (F_CPU / 1000000L)))

/// CPU speed, in megahertz (MHz)
#define F_CPU_MHZ (F_CPU / 1000000L)

// #define NS(_NS) ( (_NS * (F_CPU / 1000000L))) / 1000

/// Convert from nanoseconds to number of clock cycles 
#define NS(_NS) (((_NS * F_CPU_MHZ) + 999) / 1000)
/// Convert from number of clock cycles to microseconds 
#define CLKS_TO_MICROS(_CLKS) ((long)(_CLKS)) / (F_CPU / 1000000L)

/// Macro for making sure there's enough time available
#define NO_TIME(A, B, C) (NS(A) < 3 || NS(B) < 3 || NS(C) < 6)

/// @}


#endif
