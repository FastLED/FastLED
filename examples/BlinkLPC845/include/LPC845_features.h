/*
** ###################################################################
**     Version:             rev. 1.2, 2017-06-08
**     Build:               b220714
**
**     Abstract:
**         Chip specific module features.
**
**     Copyright 2016 Freescale Semiconductor, Inc.
**     Copyright 2016-2022 NXP
**     All rights reserved.
**
**     SPDX-License-Identifier: BSD-3-Clause
**
**     http:                 www.nxp.com
**     mail:                 support@nxp.com
**
**     Revisions:
**     - rev. 1.0 (2016-08-12)
**         Initial version.
**     - rev. 1.1 (2016-11-25)
**         Update CANFD and Classic CAN register.
**         Add MAC TIMERSTAMP registers.
**     - rev. 1.2 (2017-06-08)
**         Remove RTC_CTRL_RTC_OSC_BYPASS.
**         SYSCON_ARMTRCLKDIV rename to SYSCON_ARMTRACECLKDIV.
**         Remove RESET and HALT from SYSCON_AHBCLKDIV.
**
** ###################################################################
*/

#ifndef _LPC845_FEATURES_H_
#define _LPC845_FEATURES_H_

/* SOC module features */

#if defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || defined(CPU_LPC845M301JHI48)
    /* @brief ACOMP availability on the SoC. */
    #define FSL_FEATURE_SOC_ACOMP_COUNT (1)
    /* @brief ADC availability on the SoC. */
    #define FSL_FEATURE_SOC_ADC_COUNT (1)
    /* @brief CAPT availability on the SoC. */
    #define FSL_FEATURE_SOC_CAPT_COUNT (1)
    /* @brief CRC availability on the SoC. */
    #define FSL_FEATURE_SOC_CRC_COUNT (1)
    /* @brief CTIMER availability on the SoC. */
    #define FSL_FEATURE_SOC_CTIMER_COUNT (1)
    /* @brief DAC availability on the SoC. */
    #define FSL_FEATURE_SOC_DAC_COUNT (2)
    /* @brief DMA availability on the SoC. */
    #define FSL_FEATURE_SOC_DMA_COUNT (1)
    /* @brief GPIO availability on the SoC. */
    #define FSL_FEATURE_SOC_GPIO_COUNT (1)
    /* @brief I2C availability on the SoC. */
    #define FSL_FEATURE_SOC_I2C_COUNT (4)
    /* @brief INPUTMUX availability on the SoC. */
    #define FSL_FEATURE_SOC_INPUTMUX_COUNT (1)
    /* @brief IOCON availability on the SoC. */
    #define FSL_FEATURE_SOC_IOCON_COUNT (1)
    /* @brief MRT availability on the SoC. */
    #define FSL_FEATURE_SOC_MRT_COUNT (1)
    /* @brief MTB availability on the SoC. */
    #define FSL_FEATURE_SOC_MTB_COUNT (1)
    /* @brief PINT availability on the SoC. */
    #define FSL_FEATURE_SOC_PINT_COUNT (1)
    /* @brief PMU availability on the SoC. */
    #define FSL_FEATURE_SOC_PMU_COUNT (1)
    /* @brief SCT availability on the SoC. */
    #define FSL_FEATURE_SOC_SCT_COUNT (1)
    /* @brief SPI availability on the SoC. */
    #define FSL_FEATURE_SOC_SPI_COUNT (2)
    /* @brief SWM availability on the SoC. */
    #define FSL_FEATURE_SOC_SWM_COUNT (1)
    /* @brief SYSCON availability on the SoC. */
    #define FSL_FEATURE_SOC_SYSCON_COUNT (1)
    /* @brief USART availability on the SoC. */
    #define FSL_FEATURE_SOC_USART_COUNT (5)
    /* @brief WWDT availability on the SoC. */
    #define FSL_FEATURE_SOC_WWDT_COUNT (1)
#elif defined(CPU_LPC845M301JHI33)
    /* @brief ACOMP availability on the SoC. */
    #define FSL_FEATURE_SOC_ACOMP_COUNT (1)
    /* @brief ADC availability on the SoC. */
    #define FSL_FEATURE_SOC_ADC_COUNT (1)
    /* @brief CRC availability on the SoC. */
    #define FSL_FEATURE_SOC_CRC_COUNT (1)
    /* @brief CTIMER availability on the SoC. */
    #define FSL_FEATURE_SOC_CTIMER_COUNT (1)
    /* @brief DAC availability on the SoC. */
    #define FSL_FEATURE_SOC_DAC_COUNT (1)
    /* @brief DMA availability on the SoC. */
    #define FSL_FEATURE_SOC_DMA_COUNT (1)
    /* @brief GPIO availability on the SoC. */
    #define FSL_FEATURE_SOC_GPIO_COUNT (1)
    /* @brief I2C availability on the SoC. */
    #define FSL_FEATURE_SOC_I2C_COUNT (4)
    /* @brief INPUTMUX availability on the SoC. */
    #define FSL_FEATURE_SOC_INPUTMUX_COUNT (1)
    /* @brief IOCON availability on the SoC. */
    #define FSL_FEATURE_SOC_IOCON_COUNT (1)
    /* @brief MRT availability on the SoC. */
    #define FSL_FEATURE_SOC_MRT_COUNT (1)
    /* @brief MTB availability on the SoC. */
    #define FSL_FEATURE_SOC_MTB_COUNT (1)
    /* @brief PINT availability on the SoC. */
    #define FSL_FEATURE_SOC_PINT_COUNT (1)
    /* @brief PMU availability on the SoC. */
    #define FSL_FEATURE_SOC_PMU_COUNT (1)
    /* @brief SCT availability on the SoC. */
    #define FSL_FEATURE_SOC_SCT_COUNT (1)
    /* @brief SPI availability on the SoC. */
    #define FSL_FEATURE_SOC_SPI_COUNT (2)
    /* @brief SWM availability on the SoC. */
    #define FSL_FEATURE_SOC_SWM_COUNT (1)
    /* @brief SYSCON availability on the SoC. */
    #define FSL_FEATURE_SOC_SYSCON_COUNT (1)
    /* @brief USART availability on the SoC. */
    #define FSL_FEATURE_SOC_USART_COUNT (5)
    /* @brief WWDT availability on the SoC. */
    #define FSL_FEATURE_SOC_WWDT_COUNT (1)
#endif

/* ACOMP module features */

/* @brief Has INTENA bitfile in CTRL reigster. */
#define FSL_FEATURE_ACOMP_HAS_CTRL_INTENA (1)

/* ADC module features */

/* @brief Do not has input select (register INSEL). */
#define FSL_FEATURE_ADC_HAS_NO_INSEL  (1)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_ASYNMODE (1)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_RESOL (0)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_BYPASSCAL (0)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_TSAMP (0)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_LPWRMODE (1)
/* @brief Has ASYNMODE bitfile in CTRL reigster. */
#define FSL_FEATURE_ADC_HAS_CTRL_CALMODE (1)
/* @brief Has startup register. */
#define FSL_FEATURE_ADC_HAS_STARTUP_REG (0)
/* @brief Has ADC Trim register */
#define FSL_FEATURE_ADC_HAS_TRIM_REG (1)
/* @brief Has Calibration register. */
#define FSL_FEATURE_ADC_HAS_CALIB_REG (0)

/* CAPT module features */

#if defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || defined(CPU_LPC845M301JHI48)
    /* @brief Has DMA bitfile in CTRL reigster. */
    #define FSL_FEATURE_CAPT_HAS_CTRL_DMA (1)
#endif /* defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || defined(CPU_LPC845M301JHI48) */

/* CLOCK module features */

/* @brief GPIOINT clock source. */
#define FSL_FEATURE_CLOCK_HAS_GPIOINT_CLOCK_SOURCE (1)

/* CTIMER module features */

/* @brief CTIMER has no capture channel. */
#define FSL_FEATURE_CTIMER_HAS_NO_INPUT_CAPTURE (0)
/* @brief CTIMER has no capture 2 interrupt. */
#define FSL_FEATURE_CTIMER_HAS_NO_IR_CR2INT (0)
/* @brief CTIMER capture 3 interrupt. */
#define FSL_FEATURE_CTIMER_HAS_IR_CR3INT (1)
/* @brief Has CTIMER CCR_CAP2 (register bits CCR[CAP2RE][CAP2FE][CAP2I]. */
#define FSL_FEATURE_CTIMER_HAS_NO_CCR_CAP2 (0)
/* @brief Has CTIMER CCR_CAP3 (register bits CCR[CAP3RE][CAP3FE][CAP3I]). */
#define FSL_FEATURE_CTIMER_HAS_CCR_CAP3 (1)
/* @brief Writing a zero asserts the CTIMER reset. */
#define FSL_FEATURE_CTIMER_WRITE_ZERO_ASSERT_RESET (1)
/* @brief CTIMER Has register MSR */
#define FSL_FEATURE_CTIMER_HAS_MSR (1)

/* DAC module features */

/* @brief Has DMA_ENA bitfile in CTRL reigster. */
#define FSL_FEATURE_DAC_HAS_CTRL_DMA_ENA (1)

/* DMA module features */

/* @brief Number of channels */
#define FSL_FEATURE_DMA_NUMBER_OF_CHANNELS (25)
/* @brief Align size of DMA descriptor */
#define FSL_FEATURE_DMA_DESCRIPTOR_ALIGN_SIZE (512)
/* @brief DMA head link descriptor table align size */
#define FSL_FEATURE_DMA_LINK_DESCRIPTOR_ALIGN_SIZE (16U)

/* FAIM module features */

/* @brief Size of the FAIM */
#define FSL_FEATURE_FAIM_SIZE (32)
/* @brief Page count of the FAIM */
#define FSL_FEATURE_FAIM_PAGE_COUNT (8)

/* INPUTMUX module features */

/* @brief Inputmux clock source. */
#define FSL_FEATURE_INPUTMUX_HAS_NO_INPUTMUX_CLOCK_SOURCE (1)

/* IOCON module features */

/* No feature definitions */

/* MRT module features */

/* @brief number of channels. */
#define FSL_FEATURE_MRT_NUMBER_OF_CHANNELS  (4)
/* @brief Has no MULTITASK bitfile in MODCFG reigster. */
#define FSL_FEATURE_MRT_HAS_NO_MODCFG_MULTITASK (1)
/* @brief Has no INUSE bitfile in STAT reigster. */
#define FSL_FEATURE_MRT_HAS_NO_CHANNEL_STAT_INUSE (1)
/* @brief Writing a zero asserts the MRT reset. */
#define FSL_FEATURE_MRT_WRITE_ZERO_ASSERT_RESET (1)

/* NVIC module features */

/* @brief Number of connected outputs. */
#define FSL_FEATURE_NVIC_HAS_SHARED_INTERTTUPT_NUMBER (1)

/* PINT module features */

/* @brief Number of connected outputs */
#define FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS (8)

/* SCT module features */

/* @brief Number of events */
#define FSL_FEATURE_SCT_NUMBER_OF_EVENTS (8)
/* @brief Number of states */
#define FSL_FEATURE_SCT_NUMBER_OF_STATES (8)
/* @brief Number of match capture */
#define FSL_FEATURE_SCT_NUMBER_OF_MATCH_CAPTURE (8)
/* @brief Number of outputs */
#define FSL_FEATURE_SCT_NUMBER_OF_OUTPUTS (7)
/* @brief Writing a zero asserts the SCT reset. */
#define FSL_FEATURE_SCT_WRITE_ZERO_ASSERT_RESET (1)

/* SPI module features */

/* @brief Has SPOL0 bitfile in CFG reigster. */
#define FSL_FEATURE_SPI_HAS_SSEL0 (1)
/* @brief Has SPOL1 bitfile in CFG reigster. */
#define FSL_FEATURE_SPI_HAS_SSEL1 (1)
/* @brief Has SPOL2 bitfile in CFG reigster. */
#define FSL_FEATURE_SPI_HAS_SSEL2 (1)
/* @brief Has SPOL3 bitfile in CFG reigster. */
#define FSL_FEATURE_SPI_HAS_SSEL3 (1)

/* SWM module features */

/* @brief Has SWM PINENABLE0 ACMP I3. */
#define FSL_FEATURE_SWM_HAS_PINENABLE0_ACMP_I3 (1)
/* @brief Has SWM PINENABLE0 ACMP I4. */
#define FSL_FEATURE_SWM_HAS_PINENABLE0_ACMP_I4 (1)
/* @brief Has SWM PINENABLE0 ACMP I5. */
#define FSL_FEATURE_SWM_HAS_PINENABLE0_ACMP_I5 (1)
/* @brief Has SWM PINENABLE1. */
#define FSL_FEATURE_SWM_HAS_PINENABLE1_REGISTER (1)

/* SYSCON module features */

/* @brief Pointer to ROM IAP entry functions */
#define FSL_FEATURE_SYSCON_IAP_ENTRY_LOCATION (0x0F001FF1)
/* @brief IAP Reinvoke ISP command parameter is pointer */
#define FSL_FEATURE_SYSCON_IAP_REINVOKE_ISP_PARAM_POINTER (0)
/* @brief Flash page size in bytes */
#define FSL_FEATURE_SYSCON_FLASH_PAGE_SIZE_BYTES (64)
/* @brief Flash sector size in bytes */
#define FSL_FEATURE_SYSCON_FLASH_SECTOR_SIZE_BYTES (1024)
/* @brief Flash size in bytes */
#define FSL_FEATURE_SYSCON_FLASH_SIZE_BYTES (65536)
/* @brief IAP has Flash read & write function */
#define FSL_FEATURE_IAP_HAS_FLASH_FUNCTION (1)
/* @brief IAP has FAIM read & write function */
#define FSL_FEATURE_IAP_HAS_FAIM_FUNCTION (1)
/* @brief IAP has read Flash signature function */
#define FSL_FEATURE_IAP_HAS_FLASH_SIGNATURE_READ (0)
/* @brief IAP has read extended Flash signature function */
#define FSL_FEATURE_IAP_HAS_FLASH_EXTENDED_SIGNATURE_READ (1)
/* @brief Starter register discontinuous. */
#define FSL_FEATURE_SYSCON_STARTER_DISCONTINUOUS (1)
/* @brief Has PINTSEL register. */
#define FSL_FEATURE_SYSCON_HAS_PINT_SEL_REGISTER (1)

/* USART module features */

/* @brief Has OSR (register OSR). */
#define FSL_FEATURE_USART_HAS_OSR_REGISTER (1)
/* @brief Has TXIDLEEN bitfile in INTENSET reigster. */
#define FSL_FEATURE_USART_HAS_INTENSET_TXIDLEEN (1)
/* @brief Has ABERREN bitfile in INTENSET reigster. */
#define FSL_FEATURE_USART_HAS_ABERR_CHECK (1)

/* WKT module features */

/* @brief Has SEL_EXTCLK bitfile in CTRL reigster. */
#define FSL_FEATURE_WKT_HAS_CTRL_SEL_EXTCLK (1)

/* WWDT module features */

/* @brief Has no RESET register. */
#define FSL_FEATURE_WWDT_HAS_NO_RESET (1)

#endif /* _LPC845_FEATURES_H_ */

