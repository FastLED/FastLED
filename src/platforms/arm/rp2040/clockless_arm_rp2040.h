#ifndef __INC_CLOCKLESS_ARM_RP2040
#define __INC_CLOCKLESS_ARM_RP2040

#include "hardware/clocks.h"
#include "hardware/dma.h"

// compiler throws a warning about comparison that is always true
// silence that so users don't see it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

#include "pio_gen.h"

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

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    int dma_channel = -1;
    void *dma_buf = nullptr;
    size_t dma_buf_size = 0;
    
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
public:
    virtual void init() {
        if (dma_channel != -1) return; // maybe init was called twice somehow? not sure if possible
        
        PIO pio;
        int sm;
        int offset = -1;
        
        // find an unclaimed PIO state machine and upload the clockless program if possible
        // there's two PIO instances, each with four state machines, so this should usually work out fine
        static PIO pios[NUM_PIOS] = { pio0, pio1 };
        // iterate over PIO instances
        for (unsigned int i = 0; i < NUM_PIOS; i++) {
            pio = pios[i];
            sm = pio_claim_unused_sm(pio, false); // claim a state machine
            if (sm == -1) continue; // skip this PIO if no unused sm
            
            offset = add_clockless_pio_program(pio, T1, T2, T3);
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
        sm_config_set_sideset_pins(&c, DATA_PIN);
        sm_config_set_out_shift(&c, false, true, 32);
        
        // uncommenting this makes the FIFO 8 words long,
        // which seems like it won't actually benefit us
        // sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
        
        float div = clock_get_hz(clk_sys) / CLOCKLESS_FREQUENCY;
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
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
        if (dma_channel == -1) return; // setup failed
        
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
    }

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

};

FASTLED_NAMESPACE_END


#endif // __INC_CLOCKLESS_ARM_RP2040
