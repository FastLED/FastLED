// ok no namespace fl
#pragma once

/// @file cm3_regs.h
/// ARM Cortex-M Debug Watch and Trace (DWT) and CoreDebug register definitions.
///
/// Fallback for legacy libmaple core which lacks CMSIS headers.
/// For STM32duino, use stm32_def.h instead which includes core_cmX.h.

#include "fl/stl/stdint.h"

#ifndef CoreDebug_BASE

// CMSIS-style register access qualifiers
#ifndef __I
#ifdef __cplusplus
#define   __I     volatile             /*!< Defines 'read only' permissions                 */
#else
#define   __I     volatile const       /*!< Defines 'read only' permissions                 */
#endif
#endif

#ifndef __O
#define   __O     volatile             /*!< Defines 'write only' permissions                */
#endif

#ifndef __IO
#define   __IO    volatile             /*!< Defines 'read / write' permissions              */
#endif

typedef struct
{
    __IO fl::u32 DHCSR;                   /*!< Offset: 0x000 (R/W)  Debug Halting Control and Status Register    */
    __O  fl::u32 DCRSR;                   /*!< Offset: 0x004 ( /W)  Debug Core Register Selector Register        */
    __IO fl::u32 DCRDR;                   /*!< Offset: 0x008 (R/W)  Debug Core Register Data Register            */
    __IO fl::u32 DEMCR;                   /*!< Offset: 0x00C (R/W)  Debug Exception and Monitor Control Register */
} CoreDebug_Type;

#define CoreDebug_BASE      (0xE000EDF0UL)                            /*!< Core Debug Base Address            */
#define CoreDebug           ((CoreDebug_Type *)     CoreDebug_BASE)   /*!< Core Debug configuration struct    */

#define CoreDebug_DEMCR_TRCENA_Pos         24                                             /*!< CoreDebug DEMCR: TRCENA Position */
#define CoreDebug_DEMCR_TRCENA_Msk         (1UL << CoreDebug_DEMCR_TRCENA_Pos)            /*!< CoreDebug DEMCR: TRCENA Mask */

typedef struct
{
    __IO fl::u32 CTRL;                    /*!< Offset: 0x000 (R/W)  Control Register                          */
    __IO fl::u32 CYCCNT;                  /*!< Offset: 0x004 (R/W)  Cycle Count Register                      */
    __IO fl::u32 CPICNT;                  /*!< Offset: 0x008 (R/W)  CPI Count Register                        */
    __IO fl::u32 EXCCNT;                  /*!< Offset: 0x00C (R/W)  Exception Overhead Count Register         */
    __IO fl::u32 SLEEPCNT;                /*!< Offset: 0x010 (R/W)  Sleep Count Register                      */
    __IO fl::u32 LSUCNT;                  /*!< Offset: 0x014 (R/W)  LSU Count Register                        */
    __IO fl::u32 FOLDCNT;                 /*!< Offset: 0x018 (R/W)  Folded-instruction Count Register         */
    __I  fl::u32 PCSR;                    /*!< Offset: 0x01C (R/ )  Program Counter Sample Register           */
    __IO fl::u32 COMP0;                   /*!< Offset: 0x020 (R/W)  Comparator Register 0                     */
    __IO fl::u32 MASK0;                   /*!< Offset: 0x024 (R/W)  Mask Register 0                           */
    __IO fl::u32 FUNCTION0;               /*!< Offset: 0x028 (R/W)  Function Register 0                       */
          fl::u32 RESERVED0[1];
    __IO fl::u32 COMP1;                   /*!< Offset: 0x030 (R/W)  Comparator Register 1                     */
    __IO fl::u32 MASK1;                   /*!< Offset: 0x034 (R/W)  Mask Register 1                           */
    __IO fl::u32 FUNCTION1;               /*!< Offset: 0x038 (R/W)  Function Register 1                       */
          fl::u32 RESERVED1[1];
    __IO fl::u32 COMP2;                   /*!< Offset: 0x040 (R/W)  Comparator Register 2                     */
    __IO fl::u32 MASK2;                   /*!< Offset: 0x044 (R/W)  Mask Register 2                           */
    __IO fl::u32 FUNCTION2;               /*!< Offset: 0x048 (R/W)  Function Register 2                       */
          fl::u32 RESERVED2[1];
    __IO fl::u32 COMP3;                   /*!< Offset: 0x050 (R/W)  Comparator Register 3                     */
    __IO fl::u32 MASK3;                   /*!< Offset: 0x054 (R/W)  Mask Register 3                           */
    __IO fl::u32 FUNCTION3;               /*!< Offset: 0x058 (R/W)  Function Register 3                       */
} DWT_Type;


#define DWT_BASE            (0xE0001000UL)                            /*!< DWT Base Address                   */
#define DWT                 ((DWT_Type       *)     DWT_BASE      )   /*!< DWT configuration struct           */

#define DWT_CTRL_CYCCNTENA_Pos              0                                          /*!< DWT CTRL: CYCCNTENA Position */
#define DWT_CTRL_CYCCNTENA_Msk             (0x1UL << DWT_CTRL_CYCCNTENA_Pos)           /*!< DWT CTRL: CYCCNTENA Mask */

#endif // CoreDebug_BASE
