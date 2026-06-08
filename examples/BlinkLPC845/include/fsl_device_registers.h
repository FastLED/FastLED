/*
 * Copyright 2014-2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2018 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __FSL_DEVICE_REGISTERS_H__
#define __FSL_DEVICE_REGISTERS_H__

/*
 * Include the cpu specific register header files.
 *
 * The CPU macro should be declared in the project or makefile.
 */
#if (defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || defined(CPU_LPC845M301JHI33) || \
    defined(CPU_LPC845M301JHI48))

#define LPC845_SERIES

/* CMSIS-style register definitions */
#include "LPC845.h"
/* CPU specific feature definitions */
#include "LPC845_features.h"

#else
    #error "No valid CPU defined!"
#endif

#endif /* __FSL_DEVICE_REGISTERS_H__ */

/*******************************************************************************
 * EOF
 ******************************************************************************/
