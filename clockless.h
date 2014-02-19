#ifndef __INC_CLOCKLESS_H
#define __INC_CLOCKLESS_H

#include "controller.h"
#include "lib8tion.h"
#include "led_sysdefs.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convinience macros to wrap around the toggling of hi vs. lo
#define SET_LO FLIP ? FastPin<DATA_PIN>::fastset(port, hi) : FastPin<DATA_PIN>::fastset(port, lo); 
#define SET_HI FLIP ? FastPin<DATA_PIN>::fastset(port, lo) : FastPin<DATA_PIN>::fastset(port, hi); 

// #include "clockless_avr.h"
#include "clockless_trinket.h"
#include "clockless_arm_k20.h"
#include "clockless_arm_sam.h"
#include "clockless2.h"
#include "block_clockless.h"

#endif
