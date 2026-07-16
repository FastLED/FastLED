// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/singleton.h"
#include "platforms/arm/teensy/audio/pjrc_input_i2s2.h"
#include "platforms/arm/teensy/audio/pjrc_i2s_config.h"

#if defined(__IMXRT1062__)

// IWYU pragma: begin_keep
#include <DMAChannel.h>
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace teensy {

struct I2S2RxBuffer {
    fl::u32 data[AUDIO_BLOCK_SAMPLES];
};

inline fl::u32* i2s2_rx_buffer() {
    static DMAMEM __attribute__((aligned(32))) I2S2RxBuffer buffer;
    return buffer.data;
}

struct I2S2InputState {
    I2S2InputState() : dma(false), rx_buffer(i2s2_rx_buffer()) {}

    DMAChannel dma;
    fl::u32* rx_buffer;
    audio_block_t* block_left = nullptr;
    audio_block_t* block_right = nullptr;
    fl::u16 block_offset = 0;
    bool update_responsibility = false;

    static constexpr fl::u32 kRxBufferBytes =
        static_cast<fl::u32>(sizeof(fl::u32) * AUDIO_BLOCK_SAMPLES);
};

inline I2S2InputState& i2s2_input_state() {
    return fl::Singleton<I2S2InputState>::instance();
}

void PjrcAudioInputI2S2::begin() {
    fl::platforms::teensy::I2S2InputState& state =
        fl::platforms::teensy::i2s2_input_state();
    state.dma.begin(true);
    fl::platforms::teensy::config_i2s2();

    CORE_PIN5_CONFIG = 2;
    IOMUXC_SAI2_RX_DATA0_SELECT_INPUT = 0;

    state.dma.TCD->SADDR = reinterpret_cast<void*>( // ok reinterpret cast
        reinterpret_cast<fl::u32>(&I2S2_RDR0) + 2u); // ok reinterpret cast
    state.dma.TCD->SOFF = 0;
    state.dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    state.dma.TCD->NBYTES_MLNO = 2;
    state.dma.TCD->SLAST = 0;
    state.dma.TCD->DADDR = state.rx_buffer;
    state.dma.TCD->DOFF = 2;
    state.dma.TCD->CITER_ELINKNO = I2S2InputState::kRxBufferBytes / 2u;
    state.dma.TCD->DLASTSGA = -static_cast<fl::i32>(I2S2InputState::kRxBufferBytes);
    state.dma.TCD->BITER_ELINKNO = I2S2InputState::kRxBufferBytes / 2u;
    state.dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    state.dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_RX);
    state.dma.enable();

    I2S2_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
    I2S2_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE;

    state.update_responsibility = update_setup();
    state.dma.attachInterrupt(isr);
}

void PjrcAudioInputI2S2::isr() {
    fl::platforms::teensy::I2S2InputState& state =
        fl::platforms::teensy::i2s2_input_state();
    const fl::u32 daddr = reinterpret_cast<fl::u32>(state.dma.TCD->DADDR); // ok reinterpret cast
    state.dma.clearInterrupt();

    const fl::i16* src;
    const fl::i16* end;
    if (daddr < reinterpret_cast<fl::u32>(state.rx_buffer) + // ok reinterpret cast
                    I2S2InputState::kRxBufferBytes / 2u) {
        src = reinterpret_cast<const fl::i16*>(&state.rx_buffer[AUDIO_BLOCK_SAMPLES / 2]); // ok reinterpret cast
        end = reinterpret_cast<const fl::i16*>(&state.rx_buffer[AUDIO_BLOCK_SAMPLES]); // ok reinterpret cast
        if (state.update_responsibility) AudioStream::update_all();
    } else {
        src = reinterpret_cast<const fl::i16*>(&state.rx_buffer[0]); // ok reinterpret cast
        end = reinterpret_cast<const fl::i16*>(&state.rx_buffer[AUDIO_BLOCK_SAMPLES / 2]); // ok reinterpret cast
    }

    audio_block_t* left = state.block_left;
    audio_block_t* right = state.block_right;
    if (left == nullptr || right == nullptr || state.block_offset > AUDIO_BLOCK_SAMPLES / 2) {
        return;
    }

    const fl::u16 offset = state.block_offset;
    fl::i16* dest_left = &left->data[offset];
    fl::i16* dest_right = &right->data[offset];
    state.block_offset = static_cast<fl::u16>(offset + AUDIO_BLOCK_SAMPLES / 2);
    arm_dcache_delete(const_cast<fl::i16*>(src), I2S2InputState::kRxBufferBytes / 2u);
    while (src < end) {
        *dest_left++ = *src++;
        *dest_right++ = *src++;
    }
}

void PjrcAudioInputI2S2::update() {
    fl::platforms::teensy::I2S2InputState& state =
        fl::platforms::teensy::i2s2_input_state();
    audio_block_t* new_left = allocate();
    audio_block_t* new_right = nullptr;
    if (new_left != nullptr) {
        new_right = allocate();
        if (new_right == nullptr) {
            release(new_left);
            new_left = nullptr;
        }
    }

    __disable_irq();
    if (state.block_offset >= AUDIO_BLOCK_SAMPLES) {
        audio_block_t* out_left = state.block_left;
        audio_block_t* out_right = state.block_right;
        state.block_left = new_left;
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

void PjrcAudioInputI2S2Slave::begin() {
    fl::platforms::teensy::I2S2InputState& state =
        fl::platforms::teensy::i2s2_input_state();
    state.dma.begin(true);

    CORE_PIN5_CONFIG = 2;
    IOMUXC_SAI2_RX_DATA0_SELECT_INPUT = 0;
    fl::platforms::teensy::config_i2s2();

    state.dma.TCD->SADDR = reinterpret_cast<void*>( // ok reinterpret cast
        reinterpret_cast<fl::u32>(&I2S2_RDR0) + 2u); // ok reinterpret cast
    state.dma.TCD->SOFF = 0;
    state.dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    state.dma.TCD->NBYTES_MLNO = 2;
    state.dma.TCD->SLAST = 0;
    state.dma.TCD->DADDR = state.rx_buffer;
    state.dma.TCD->DOFF = 2;
    state.dma.TCD->CITER_ELINKNO = I2S2InputState::kRxBufferBytes / 2u;
    state.dma.TCD->DLASTSGA = -static_cast<fl::i32>(I2S2InputState::kRxBufferBytes);
    state.dma.TCD->BITER_ELINKNO = I2S2InputState::kRxBufferBytes / 2u;
    state.dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
    state.dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_RX);
    state.dma.enable();

    I2S2_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
    I2S2_TCSR |= I2S_TCSR_TE | I2S_TCSR_BCE;
    state.update_responsibility = update_setup();
    state.dma.attachInterrupt(PjrcAudioInputI2S2::isr);
}

} // namespace teensy
} // namespace platforms
} // namespace fl

#endif // defined(__IMXRT1062__)
