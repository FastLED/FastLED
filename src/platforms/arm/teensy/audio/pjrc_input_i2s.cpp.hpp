// IWYU pragma: private

#include "platforms/arm/teensy/audio/pjrc_input_i2s.h"
#include "platforms/arm/teensy/audio/pjrc_i2s_config.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/stdint.h"

// IWYU pragma: begin_keep
#include <DMAChannel.h>
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace teensy {

#if !defined(KINETISL)

struct I2sRxBuffer {
    fl::u32 data[AUDIO_BLOCK_SAMPLES] __attribute__((aligned(32)));
};

I2sRxBuffer& i2s_rx_buffer() {
    static DMAMEM I2sRxBuffer buffer;
    return buffer;
}

struct I2sInputState {
    I2sInputState() : dma(false), rx_buffer(i2s_rx_buffer().data) {}

    DMAChannel dma;
    fl::u32* rx_buffer;
    audio_block_t* block_left = nullptr;
    audio_block_t* block_right = nullptr;
    fl::u16 block_offset = 0;
    bool update_responsibility = false;
};

#else

#define NUM_SAMPLES (AUDIO_BLOCK_SAMPLES / 2)

struct I2sRxBuffer {
    fl::i16 data[NUM_SAMPLES * 2];
};

I2sRxBuffer& i2s_rx_buffer1() {
    static DMAMEM I2sRxBuffer buffer;
    return buffer;
}

I2sRxBuffer& i2s_rx_buffer2() {
    static DMAMEM I2sRxBuffer buffer;
    return buffer;
}

struct I2sInputState {
    I2sInputState()
        : dma1(false), dma2(false), rx_buffer1(i2s_rx_buffer1().data),
          rx_buffer2(i2s_rx_buffer2().data) {}

    DMAChannel dma1;
    DMAChannel dma2;
    fl::i16* rx_buffer1;
    fl::i16* rx_buffer2;
    audio_block_t* block_left = nullptr;
    audio_block_t* block_right = nullptr;
    bool update_responsibility = false;
};

#endif

I2sInputState& i2s_input_state() {
    return fl::Singleton<I2sInputState>::instance();
}

} // namespace teensy
} // namespace platforms
} // namespace fl

namespace fl {
namespace platforms {
namespace teensy {

#if !defined(KINETISL)

void PjrcAudioInputI2S::begin(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    state.dma.begin(true);
#if defined(__IMXRT1062__)
    fl::platforms::teensy::config_i2s1();
#endif

#if defined(KINETISK)
    CORE_PIN13_CONFIG = PORT_PCR_MUX(4);
    state.dma.TCD->SADDR = (void *)((fl::u32)&I2S0_RDR0 + 2);
    state.dma.TCD->SOFF = 0;
    state.dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    state.dma.TCD->NBYTES_MLNO = 2;
    state.dma.TCD->SLAST = 0;
    state.dma.TCD->DADDR = state.rx_buffer;
    state.dma.TCD->DOFF = 2;
    state.dma.TCD->CITER_ELINKNO = sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2;
    state.dma.TCD->DLASTSGA = -sizeof(fl::platforms::teensy::i2s_rx_buffer().data);
    state.dma.TCD->BITER_ELINKNO = sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2;
    state.dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    state.dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
    I2S0_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
    I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE;
#elif defined(__IMXRT1062__)
    CORE_PIN8_CONFIG = 3;
    IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;
    state.dma.TCD->SADDR = (void *)((fl::u32)&I2S1_RDR0 + 2);
    state.dma.TCD->SOFF = 0;
    state.dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    state.dma.TCD->NBYTES_MLNO = 2;
    state.dma.TCD->SLAST = 0;
    state.dma.TCD->DADDR = state.rx_buffer;
    state.dma.TCD->DOFF = 2;
    state.dma.TCD->CITER_ELINKNO = sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2;
    state.dma.TCD->DLASTSGA = -sizeof(fl::platforms::teensy::i2s_rx_buffer().data);
    state.dma.TCD->BITER_ELINKNO = sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2;
    state.dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    state.dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);
    I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
#endif
    state.update_responsibility = update_setup();
    state.dma.enable();
    state.dma.attachInterrupt(isr);
}

void PjrcAudioInputI2S::isr(void)
{
#if defined(KINETISK) || defined(__IMXRT1062__)
    auto& state = fl::platforms::teensy::i2s_input_state();
    const fl::u32 daddr = (fl::u32)(state.dma.TCD->DADDR);
    state.dma.clearInterrupt();
    const fl::i16* src;
    const fl::i16* end;
    if (daddr < (fl::u32)state.rx_buffer + sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2) {
        src = (const fl::i16*)&state.rx_buffer[AUDIO_BLOCK_SAMPLES / 2];
        end = (const fl::i16*)&state.rx_buffer[AUDIO_BLOCK_SAMPLES];
        if (state.update_responsibility) AudioStream::update_all();
    } else {
        src = (const fl::i16*)&state.rx_buffer[0];
        end = (const fl::i16*)&state.rx_buffer[AUDIO_BLOCK_SAMPLES / 2];
    }
    audio_block_t* left = state.block_left;
    audio_block_t* right = state.block_right;
    if (left != nullptr && right != nullptr) {
        const fl::u16 offset = state.block_offset;
        if (offset <= AUDIO_BLOCK_SAMPLES / 2) {
            fl::i16* dest_left = &(left->data[offset]);
            fl::i16* dest_right = &(right->data[offset]);
            state.block_offset = offset + AUDIO_BLOCK_SAMPLES / 2;
            arm_dcache_delete((void*)src, sizeof(fl::platforms::teensy::i2s_rx_buffer().data) / 2);
            do {
                *dest_left++ = *src++;
                *dest_right++ = *src++;
            } while (src < end);
        }
    }
#endif
}

void PjrcAudioInputI2S::update(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    audio_block_t* new_left = allocate();
    audio_block_t* new_right = nullptr;
    audio_block_t* out_left = nullptr;
    audio_block_t* out_right = nullptr;
    if (new_left != nullptr) {
        new_right = allocate();
        if (new_right == nullptr) {
            release(new_left);
            new_left = nullptr;
        }
    }
    __disable_irq();
    if (state.block_offset >= AUDIO_BLOCK_SAMPLES) {
        out_left = state.block_left;
        state.block_left = new_left;
        out_right = state.block_right;
        state.block_right = new_right;
        state.block_offset = 0;
        __enable_irq();
        transmit(out_left, 0);
        release(out_left);
        transmit(out_right, 1);
        release(out_right);
    } else if (new_left != nullptr) {
        if (state.block_left == nullptr) {
            state.block_left = new_left;
            state.block_right = new_right;
            state.block_offset = 0;
            __enable_irq();
        } else {
            __enable_irq();
            release(new_left);
            release(new_right);
        }
    } else {
        __enable_irq();
    }
}

void PjrcAudioInputI2Sslave::begin(void)
{
    PjrcAudioInputI2S::begin();
}

#elif defined(KINETISL)

void PjrcAudioInputI2S::begin(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    fl::memset(state.rx_buffer1, 0, sizeof(fl::platforms::teensy::i2s_rx_buffer1().data));
    fl::memset(state.rx_buffer2, 0, sizeof(fl::platforms::teensy::i2s_rx_buffer2().data));
    state.dma1.begin(true);
    state.dma2.begin(true);
    CORE_PIN13_CONFIG = PORT_PCR_MUX(4);
    state.dma1.CFG->SAR = (void *)((fl::u32)&I2S0_RDR0 + 2);
    state.dma1.CFG->DCR = (state.dma1.CFG->DCR & 0xF08E0FFF) | DMA_DCR_SSIZE(2);
    state.dma1.destinationBuffer(state.rx_buffer1, sizeof(fl::platforms::teensy::i2s_rx_buffer1().data));
    state.dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
    state.dma1.interruptAtCompletion();
    state.dma1.disableOnCompletion();
    state.dma1.attachInterrupt(isr1);
    state.dma2.CFG->SAR = state.dma1.CFG->SAR;
    state.dma2.CFG->DCR = state.dma1.CFG->DCR;
    state.dma2.destinationBuffer(state.rx_buffer2, sizeof(fl::platforms::teensy::i2s_rx_buffer2().data));
    state.dma2.interruptAtCompletion();
    state.dma2.disableOnCompletion();
    state.dma2.attachInterrupt(isr2);
    I2S0_RCSR = 0;
    I2S0_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FWDE | I2S_RCSR_FR;
    I2S0_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE;
    state.update_responsibility = update_setup();
    state.dma1.enable();
}

void PjrcAudioInputI2S::update(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    if (state.block_left != nullptr) {
        transmit(state.block_left, 0);
        release(state.block_left);
        state.block_left = nullptr;
    }
    if (state.block_right != nullptr) {
        transmit(state.block_right, 1);
        release(state.block_right);
        state.block_right = nullptr;
    }
    state.block_left = allocate();
    if (state.block_left != nullptr) {
        state.block_right = allocate();
        if (state.block_right == nullptr) {
            release(state.block_left);
            state.block_left = nullptr;
        }
    }
}

inline __attribute__((always_inline, hot, optimize("O2")))
static void deinterleave(const fl::i16* src, audio_block_t* block_left,
                         audio_block_t* block_right, const fl::uint offset)
{
    if (block_left == nullptr) return;
    for (fl::uint i = 0; i < NUM_SAMPLES; i++) {
        block_left->data[i + offset] = src[i * 2];
        block_right->data[i + offset] = src[i * 2 + 1];
    }
}

void PjrcAudioInputI2S::isr1(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    state.dma2.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
    state.dma2.enable();
    state.dma1.clearInterrupt();
    state.dma1.destinationBuffer(state.rx_buffer1, sizeof(fl::platforms::teensy::i2s_rx_buffer1().data));
    deinterleave(state.rx_buffer1, state.block_left, state.block_right, 0);
}

void PjrcAudioInputI2S::isr2(void)
{
    auto& state = fl::platforms::teensy::i2s_input_state();
    state.dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
    state.dma1.enable();
    state.dma2.clearInterrupt();
    state.dma2.destinationBuffer(state.rx_buffer2, sizeof(fl::platforms::teensy::i2s_rx_buffer2().data));
    deinterleave(state.rx_buffer2, state.block_left, state.block_right, NUM_SAMPLES);
    if (state.update_responsibility) AudioStream::update_all();
}

void PjrcAudioInputI2Sslave::begin(void)
{
    PjrcAudioInputI2S::begin();
}

#endif

} // namespace teensy
} // namespace platforms
} // namespace fl
