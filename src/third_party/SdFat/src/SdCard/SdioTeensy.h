/**
 * \file
 * \brief Definitions for Teensy HDHC.
 */

#ifndef SdioTeensy_h
#define SdioTeensy_h

// From Paul's SD.h driver.

#if defined(__IMXRT1062__)
#define MAKE_REG_MASK(m, s) (((uint32_t)(((uint32_t)(m) << s))))
#define MAKE_REG_GET(x, m, s) (((uint32_t)(((uint32_t)(x) >> s) & m)))
#define MAKE_REG_SET(x, m, s) (((uint32_t)(((uint32_t)(x)&m) << s)))

#define SDHC_BLKATTR_BLKSIZE_MASK \
  MAKE_REG_MASK(                  \
      0x1FFF, 0)  // uint32_t)(((n) & 0x1FFF)<<0) // Transfer Block Size Mask
#define SDHC_BLKATTR_BLKSIZE(n) \
  MAKE_REG_SET(n, 0x1FFF,       \
               0)  // uint32_t)(((n) & 0x1FFF)<<0) // Transfer Block Size
#define SDHC_BLKATTR_BLKCNT_MASK \
  MAKE_REG_MASK(0x1FFF, 16)  //((uint32_t)0x1FFF<<16)
#define SDHC_BLKATTR_BLKCNT(n) \
  MAKE_REG_SET(n, 0x1FFF, 16)  //(uint32_t)(((n) & 0x1FFF)<<16) // Blocks Count
                               // For Current Transfer

#define SDHC_XFERTYP_CMDINX(n) \
  MAKE_REG_SET(n, 0x3F, 24)  //(uint32_t)(((n) & 0x3F)<<24)// Command Index
#define SDHC_XFERTYP_CMDTYP(n) \
  MAKE_REG_SET(n, 0x3, 22)  //(uint32_t)(((n) & 0x3)<<22)  // Command Type
#define SDHC_XFERTYP_DPSEL \
  MAKE_REG_MASK(0x1, 21)  //((uint32_t)0x00200000)    // Data Present Select
#define SDHC_XFERTYP_CICEN \
  MAKE_REG_MASK(0x1,       \
                20)  //((uint32_t)0x00100000)    // Command Index Check Enable
#define SDHC_XFERTYP_CCCEN \
  MAKE_REG_MASK(0x1,       \
                19)  //((uint32_t)0x00080000)    // Command CRC Check Enable
#define SDHC_XFERTYP_RSPTYP(n) \
  MAKE_REG_SET(n, 0x3,         \
               16)  //(uint32_t)(((n) & 0x3)<<16)  // Response Type Select
#define SDHC_XFERTYP_MSBSEL \
  MAKE_REG_MASK(0x1, 5)  //((uint32_t)0x00000020)   // Multi/Single Block Select
#define SDHC_XFERTYP_DTDSEL \
  MAKE_REG_MASK(            \
      0x1, 4)  //((uint32_t)0x00000010)   // Data Transfer Direction Select
#define SDHC_XFERTYP_AC12EN \
  MAKE_REG_MASK(0x1, 2)  //((uint32_t)0x00000004)   // Auto CMD12 Enable
#define SDHC_XFERTYP_BCEN \
  MAKE_REG_MASK(0x1, 1)  //((uint32_t)0x00000002)   // Block Count Enable
#define SDHC_XFERTYP_DMAEN \
  MAKE_REG_MASK(0x3, 0)  //((uint32_t)0x00000001)   // DMA Enable

#define SDHC_PRSSTAT_DLSL_MASK \
  MAKE_REG_MASK(0xFF, 24)  //((uint32_t)0xFF000000)    // DAT Line Signal Level
#define SDHC_PRSSTAT_CLSL \
  MAKE_REG_MASK(0x1, 23)  //((uint32_t)0x00800000)    // CMD Line Signal Level
#define SDHC_PRSSTAT_WPSPL MAKE_REG_MASK(0x1, 19)  //
#define SDHC_PRSSTAT_CDPL MAKE_REG_MASK(0x1, 18)   //
#define SDHC_PRSSTAT_CINS \
  MAKE_REG_MASK(0x1, 16)  //((uint32_t)0x00010000)    // Card Inserted
#define SDHC_PRSSTAT_TSCD MAKE_REG_MASK(0x1, 15)
#define SDHC_PRSSTAT_RTR MAKE_REG_MASK(0x1, 12)
#define SDHC_PRSSTAT_BREN \
  MAKE_REG_MASK(0x1, 11)  //((uint32_t)0x00000800)    // Buffer Read Enable
#define SDHC_PRSSTAT_BWEN \
  MAKE_REG_MASK(0x1, 10)  //((uint32_t)0x00000400)    // Buffer Write Enable
#define SDHC_PRSSTAT_RTA \
  MAKE_REG_MASK(0x1, 9)  //((uint32_t)0x00000200)   // Read Transfer Active
#define SDHC_PRSSTAT_WTA \
  MAKE_REG_MASK(0x1, 8)  //((uint32_t)0x00000100)   // Write Transfer Active
#define SDHC_PRSSTAT_SDOFF \
  MAKE_REG_MASK(           \
      0x1, 7)  //((uint32_t)0x00000080)   // SD Clock Gated Off Internally
#define SDHC_PRSSTAT_PEROFF \
  MAKE_REG_MASK(            \
      0x1, 6)  //((uint32_t)0x00000040)   // SDHC clock Gated Off Internally
#define SDHC_PRSSTAT_HCKOFF \
  MAKE_REG_MASK(            \
      0x1, 5)  //((uint32_t)0x00000020)   // System Clock Gated Off Internally
#define SDHC_PRSSTAT_IPGOFF \
  MAKE_REG_MASK(            \
      0x1, 4)  //((uint32_t)0x00000010)   // Bus Clock Gated Off Internally
#define SDHC_PRSSTAT_SDSTB \
  MAKE_REG_MASK(0x1, 3)  //((uint32_t)0x00000008)   // SD Clock Stable
#define SDHC_PRSSTAT_DLA \
  MAKE_REG_MASK(0x1, 2)  //((uint32_t)0x00000004)   // Data Line Active
#define SDHC_PRSSTAT_CDIHB \
  MAKE_REG_MASK(0x1, 1)  //((uint32_t)0x00000002)   // Command Inhibit (DAT)
#define SDHC_PRSSTAT_CIHB \
  MAKE_REG_MASK(0x1, 0)  //((uint32_t)0x00000001)   // Command Inhibit (CMD)

#define SDHC_PROTCT_NONEXACT_BLKRD MAKE_REG_MASK(0x1, 30)    //
#define SDHC_PROTCT_BURST_LENEN(n) MAKE_REG_SET(n, 0x7, 12)  //
#define SDHC_PROCTL_WECRM \
  MAKE_REG_MASK(0x1, 26)  //((uint32_t)0x04000000)    // Wakeup Event Enable On
                          // SD Card Removal
#define SDHC_PROCTL_WECINS \
  MAKE_REG_MASK(0x1, 25)  //((uint32_t)0x02000000)    // Wakeup Event Enable On
                          // SD Card Insertion
#define SDHC_PROCTL_WECINT \
  MAKE_REG_MASK(0x1, 24)  //((uint32_t)0x01000000)    // Wakeup Event Enable On
                          // Card Interrupt
#define SDHC_PROCTL_RD_DONE_NOBLK MAKE_REG_MASK(0x1, 20)  //
#define SDHC_PROCTL_IABG \
  MAKE_REG_MASK(0x1, 19)  //((uint32_t)0x00080000)    // Interrupt At Block Gap
#define SDHC_PROCTL_RWCTL \
  MAKE_REG_MASK(0x1, 18)  //((uint32_t)0x00040000)    // Read Wait Control
#define SDHC_PROCTL_CREQ \
  MAKE_REG_MASK(0x1, 17)  //((uint32_t)0x00020000)    // Continue Request
#define SDHC_PROCTL_SABGREQ \
  MAKE_REG_MASK(0x1,        \
                16)  //((uint32_t)0x00010000)    // Stop At Block Gap Request
#define SDHC_PROCTL_DMAS(n) \
  MAKE_REG_SET(n, 0x3, 8)  //(uint32_t)(((n) & 0x3)<<8)  // DMA Select
#define SDHC_PROCTL_CDSS \
  MAKE_REG_MASK(0x1,     \
                7)  //((uint32_t)0x00000080)   // Card Detect Signal Selection
#define SDHC_PROCTL_CDTL \
  MAKE_REG_MASK(0x1, 6)  //((uint32_t)0x00000040)   // Card Detect Test Level
#define SDHC_PROCTL_EMODE(n) \
  MAKE_REG_SET(n, 0x3, 4)  //(uint32_t)(((n) & 0x3)<<4)  // Endian Mode
#define SDHC_PROCTL_EMODE_MASK \
  MAKE_REG_MASK(0x3, 4)  //(uint32_t)((0x3)<<4) // Endian Mode
#define SDHC_PROCTL_D3CD \
  MAKE_REG_MASK(0x1,     \
                3)  //((uint32_t)0x00000008)   // DAT3 As Card Detection Pin
#define SDHC_PROCTL_DTW(n) \
  MAKE_REG_SET(n, 0x3, 1)  //(uint32_t)(((n) & 0x3)<<1)  // Data Transfer Width,
                           // 0=1bit, 1=4bit, 2=8bit
#define SDHC_PROCTL_DTW_MASK MAKE_REG_MASK(0x3, 1)  //((uint32_t)0x00000006)
#define SDHC_PROCTL_LCTL \
  MAKE_REG_MASK(0x1, 0)  //((uint32_t)0x00000001)   // LED Control

#define SDHC_SYSCTL_RSTT MAKE_REG_MASK(0x1, 28)  //
#define SDHC_SYSCTL_INITA \
  MAKE_REG_MASK(0x1, 27)  //((uint32_t)0x08000000)    // Initialization Active
#define SDHC_SYSCTL_RSTD \
  MAKE_REG_MASK(         \
      0x1, 26)  //((uint32_t)0x04000000)    // Software Reset For DAT Line
#define SDHC_SYSCTL_RSTC \
  MAKE_REG_MASK(         \
      0x1, 25)  //((uint32_t)0x02000000)    // Software Reset For CMD Line
#define SDHC_SYSCTL_RSTA \
  MAKE_REG_MASK(0x1, 24)  //((uint32_t)0x01000000)    // Software Reset For ALL
#define SDHC_SYSCTL_DTOCV(n) \
  MAKE_REG_SET(              \
      n, 0xF,                \
      16)  //(uint32_t)(((n) & 0xF)<<16)  // Data Timeout Counter Value
#define SDHC_SYSCTL_DTOCV_MASK MAKE_REG_MASK(0xF, 16)  //((uint32_t)0x000F0000)
#define SDHC_SYSCTL_SDCLKFS(n) \
  MAKE_REG_SET(n, 0xFF,        \
               8)  //(uint32_t)(((n) & 0xFF)<<8)  // SDCLK Frequency Select
#define SDHC_SYSCTL_SDCLKFS_MASK \
  MAKE_REG_MASK(0xFF, 8)  //((uint32_t)0x0000FF00)
#define SDHC_SYSCTL_DVS(n) \
  MAKE_REG_SET(n, 0xF, 4)  //(uint32_t)(((n) & 0xF)<<4)  // Divisor
#define SDHC_SYSCTL_DVS_MASK MAKE_REG_MASK(0xF, 4)  //((uint32_t)0x000000F0)

#define SDHC_SYSCTL_SDCLKEN ((uint32_t)0x00000008)  // SD Clock Enable
#define SDHC_SYSCTL_PEREN ((uint32_t)0x00000004)    // Peripheral Clock Enable
#define SDHC_SYSCTL_HCKEN ((uint32_t)0x00000002)    // System Clock Enable
#define SDHC_SYSCTL_IPGEN ((uint32_t)0x00000001)    // IPG Clock Enable

#define SDHC_IRQSTAT_DMAE \
  MAKE_REG_MASK(0x1, 28)  //((uint32_t)0x10000000)    // DMA Error
#define SDHC_IRQSTAT_TNE MAKE_REG_MASK(0x1, 26)  //
#define SDHC_IRQSTAT_AC12E \
  MAKE_REG_MASK(0x1, 24)  //((uint32_t)0x01000000)    // Auto CMD12 Error
#define SDHC_IRQSTAT_DEBE \
  MAKE_REG_MASK(0x1, 22)  //((uint32_t)0x00400000)    // Data End Bit Error
#define SDHC_IRQSTAT_DCE \
  MAKE_REG_MASK(0x1, 21)  //((uint32_t)0x00200000)    // Data CRC Error
#define SDHC_IRQSTAT_DTOE \
  MAKE_REG_MASK(0x1, 20)  //((uint32_t)0x00100000)    // Data Timeout Error
#define SDHC_IRQSTAT_CIE \
  MAKE_REG_MASK(0x1, 19)  //((uint32_t)0x00080000)    // Command Index Error
#define SDHC_IRQSTAT_CEBE \
  MAKE_REG_MASK(0x1, 18)  //((uint32_t)0x00040000)    // Command End Bit Error
#define SDHC_IRQSTAT_CCE \
  MAKE_REG_MASK(0x1, 17)  //((uint32_t)0x00020000)    // Command CRC Error
#define SDHC_IRQSTAT_CTOE \
  MAKE_REG_MASK(0x1, 16)  //((uint32_t)0x00010000)    // Command Timeout Error
#define SDHC_IRQSTAT_TP MAKE_REG_MASK(0x1, 14)   //
#define SDHC_IRQSTAT_RTE MAKE_REG_MASK(0x1, 12)  //
#define SDHC_IRQSTAT_CINT \
  MAKE_REG_MASK(0x1, 8)  //((uint32_t)0x00000100)   // Card Interrupt
#define SDHC_IRQSTAT_CRM \
  MAKE_REG_MASK(0x1, 7)  //((uint32_t)0x00000080)   // Card Removal
#define SDHC_IRQSTAT_CINS \
  MAKE_REG_MASK(0x1, 6)  //((uint32_t)0x00000040)   // Card Insertion
#define SDHC_IRQSTAT_BRR \
  MAKE_REG_MASK(0x1, 5)  //((uint32_t)0x00000020)   // Buffer Read Ready
#define SDHC_IRQSTAT_BWR \
  MAKE_REG_MASK(0x1, 4)  //((uint32_t)0x00000010)   // Buffer Write Ready
#define SDHC_IRQSTAT_DINT \
  MAKE_REG_MASK(0x1, 3)  //((uint32_t)0x00000008)   // DMA Interrupt
#define SDHC_IRQSTAT_BGE \
  MAKE_REG_MASK(0x1, 2)  //((uint32_t)0x00000004)   // Block Gap Event
#define SDHC_IRQSTAT_TC \
  MAKE_REG_MASK(0x1, 1)  //((uint32_t)0x00000002)   // Transfer Complete
#define SDHC_IRQSTAT_CC \
  MAKE_REG_MASK(0x1, 0)  //((uint32_t)0x00000001)   // Command Complete

#define SDHC_IRQSTATEN_DMAESEN \
  MAKE_REG_MASK(0x1, 28)  //((uint32_t)0x10000000)    // DMA Error Status Enable
#define SDHC_IRQSTATEN_TNESEN MAKE_REG_MASK(0x1, 26)  //
#define SDHC_IRQSTATEN_AC12ESEN \
  MAKE_REG_MASK(                \
      0x1, 24)  //((uint32_t)0x01000000)    // Auto CMD12 Error Status Enable
#define SDHC_IRQSTATEN_DEBESEN \
  MAKE_REG_MASK(               \
      0x1,                     \
      22)  //((uint32_t)0x00400000)    // Data End Bit Error Status Enable
#define SDHC_IRQSTATEN_DCESEN \
  MAKE_REG_MASK(              \
      0x1, 21)  //((uint32_t)0x00200000)    // Data CRC Error Status Enable
#define SDHC_IRQSTATEN_DTOESEN \
  MAKE_REG_MASK(               \
      0x1,                     \
      20)  //((uint32_t)0x00100000)    // Data Timeout Error Status Enable
#define SDHC_IRQSTATEN_CIESEN \
  MAKE_REG_MASK(              \
      0x1,                    \
      19)  //((uint32_t)0x00080000)    // Command Index Error Status Enable
#define SDHC_IRQSTATEN_CEBESEN \
  MAKE_REG_MASK(               \
      0x1,                     \
      18)  //((uint32_t)0x00040000)    // Command End Bit Error Status Enable
#define SDHC_IRQSTATEN_CCESEN \
  MAKE_REG_MASK(              \
      0x1, 17)  //((uint32_t)0x00020000)    // Command CRC Error Status Enable
#define SDHC_IRQSTATEN_CTOESEN \
  MAKE_REG_MASK(               \
      0x1,                     \
      16)  //((uint32_t)0x00010000)    // Command Timeout Error Status Enable
#define SDHC_IRQSTATEN_TPSEN MAKE_REG_MASK(0x1, 14)   //
#define SDHC_IRQSTATEN_RTESEN MAKE_REG_MASK(0x1, 12)  //
#define SDHC_IRQSTATEN_CINTSEN \
  MAKE_REG_MASK(0x1,           \
                8)  //((uint32_t)0x00000100)   // Card Interrupt Status Enable
#define SDHC_IRQSTATEN_CRMSEN \
  MAKE_REG_MASK(0x1,          \
                7)  //((uint32_t)0x00000080)   // Card Removal Status Enable
#define SDHC_IRQSTATEN_CINSEN \
  MAKE_REG_MASK(0x1,          \
                6)  //((uint32_t)0x00000040)   // Card Insertion Status Enable
#define SDHC_IRQSTATEN_BRRSEN \
  MAKE_REG_MASK(              \
      0x1, 5)  //((uint32_t)0x00000020)   // Buffer Read Ready Status Enable
#define SDHC_IRQSTATEN_BWRSEN \
  MAKE_REG_MASK(              \
      0x1, 4)  //((uint32_t)0x00000010)   // Buffer Write Ready Status Enable
#define SDHC_IRQSTATEN_DINTSEN \
  MAKE_REG_MASK(0x1,           \
                3)  //((uint32_t)0x00000008)   // DMA Interrupt Status Enable
#define SDHC_IRQSTATEN_BGESEN \
  MAKE_REG_MASK(              \
      0x1, 2)  //((uint32_t)0x00000004)   // Block Gap Event Status Enable
#define SDHC_IRQSTATEN_TCSEN \
  MAKE_REG_MASK(             \
      0x1, 1)  //((uint32_t)0x00000002)   // Transfer Complete Status Enable
#define SDHC_IRQSTATEN_CCSEN \
  MAKE_REG_MASK(             \
      0x1, 0)  //((uint32_t)0x00000001)   // Command Complete Status Enable

#define SDHC_IRQSIGEN_DMAEIEN \
  MAKE_REG_MASK(0x1,          \
                28)  //((uint32_t)0x10000000)    // DMA Error Interrupt Enable
#define SDHC_IRQSIGEN_TNEIEN MAKE_REG_MASK(0x1, 26)  //
#define SDHC_IRQSIGEN_AC12EIEN \
  MAKE_REG_MASK(               \
      0x1,                     \
      24)  //((uint32_t)0x01000000)    // Auto CMD12 Error Interrupt Enable
#define SDHC_IRQSIGEN_DEBEIEN \
  MAKE_REG_MASK(              \
      0x1,                    \
      22)  //((uint32_t)0x00400000)    // Data End Bit Error Interrupt Enable
#define SDHC_IRQSIGEN_DCEIEN \
  MAKE_REG_MASK(             \
      0x1, 21)  //((uint32_t)0x00200000)    // Data CRC Error Interrupt Enable
#define SDHC_IRQSIGEN_DTOEIEN \
  MAKE_REG_MASK(              \
      0x1,                    \
      20)  //((uint32_t)0x00100000)    // Data Timeout Error Interrupt Enable
#define SDHC_IRQSIGEN_CIEIEN \
  MAKE_REG_MASK(             \
      0x1,                   \
      19)  //((uint32_t)0x00080000)    // Command Index Error Interrupt Enable
#define SDHC_IRQSIGEN_CEBEIEN \
  MAKE_REG_MASK(0x1, 18)  //((uint32_t)0x00040000)    // Command End Bit Error
                          // Interrupt Enable
#define SDHC_IRQSIGEN_CCEIEN \
  MAKE_REG_MASK(             \
      0x1,                   \
      17)  //((uint32_t)0x00020000)    // Command CRC Error Interrupt Enable
#define SDHC_IRQSIGEN_CTOEIEN \
  MAKE_REG_MASK(0x1, 16)  //((uint32_t)0x00010000)    // Command Timeout Error
                          // Interrupt Enable
#define SDHC_IRQSIGEN_TPIEN MAKE_REG_MASK(0x1, 14)   //
#define SDHC_IRQSIGEN_RTEIEN MAKE_REG_MASK(0x1, 12)  //
#define SDHC_IRQSIGEN_CINTIEN \
  MAKE_REG_MASK(              \
      0x1, 8)  //((uint32_t)0x00000100)    // Card Interrupt Interrupt Enable
#define SDHC_IRQSIGEN_CRMIEN \
  MAKE_REG_MASK(             \
      0x1, 7)  //((uint32_t)0x00000080)    // Card Removal Interrupt Enable
#define SDHC_IRQSIGEN_CINSIEN \
  MAKE_REG_MASK(              \
      0x1, 6)  //((uint32_t)0x00000040)    // Card Insertion Interrupt Enable
#define SDHC_IRQSIGEN_BRRIEN \
  MAKE_REG_MASK(             \
      0x1,                   \
      5)  //((uint32_t)0x00000020)    // Buffer Read Ready Interrupt Enable
#define SDHC_IRQSIGEN_BWRIEN \
  MAKE_REG_MASK(             \
      0x1,                   \
      4)  //((uint32_t)0x00000010)    // Buffer Write Ready Interrupt Enable
#define SDHC_IRQSIGEN_DINTIEN \
  MAKE_REG_MASK(              \
      0x1, 3)  //((uint32_t)0x00000008)    // DMA Interrupt Interrupt Enable
#define SDHC_IRQSIGEN_BGEIEN \
  MAKE_REG_MASK(             \
      0x1, 2)  //((uint32_t)0x00000004)    // Block Gap Event Interrupt Enable
#define SDHC_IRQSIGEN_TCIEN \
  MAKE_REG_MASK(            \
      0x1,                  \
      1)  //((uint32_t)0x00000002)    // Transfer Complete Interrupt Enable
#define SDHC_IRQSIGEN_CCIEN \
  MAKE_REG_MASK(            \
      0x1,                  \
      0)  //((uint32_t)0x00000001)    // Command Complete Interrupt Enable

#define SDHC_AC12ERR_SMPLCLK_SEL MAKE_REG_MASK(0x1, 23)  //
#define SDHC_AC12ERR_EXEC_TUNING MAKE_REG_MASK(0x1, 22)  //
#define SDHC_AC12ERR_CNIBAC12E \
  MAKE_REG_MASK(0x1, 7)  //((uint32_t)0x00000080)    // Command Not Issued By
                         // Auto CMD12 Error
#define SDHC_AC12ERR_AC12IE \
  MAKE_REG_MASK(0x1, 4)  //((uint32_t)0x00000010)    // Auto CMD12 Index Error
#define SDHC_AC12ERR_AC12CE \
  MAKE_REG_MASK(0x1, 3)  //((uint32_t)0x00000008)    // Auto CMD12 CRC Error
#define SDHC_AC12ERR_AC12EBE \
  MAKE_REG_MASK(0x1, 2)  //((uint32_t)0x00000004)    // Auto CMD12 End Bit Error
#define SDHC_AC12ERR_AC12TOE \
  MAKE_REG_MASK(0x1, 1)  //((uint32_t)0x00000002)    // Auto CMD12 Timeout Error
#define SDHC_AC12ERR_AC12NE \
  MAKE_REG_MASK(0x1, 0)  //((uint32_t)0x00000001)    // Auto CMD12 Not Executed

#define SDHC_HTCAPBLT_VS18 MAKE_REG_MASK(0x1, 26)                            //
#define SDHC_HTCAPBLT_VS30 MAKE_REG_MASK(0x1, 25)                            //
#define SDHC_HTCAPBLT_VS33 MAKE_REG_MASK(0x1, 24)                            //
#define SDHC_HTCAPBLT_SRS MAKE_REG_MASK(0x1, 23)                             //
#define SDHC_HTCAPBLT_DMAS MAKE_REG_MASK(0x1, 22)                            //
#define SDHC_HTCAPBLT_HSS MAKE_REG_MASK(0x1, 21)                             //
#define SDHC_HTCAPBLT_ADMAS MAKE_REG_MASK(0x1, 20)                           //
#define SDHC_HTCAPBLT_MBL_VAL MAKE_REG_GET((USDHC1_HOST_CTRL_CAP), 0x7, 16)  //
#define SDHC_HTCAPBLT_RETUN_MODE \
  MAKE_REG_GET((USDHC1_HOST_CTRL_CAP), 0x3, 14)              //
#define SDHC_HTCAPBLT_TUNE_SDR50 MAKE_REG_MASK(0x1, 13)      //
#define SDHC_HTCAPBLT_TIME_RETUN(n) MAKE_REG_SET(n, 0xF, 8)  //

#define SDHC_WML_WR_BRSTLEN_MASK MAKE_REG_MASK(0x1F, 24)  //
#define SDHC_WML_RD_BRSTLEN_MASK MAKE_REG_MASK(0x1F, 8)   //
#define SDHC_WML_WR_WML_MASK MAKE_REG_MASK(0xFF, 16)      //
#define SDHC_WML_RD_WML_MASK MAKE_REG_MASK(0xFF, 0)       //
#define SDHC_WML_WR_BRSTLEN(n) \
  MAKE_REG_SET(n, 0x1F, 24)  //(uint32_t)(((n) & 0x7F)<<16)  // Write Burst Len
#define SDHC_WML_RD_BRSTLEN(n) \
  MAKE_REG_SET(n, 0x1F, 8)  //(uint32_t)(((n) & 0x7F)<<0)   // Read Burst Len
#define SDHC_WML_WR_WML(n) \
  MAKE_REG_SET(n, 0xFF,    \
               16)  //(uint32_t)(((n) & 0x7F)<<16)  // Write Watermark Level
#define SDHC_WML_RD_WML(n) \
  MAKE_REG_SET(n, 0xFF,    \
               0)  //(uint32_t)(((n) & 0x7F)<<0)   // Read Watermark Level
#define SDHC_WML_WRWML(n) \
  MAKE_REG_SET(n, 0xFF,   \
               16)  //(uint32_t)(((n) & 0x7F)<<16)  // Write Watermark Level
#define SDHC_WML_RDWML(n) \
  MAKE_REG_SET(n, 0xFF,   \
               0)  //(uint32_t)(((n) & 0x7F)<<0)   // Read Watermark Level

// Teensy 4.0 only
#define SDHC_MIX_CTRL_DMAEN MAKE_REG_MASK(0x1, 0)       //
#define SDHC_MIX_CTRL_BCEN MAKE_REG_MASK(0x1, 1)        //
#define SDHC_MIX_CTRL_AC12EN MAKE_REG_MASK(0x1, 2)      //
#define SDHC_MIX_CTRL_DDR_EN MAKE_REG_MASK(0x1, 3)      //
#define SDHC_MIX_CTRL_DTDSEL MAKE_REG_MASK(0x1, 4)      //
#define SDHC_MIX_CTRL_MSBSEL MAKE_REG_MASK(0x1, 5)      //
#define SDHC_MIX_CTRL_NIBBLE_POS MAKE_REG_MASK(0x1, 6)  //
#define SDHC_MIX_CTRL_AC23EN MAKE_REG_MASK(0x1, 7)      //

#define SDHC_FEVT_CINT \
  MAKE_REG_MASK(0x1,   \
                31)  //((uint32_t)0x80000000)    // Force Event Card Interrupt
#define SDHC_FEVT_DMAE \
  MAKE_REG_MASK(0x1, 28)  //((uint32_t)0x10000000)    // Force Event DMA Error
#define SDHC_FEVT_AC12E \
  MAKE_REG_MASK(        \
      0x1, 24)  //((uint32_t)0x01000000)    // Force Event Auto CMD12 Error
#define SDHC_FEVT_DEBE \
  MAKE_REG_MASK(       \
      0x1, 22)  //((uint32_t)0x00400000)    // Force Event Data End Bit Error
#define SDHC_FEVT_DCE \
  MAKE_REG_MASK(0x1,  \
                21)  //((uint32_t)0x00200000)    // Force Event Data CRC Error
#define SDHC_FEVT_DTOE \
  MAKE_REG_MASK(       \
      0x1, 20)  //((uint32_t)0x00100000)    // Force Event Data Timeout Error
#define SDHC_FEVT_CIE \
  MAKE_REG_MASK(      \
      0x1, 19)  //((uint32_t)0x00080000)    // Force Event Command Index Error
#define SDHC_FEVT_CEBE \
  MAKE_REG_MASK(       \
      0x1,             \
      18)  //((uint32_t)0x00040000)    // Force Event Command End Bit Error
#define SDHC_FEVT_CCE \
  MAKE_REG_MASK(      \
      0x1, 17)  //((uint32_t)0x00020000)    // Force Event Command CRC Error
#define SDHC_FEVT_CTOE \
  MAKE_REG_MASK(       \
      0x1,             \
      16)  //((uint32_t)0x00010000)    // Force Event Command Timeout Error
#define SDHC_FEVT_CNIBAC12E \
  MAKE_REG_MASK(0x1, 7)  //((uint32_t)0x00000080)    // Force Event Command Not
                         // Executed By Auto Command 12 Error
#define SDHC_FEVT_AC12IE \
  MAKE_REG_MASK(0x1, 4)  //((uint32_t)0x00000010)    // Force Event Auto Command
                         // 12 Index Error
#define SDHC_FEVT_AC12EBE \
  MAKE_REG_MASK(0x1, 3)  //((uint32_t)0x00000008)    // Force Event Auto Command
                         // 12 End Bit Error
#define SDHC_FEVT_AC12CE \
  MAKE_REG_MASK(         \
      0x1,               \
      2)  //((uint32_t)0x00000004)    // Force Event Auto Command 12 CRC Error
#define SDHC_FEVT_AC12TOE \
  MAKE_REG_MASK(0x1, 1)  //((uint32_t)0x00000002)    // Force Event Auto Command
                         // 12 Time Out Error
#define SDHC_FEVT_AC12NE \
  MAKE_REG_MASK(0x1, 0)  //((uint32_t)0x00000001)    // Force Event Auto Command
                         // 12 Not Executed

#define SDHC_ADMAES_ADMADCE MAKE_REG_MASK(0x1, 3)      //((uint32_t)0x00000008)
#define SDHC_ADMAES_ADMALME MAKE_REG_MASK(0x1, 2)      //((uint32_t)0x00000004)
#define SDHC_ADMAES_ADMAES_MASK MAKE_REG_MASK(0x3, 0)  //((uint32_t)0x00000003)

#define SDHC_MMCBOOT_BOOTBLKCNT(n) \
  MAKE_REG_MASK(0xFF, 16)  //(uint32_t)(((n) & 0xFFF)<<16) // stop at block gap
                           // value of automatic mode
#define SDHC_MMCBOOT_AUTOSABGEN \
  MAKE_REG_MASK(0x1, 7)  //((uint32_t)0x00000080)    // enable auto stop at
                         // block gap function
#define SDHC_MMCBOOT_BOOTEN \
  MAKE_REG_MASK(0x1, 6)  //((uint32_t)0x00000040)    // Boot Mode Enable
#define SDHC_MMCBOOT_BOOTMODE \
  MAKE_REG_MASK(0x1, 5)  //((uint32_t)0x00000020)    // Boot Mode Select
#define SDHC_MMCBOOT_BOOTACK \
  MAKE_REG_MASK(0x1, 4)  //((uint32_t)0x00000010)    // Boot Ack Mode Select
#define SDHC_MMCBOOT_DTOCVACK(n) \
  MAKE_REG_MASK(                 \
      0xF,                       \
      0)  //(uint32_t)(((n) & 0xF)<<0)  // Boot ACK Time Out Counter Value
// #define SDHC_HOSTVER    (*(volatile uint32_t*)0x400B10FC) // Host Controller
// Version

#define CCM_ANALOG_PFD_528_PFD0_FRAC_MASK 0x3f
#define CCM_ANALOG_PFD_528_PFD0_FRAC(n) ((n)&CCM_ANALOG_PFD_528_PFD0_FRAC_MASK)
#define CCM_ANALOG_PFD_528_PFD1_FRAC_MASK (0x3f << 8)
#define CCM_ANALOG_PFD_528_PFD1_FRAC(n) \
  (((n) << 8) & CCM_ANALOG_PFD_528_PFD1_FRAC_MASK)
#define CCM_ANALOG_PFD_528_PFD2_FRAC_MASK (0x3f << 16)
#define CCM_ANALOG_PFD_528_PFD2_FRAC(n) \
  (((n) << 16) & CCM_ANALOG_PFD_528_PFD2_FRAC_MASK)
#define CCM_ANALOG_PFD_528_PFD3_FRAC_MASK ((0x3f<<24)
#define CCM_ANALOG_PFD_528_PFD3_FRAC(n) \
  (((n) << 24) & CCM_ANALOG_PFD_528_PFD3_FRAC_MASK)

#define SDHC_DSADDR (USDHC1_DS_ADDR)              // DMA System Address register
#define SDHC_BLKATTR (USDHC1_BLK_ATT)             // Block Attributes register
#define SDHC_CMDARG (USDHC1_CMD_ARG)              // Command Argument register
#define SDHC_XFERTYP (USDHC1_CMD_XFR_TYP)         // Transfer Type register
#define SDHC_CMDRSP0 (USDHC1_CMD_RSP0)            // Command Response 0
#define SDHC_CMDRSP1 (USDHC1_CMD_RSP1)            // Command Response 1
#define SDHC_CMDRSP2 (USDHC1_CMD_RSP2)            // Command Response 2
#define SDHC_CMDRSP3 (USDHC1_CMD_RSP3)            // Command Response 3
#define SDHC_DATPORT (USDHC1_DATA_BUFF_ACC_PORT)  // Buffer Data Port register
#define SDHC_PRSSTAT (USDHC1_PRES_STATE)          // Present State register
#define SDHC_PROCTL (USDHC1_PROT_CTRL)            // Protocol Control register
#define SDHC_SYSCTL (USDHC1_SYS_CTRL)             // System Control register
#define SDHC_IRQSTAT (USDHC1_INT_STATUS)          // Interrupt Status register
#define SDHC_IRQSTATEN \
  (USDHC1_INT_STATUS_EN)  // Interrupt Status Enable register
#define SDHC_IRQSIGEN \
  (USDHC1_INT_SIGNAL_EN)  // Interrupt Signal Enable register
#define SDHC_AC12ERR \
  (USDHC1_AUTOCMD12_ERR_STATUS)  // Auto CMD12 Error Status Register
#define SDHC_HTCAPBLT (USDHC1_HOST_CTRL_CAP)  // Host Controller Capabilities
#define SDHC_WML (USDHC1_WTMK_LVL)            // Watermark Level Register
#define SDHC_MIX_CTRL (USDHC1_MIX_CTRL)       // Mixer Control
#define SDHC_FEVT (USDHC1_FORCE_EVENT)        // Force Event register
#define SDHC_ADMAES (USDHC1_ADMA_ERR_STATUS)  // ADMA Error Status register
#define SDHC_ADSADDR (USDHC1_ADMA_SYS_ADDR)   // ADMA System Addressregister
#define SDHC_VENDOR (USDHC1_VEND_SPEC)        // Vendor Specific register
#define SDHC_MMCBOOT (USDHC1_MMC_BOOT)        // MMC Boot register
#define SDHC_VENDOR2 (USDHC2_VEND_SPEC2)      // Vendor Specific2 register
//
#define IRQ_SDHC IRQ_SDHC1

#define SDHC_MAX_DVS (0xF + 1U)
#define SDHC_MAX_CLKFS (0xFF + 1U)
#define SDHC_PREV_DVS(x) ((x) -= 1U)
#define SDHC_PREV_CLKFS(x, y) ((x) >>= (y))

#define CCM_CSCDR1_USDHC1_CLK_PODF_MASK (0x7 << 11)
#define CCM_CSCDR1_USDHC1_CLK_PODF(n) (((n)&0x7) << 11)

#define IOMUXC_SW_PAD_CTL_PAD_SRE ((0x1 <) < 0)
#define IOMUXC_SW_PAD_CTL_PAD_PKE ((0x1) << 12)
#define IOMUXC_SW_PAD_CTL_PAD_PUE ((0x1) << 13)
#define IOMUXC_SW_PAD_CTL_PAD_HYS ((0x1) << 16)
#define IOMUXC_SW_PAD_CTL_PAD_SPEED(n) (((n)&0x3) << 6)
#define IOMUXC_SW_PAD_CTL_PAD_PUS(n) (((n)&0x3) << 14)
#define IOMUXC_SW_PAD_CTL_PAD_PUS_MASK ((0x3) << 14)
#define IOMUXC_SW_PAD_CTL_PAD_DSE(n) (((n)&0x7) << 3)
#define IOMUXC_SW_PAD_CTL_PAD_DSE_MASK ((0x7) << 3)
#endif  // defined(__IMXRT1062__)
#endif  // SdioTeensy_h