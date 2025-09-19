#ifndef __INC_CLOCKLESS_ARM_RP2040
#define __INC_CLOCKLESS_ARM_RP2040

#include "hardware/structs/sio.h"

#if FASTLED_RP2040_CLOCKLESS_M0_FALLBACK || !FASTLED_RP2040_CLOCKLESS_PIO
#include "../common/m0clockless.h"
#endif

#if FASTLED_RP2040_CLOCKLESS_PIO
#include "hardware/clocks.h"
#include "hardware/dma.h"
// compiler throws a warning about comparison that is always true
// silence that so users don't see it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

#include "pio_gen.h"
#endif

/*
 * This clockless implementation uses RP2040's PIO feature to perform
 * non-blocking transfers to LEDs with very little memory overhead.
 * (allocates one buffer of equal size to the data to be sent)
 *
 * The SDK-provided claims system is used so that resources can used without
 * interfering with other code that behaves well and uses claims.
 *
 * Resource usage is 4 instructions of program memory on the first PIO instance
 * with an unclaimed state machine, said unclaimed PIO state machine, and one
 * DMA channel per instance of ClocklessController.
 * Additionally, one interrupt handler for DMA_IRQ_0 (configurable as shared or
 * exclusive via FASTLED_RP2040_CLOCKLESS_IRQ_SHARED) is used regardless of how
 * many instances are created.
 * 
 * The DMA handler is likely the only significant risk in terms of conflicts,
 * and users can adapt other code to use DMA_IRQ_1 and/or adopt shared handlers
 * to avoid this becoming an issue.
 */

FASTLED_NAMESPACE_BEGIN
#define FASTLED_HAS_CLOCKLESS 1

#if FASTLED_RP2040_CLOCKLESS_PIO
static CMinWait<0> *dma_chan_waits[NUM_DMA_CHANNELS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static inline void __isr clockless_dma_complete_handler() {
    for (unsigned int i = 0; i < NUM_DMA_CHANNELS; i++) {
        // if dma triggered for this channel and it's been used (has a CMinWait)
        if ((dma_hw->ints0 & (1 << i)) && dma_chan_waits[i]) {
            dma_hw->ints0 = (1 << i); // clear/ack IRQ
            dma_chan_waits[i]->mark(); // mark the wait
            return;
        }
    }
}
static bool clockless_isr_installed = false;
#endif

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
#if FASTLED_RP2040_CLOCKLESS_PIO
    int dma_channel = -1;
    void *dma_buf = nullptr;
    size_t dma_buf_size = 0;
    
    float pio_clock_multiplier;
    int T1_mult, T2_mult, T3_mult;
    
    // increase wait time by time taken to send 4 words (to flush PIO TX buffer)
    CMinWait<WAIT_TIME + ( ((T1 + T2 + T3) * 32 * 4) / (CLOCKLESS_FREQUENCY / 1000000) )> mWait;
    
    // start a DMA transfer to the PIO state machine from addr (transfer count 32 bit words)
    static void do_dma_transfer(int channel, const void *addr, uint count) {
        dma_channel_set_read_addr(channel, addr, false);
        dma_channel_set_trans_count(channel, count, true);
    }
    
    // writes bits to an in-memory buffer (to DMA from)
    // pico has enough memory to not really care about using a buffer for DMA
    template<int BITS> __attribute__ ((always_inline)) inline static int writeBitsToBuf(int32_t *out_buf, unsigned int bitpos, uint8_t b)  {
        // not really optimised and I haven't checked output assembly, but this should take ~50 cycles worst case
        // (and on average substantially fewer -- LEDs without XTRA0 should never trigger the second half of the function)
        
        // position of word that takes highest bits (first word used)
        int wordpos_1 = bitpos >> 5; // bitpos / 32;
        
        // number of bits from the byte that fit into first word
        int bitcnt_1 = 32 - (bitpos & 0b11111); // bitpos % 32;
        // shift required to place byte within the word
        int bitshift_1 = bitcnt_1 - 8;
        // mask for output bits that are taken from input
        // int32_t bitmask_1 = 0xFF << bitshift_1;
        int32_t bitmask_1 = ((1 << BITS) - 1) << (bitshift_1 - (BITS-8));
        
        out_buf[wordpos_1] = (out_buf[wordpos_1] & ~bitmask_1) | ((b << bitshift_1) & bitmask_1);
        
        if (bitcnt_1 >= BITS) return BITS; // fast case for entire byte fitting in word
        
        // number of bits from the byte to place into second word
        int bitcnt_2 = 8 - bitcnt_1;
        // shift required to place byte within the word
        int bitshift_2 = 32 - bitcnt_2;
        // mask for output bits that are taken from input
        // int32_t bitmask_2 = ((1 << bitcnt_2) - 1) << bitshift_2;
        int32_t bitmask_2 = ((1 << (bitcnt_2 + (BITS-8))) - 1) << (bitshift_2 - (BITS-8)); // fixed XTRA0
        
        out_buf[wordpos_1 + 1] = (out_buf[wordpos_1 + 1] & ~bitmask_2) | ((b << bitshift_2) & bitmask_2);
        
        return BITS;
    }
#else
    CMinWait<WAIT_TIME> mWait;
#endif
public:
    virtual void init() {
#if FASTLED_RP2040_CLOCKLESS_PIO
        if (dma_channel != -1) return; // maybe init was called twice somehow? not sure if possible
#endif
        
        // start by configuring pin as output for blocking fallback
        FastPin<DATA_PIN>::setOutput();
        
#if FASTLED_RP2040_CLOCKLESS_PIO
        // convert from input timebase to one that the PIO program can handle
        int max_t = T1 > T2 ? T1 : T2;
        max_t = T3 > max_t ? T3 : max_t;
        
        if (max_t > CLOCKLESS_PIO_MAX_TIME_PERIOD) {
            pio_clock_multiplier = (float)CLOCKLESS_PIO_MAX_TIME_PERIOD / max_t;
            T1_mult = pio_clock_multiplier * T1;
            T2_mult = pio_clock_multiplier * T2;
            T3_mult = pio_clock_multiplier * T3;
        }
        else {
            pio_clock_multiplier = 1.f;
            T1_mult = T1;
            T2_mult = T2;
            T3_mult = T3;
        }
        
        PIO pio;
        int sm;
        int offset = -1;
        
	#if defined(PICO_RP2040)
        // find an unclaimed PIO state machine and upload the clockless program if possible
        // there's two PIO instances, each with four state machines, so this should usually work out fine
		const PIO pios[NUM_PIOS] = { pio0, pio1 };
	#elif defined(PICO_RP2350)
		// RP2350 features three PIO instances!
		const PIO pios[NUM_PIOS] = { pio0, pio1, pio2 };
	#endif
        // iterate over PIO instances
        for (unsigned int i = 0; i < NUM_PIOS; i++) {
            pio = pios[i];
            sm = pio_claim_unused_sm(pio, false); // claim a state machine
            if (sm == -1) continue; // skip this PIO if no unused sm
            
            offset = add_clockless_pio_program(pio, T1_mult, T2_mult, T3_mult);
            if (offset == -1) {
                pio_sm_unclaim(pio, sm); // unclaim the state machine and skip this PIO
                continue;                // if program couldn't be added
            }
            
            break; // found pio and sm that work
        }
        if (offset == -1) return; // couldn't find good pio and sm
        
        
        // claim an unused DMA channel (there's 12 in total,, so this should also usually work out fine)
        dma_channel = dma_claim_unused_channel(false);
        if (dma_channel == -1) return; // no free DMA channel
        
        
        // setup PIO state machine
        pio_gpio_init(pio, DATA_PIN);
        pio_sm_set_consecutive_pindirs(pio, sm, DATA_PIN, 1, true);
        
        pio_sm_config c = clockless_pio_program_get_default_config(offset);
        sm_config_set_set_pins(&c, DATA_PIN, 1);
        sm_config_set_out_pins(&c, DATA_PIN, 1);
        sm_config_set_out_shift(&c, false, true, 32);
        
        // uncommenting this makes the FIFO 8 words long,
        // which seems like it won't actually benefit us
        // sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
        
        float div = clock_get_hz(clk_sys) / (pio_clock_multiplier * CLOCKLESS_FREQUENCY);
        sm_config_set_clkdiv(&c, div);
        
        pio_sm_init(pio, sm, offset, &c);
        pio_sm_set_enabled(pio, sm, true);
        
        
        // setup DMA
        dma_channel_config channel_config = dma_channel_get_default_config(dma_channel);
        channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
        dma_channel_configure(dma_channel,
                              &channel_config,
                              &pio->txf[sm],
                              NULL,   // address set when making transfer
                              1,      // count set when making transfer
                              false); // don't trigger now
        
        
        // setup DMA complete interrupt handler to update mWait time after transfer
        
        // store a pointer to mWait of this instance to a global array for the interrupt handler
        // kinda dirty hack here to cast to CMinWait<0>*, but only mark is used, which isn't affected by the template var WAIT
        dma_chan_waits[dma_channel] = (CMinWait<0>*)&mWait;
        
        if (!clockless_isr_installed) {
#if FASTLED_RP2040_CLOCKLESS_IRQ_SHARED
            irq_add_shared_handler(DMA_IRQ_0, clockless_dma_complete_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
#else
            irq_set_exclusive_handler(DMA_IRQ_0, clockless_dma_complete_handler);
#endif
            irq_set_enabled(DMA_IRQ_0, true);
            clockless_isr_installed = true;
        }
        dma_channel_set_irq0_enabled(dma_channel, true);
#endif // FASTLED_RP2040_CLOCKLESS_PIO
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
#if FASTLED_RP2040_CLOCKLESS_PIO
        if (dma_channel == -1) { // setup failed, so fall back to a blocking implementation
#if FASTLED_RP2040_CLOCKLESS_M0_FALLBACK
            showRGBBlocking(pixels);
#endif
            return;
        }
        
        // wait for past transfer to finish
        // call when previous pixels are done will run without blocking,
        // call when previous pixels are still being transmitted should block until complete
        
        // a potential improvement here would be to prepare data for the output before waiting,
        // but that would require a smarter DMA buffer system
        // (currently, the gap between LEDs is greater than 50us due to the time taken)
        if (dma_channel_is_busy(dma_channel)) {
            dma_channel_wait_for_finish_blocking(dma_channel);
        }
        mWait.wait();
        
        showRGBInternal(pixels);
#else
        mWait.wait();
        showRGBBlocking(pixels);
        mWait.mark();
#endif
    }

#if FASTLED_RP2040_CLOCKLESS_PIO
    void showRGBInternal(PixelController<RGB_ORDER> pixels) {
        size_t req_buf_size = (pixels.mLen * 3 * (8+XTRA0) + 31) / 32;
        
        // (re)allocate DMA buffer if not large enough to hold req_buf_size 32-bit words
        // pico has enough memory to not really care about using a buffer for DMA
        // just give up on failure
        if (dma_buf_size < req_buf_size) {
            if (dma_buf != nullptr)
                free(dma_buf);
            
            dma_buf = malloc(req_buf_size * 4);
            if (dma_buf == nullptr) {
                dma_buf_size = 0;
                return;
            }
            dma_buf_size = req_buf_size;
            
            // fill with zeroes to ensure XTRA0s are really zero without needing extra work
            memset(dma_buf, 0, dma_buf_size * 4);
        }
        
        unsigned int bitpos = 0;
        
        pixels.preStepFirstByteDithering();
        uint8_t b = pixels.loadAndScale0();
        
        while(pixels.has(1)) {
            pixels.stepDithering();

            // Write first byte, read next byte
            bitpos += writeBitsToBuf<8+XTRA0>((int32_t*)(dma_buf), bitpos, b);
            b = pixels.loadAndScale1();

            // Write second byte, read 3rd byte
            bitpos += writeBitsToBuf<8+XTRA0>((int32_t*)(dma_buf), bitpos, b);
            b = pixels.loadAndScale2();

            // Write third byte, read 1st byte of next pixel
            bitpos += writeBitsToBuf<8+XTRA0>((int32_t*)(dma_buf), bitpos, b);
            b = pixels.advanceAndLoadAndScale0();
        };
        
        do_dma_transfer(dma_channel, dma_buf, req_buf_size);
    }
#endif // FASTLED_RP2040_CLOCKLESS_PIO
    
#if FASTLED_RP2040_CLOCKLESS_M0_FALLBACK
    void showRGBBlocking(PixelController<RGB_ORDER> pixels) {
        struct M0ClocklessData data;
        data.d[0] = pixels.d[0];
        data.d[1] = pixels.d[1];
        data.d[2] = pixels.d[2];
        data.s[0] = pixels.mColorAdjustment.premixed[0];
        data.s[1] = pixels.mColorAdjustment.premixed[1];
        data.s[2] = pixels.mColorAdjustment.premixed[2];
        data.e[0] = pixels.e[0];
        data.e[1] = pixels.e[1];
        data.e[2] = pixels.e[2];
        data.adj = pixels.mAdvance;

        typedef FastPin<DATA_PIN> pin;
        volatile uint32_t *portBase = &sio_hw->gpio_out;
        const int portSetOff = (uint32_t)&sio_hw->gpio_set - (uint32_t)&sio_hw->gpio_out;
        const int portClrOff = (uint32_t)&sio_hw->gpio_clr - (uint32_t)&sio_hw->gpio_out;
        
        cli();
        showLedData<portSetOff, portClrOff, T1, T2, T3, RGB_ORDER, WAIT_TIME>(portBase, pin::mask(), pixels.mData, pixels.mLen, &data);
        sei();
    }
#endif

};

FASTLED_NAMESPACE_END


#endif // __INC_CLOCKLESS_ARM_RP2040
