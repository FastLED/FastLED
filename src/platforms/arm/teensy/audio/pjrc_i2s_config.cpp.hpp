// IWYU pragma: private

#include "platforms/arm/teensy/audio/pjrc_i2s_config.h" // IWYU pragma: keep

#if defined(__IMXRT1062__)

// IWYU pragma: begin_keep
#include <Arduino.h>
#include "utility/imxrt_hw.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace teensy {

namespace {

// Keep this import independent of the PJRC Audio library's AudioStream.h.
constexpr int kAudioSampleRateHz = 44100;

} // namespace

void config_i2s1()
{
	CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

	if ((I2S1_TCSR & I2S_TCSR_TE) != 0 || (I2S1_RCSR & I2S_RCSR_RE) != 0) return;

	int fs = kAudioSampleRateHz;
	int n1 = 4;
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);
	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);

	CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
	           | CCM_CSCMR1_SAI1_CLK_SEL(2);
	CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
	           | CCM_CS1CDR_SAI1_CLK_PRED(n1-1)
	           | CCM_CS1CDR_SAI1_CLK_PODF(n2-1);
	IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
	                | (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));

	CORE_PIN23_CONFIG = 3;
	CORE_PIN20_CONFIG = 3;
	CORE_PIN21_CONFIG = 3;

	int rsync = 0;
	int tsync = 1;
	I2S1_TMR = 0;
	I2S1_TCR1 = I2S_TCR1_RFW(1);
	I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP
	           | I2S_TCR2_BCD | I2S_TCR2_DIV(1) | I2S_TCR2_MSEL(1);
	I2S1_TCR3 = I2S_TCR3_TCE;
	I2S1_TCR4 = I2S_TCR4_FRSZ(2-1) | I2S_TCR4_SYWD(32-1) | I2S_TCR4_MF
	           | I2S_TCR4_FSD | I2S_TCR4_FSE | I2S_TCR4_FSP;
	I2S1_TCR5 = I2S_TCR5_WNW(32-1) | I2S_TCR5_W0W(32-1) | I2S_TCR5_FBT(32-1);

	I2S1_RMR = 0;
	I2S1_RCR1 = I2S_RCR1_RFW(1);
	I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_BCP
	           | I2S_RCR2_BCD | I2S_RCR2_DIV(1) | I2S_RCR2_MSEL(1);
	I2S1_RCR3 = I2S_RCR3_RCE;
	I2S1_RCR4 = I2S_RCR4_FRSZ(2-1) | I2S_RCR4_SYWD(32-1) | I2S_RCR4_MF
	           | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
	I2S1_RCR5 = I2S_RCR5_WNW(32-1) | I2S_RCR5_W0W(32-1) | I2S_RCR5_FBT(32-1);
}

void config_i2s2()
{
	CCM_CCGR5 |= CCM_CCGR5_SAI2(CCM_CCGR_ON);

	if (I2S2_TCSR & I2S_TCSR_TE) return;
	if (I2S2_RCSR & I2S_RCSR_RE) return;

	int fs = kAudioSampleRateHz;
	int n1 = 4;
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);
	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);

	CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI2_CLK_SEL_MASK))
	           | CCM_CSCMR1_SAI2_CLK_SEL(2);
	CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
	           | CCM_CS2CDR_SAI2_CLK_PRED(n1-1)
	           | CCM_CS2CDR_SAI2_CLK_PODF(n2-1);
	IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL_MASK))
	                | (IOMUXC_GPR_GPR1_SAI2_MCLK_DIR | IOMUXC_GPR_GPR1_SAI2_MCLK3_SEL(0));

	CORE_PIN33_CONFIG = 2;
	CORE_PIN4_CONFIG = 2;
	CORE_PIN3_CONFIG = 2;

	int rsync = 1;
	int tsync = 0;
	I2S2_TMR = 0;
	I2S2_TCR1 = I2S_TCR1_RFW(1);
	I2S2_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP
	           | I2S_TCR2_BCD | I2S_TCR2_DIV(1) | I2S_TCR2_MSEL(1);
	I2S2_TCR3 = I2S_TCR3_TCE;
	I2S2_TCR4 = I2S_TCR4_FRSZ(2-1) | I2S_TCR4_SYWD(32-1) | I2S_TCR4_MF
	           | I2S_TCR4_FSD | I2S_TCR4_FSE | I2S_TCR4_FSP;
	I2S2_TCR5 = I2S_TCR5_WNW(32-1) | I2S_TCR5_W0W(32-1) | I2S_TCR5_FBT(32-1);

	I2S2_RMR = 0;
	I2S2_RCR1 = I2S_RCR1_RFW(1);
	I2S2_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_BCP
	           | I2S_RCR2_BCD | I2S_RCR2_DIV(1) | I2S_RCR2_MSEL(1);
	I2S2_RCR3 = I2S_RCR3_RCE;
	I2S2_RCR4 = I2S_RCR4_FRSZ(2-1) | I2S_RCR4_SYWD(32-1) | I2S_RCR4_MF
	           | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
	I2S2_RCR5 = I2S_RCR5_WNW(32-1) | I2S_RCR5_W0W(32-1) | I2S_RCR5_FBT(32-1);
}

} // namespace teensy
} // namespace platforms
} // namespace fl

#endif // defined(__IMXRT1062__)
