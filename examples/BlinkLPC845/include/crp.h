/*
 * crp.h
 *
 * Copyright 2013-2014, 2017-2018, 2020-2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Code Read Protection macros.
 *
 */

#ifndef _CRP_H_INCLUDED_
#define _CRP_H_INCLUDED_

// A macro for placing data into the Code Read Protect (CRP) section, 
// which is then located at the correct address for the selected MCU
// by the automatically generated linker script. The CRP section should 
// contain a single 32-bit value which is the CRP value. See appropriate
// documentation for the MCU to determine CRP values.
//
// This feature is only available for NXP MCU targets with the Code Read
// Protect Feature
//
// Example:
//        __CRP const uint32_t CRP_WORD = CRP_NO_CRP ;
//
#define __CRP __attribute__ ((used,section(".crp")))

#define CRP_NO_CRP          0xFFFFFFFF

// Disables UART and USB In System Programming (reads and writes)
// Leaves SWD debugging, with reads and writes, enabled
#define CRP_NO_ISP    0x4E697370

// Disables SWD debugging & JTAG, leaves ISP with with reads and writes enabled
// You will need UART connectivity and FlashMagic (flashmagictool.com) to reverse
// this. Don't even try this without these tools; most likely the SWD flash
// programming will not even complete.
// Allows reads and writes only to RAM above 0x10000300 and flash other than
// sector 0 (the first 4 kB). Full erase also allowed- again only through UART
// and FlashMagic (NO JTAG/SWD)
#define CRP_CRP1      0x12345678

// Disables SWD debugging & JTAG, leaves UART ISP with with only full erase
// enabled. You must have UART access and FlashMagic before setting this
// option.
// Don't even try this without these tools; most likely the SWD flash
// programming will not even complete.
#define CRP_CRP2      0x87654321

/************************************************************/
/**** DANGER CRP3 WILL LOCK PART TO ALL READS and WRITES ****/
#define CRP_CRP3_CONSUME_PART 0x43218765
/************************************************************/

#if CONFIG_CRP_SETTING_NO_CRP == 1
#define CURRENT_CRP_SETTING CRP_NO_CRP
#endif

#if CONFIG_CRP_SETTING_NOISP == 1
#define CURRENT_CRP_SETTING CRP_NO_ISP
#endif

#if CONFIG_CRP_SETTING_CRP1 == 1
#define CURRENT_CRP_SETTING CRP_CRP1
#endif

#if CONFIG_CRP_SETTING_CRP2 == 1
#define CURRENT_CRP_SETTING CRP_CRP2
#endif

#if CONFIG_CRP_SETTING_CRP3_CONSUME_PART == 1
#define CURRENT_CRP_SETTING CRP_CRP3_CONSUME_PART
#endif

#ifndef CURRENT_CRP_SETTING
#define CURRENT_CRP_SETTING CRP_NO_CRP
#endif

 
#endif /* _CRP_H_INCLUDED_ */
