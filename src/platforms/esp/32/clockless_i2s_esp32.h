/*
 * I2S Driver
 *
 * Copyright (c) 2019 Yves Bazin
 * Copyright (c) 2019 Samuel Z. Guyer
 * Derived from lots of code examples from other people.
 *
 * The I2S implementation can drive up to 24 strips in parallel, but
 * with the following limitation: all the strips must have the same
 * timing (i.e., they must all use the same chip).
 *
 * To enable the I2S driver, add the following line *before* including
 * FastLED.h (no other changes are necessary):
 *
 * #define FASTLED_ESP32_I2S true
 *
 * The overall strategy is to use the parallel mode of the I2S "audio"
 * peripheral to send up to 24 bits in parallel to 24 different pins.
 * Unlike the RMT peripheral the I2S system cannot send bits of
 * different lengths. Instead, we set the I2S data clock fairly high
 * and then encode a signal as a series of bits. 
 *
 * For example, with a clock divider of 10 the data clock will be
 * 8MHz, so each bit is 125ns. The WS2812 expects a "1" bit to be
 * encoded as a HIGH signal for around 875ns, followed by LOW for
 * 375ns. Sending the following pattern results in the right shape
 * signal:
 *
 *    1111111000        WS2812 "1" bit encoded as 10 125ns pulses
 *
 * The I2S peripheral expects the bits for all 24 outputs to be packed
 * into a single 32-bit word. The complete signal is a series of these
 * 32-bit values -- one for each bit for each strip. The pixel data,
 * however, is stored "serially" as a series of RGB values separately
 * for each strip. To prepare the data we need to do three things: (1)
 * take 1 pixel from each strip, and (2) tranpose the bits so that
 * they are in the parallel form, (3) translate each data bit into the
 * bit pattern that encodes the signal for that bit. This code is in
 * the fillBuffer() method:
 *
 *   1. Read 1 pixel from each strip into an array; store this data by
 *      color channel (e.g., all the red bytes, then all the green
 *      bytes, then all the blue bytes). For three color channels, the
 *      array is 3 X 24 X 8 bits.
 *
 *   2. Tranpose the array so that it is 3 X 8 X 24 bits. The hardware
 *      wants the data in 32-bit chunks, so the actual form is 3 X 8 X
 *      32, with the low 8 bits unused.
 *
 *   3. Take each group of 24 parallel bits and "expand" them into a
 *      pattern according to the encoding. For example, with a 8MHz
 *      data clock, each data bit turns into 10 I2s pulses, so 24
 *      parallel data bits turn into 10 X 24 pulses.
 *
 * We send data to the I2S peripheral using the DMA interface. We use
 * two DMA buffers, so that we can fill one buffer while the other
 * buffer is being sent. Each DMA buffer holds the fully-expanded
 * pulse pattern for one pixel on up to 24 strips. The exact amount of
 * memory required depends on the number of color channels and the
 * number of pulses used to encode each bit.
 *
 * We get an interrupt each time a buffer is sent; we then fill that
 * buffer while the next one is being sent. The DMA interface allows
 * us to configure the buffers as a circularly linked list, so that it
 * can automatically start on the next buffer.
 */
/*
 * The implementation uses two DMA buffers by default. To increase the 
 * number of DMA buffers set the preprocessor definition 
 * 
 * FASTLED_ESP32_I2S_NUM_DMA_BUFFERS 
 *
 * to a value between 2 and 16. Increasing the buffer to 4 by adding
 *
 * #define FASTLED_ESP32_I2S_NUM_DMA_BUFFERS 4 
 *
 * solves flicker issues in combination with interrupts triggered by 
 * other code parts.
 */ 
/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#ifdef FASTLED_INTERNAL
#error "This should only be active for includion of FastLED.h in a sketch."
#endif
#pragma message                                                                \
    "NOTE: ESP32 support using I2S parallel driver. All strips must use the same chipset"

#include "eorder.h"
#include "platforms/esp/32/i2s/i2s_esp32dev.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1
#define NUM_COLOR_CHANNELS 3

// -- Choose which I2S device to use
#ifndef I2S_DEVICE
#define I2S_DEVICE 0
#endif

// -- Max number of controllers we can support
#ifndef FASTLED_I2S_MAX_CONTROLLERS
#define FASTLED_I2S_MAX_CONTROLLERS 24
#endif

// override default NUM_DMA_BUFFERS if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
// is defined and has a valid value
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 16
#error invalid value for FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
#endif
#define NUM_DMA_BUFFERS FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
#else
#define NUM_DMA_BUFFERS 2
#endif


// -- Array of all controllers
static CLEDController *gControllers[FASTLED_I2S_MAX_CONTROLLERS];
static int gNumControllers = 0;
static int gNumStarted = 0;



// --- I2S DMA buffers

// -- Bit patterns
//    For now, we require all strips to be the same chipset, so these
//    are global variables.

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    // -- Store the GPIO pin
    gpio_num_t mPin;

    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

    // -- Save the pixel controller
    PixelController<RGB_ORDER> *mPixels;

    // -- Make sure we can't call show() too quickly
    CMinWait<50> mWait;

  public:
    void init() {
        // -- Allocate space to save the pixel controller
        //    during parallel output
        mPixels = (PixelController<RGB_ORDER> *)malloc(
            sizeof(PixelController<RGB_ORDER>));

        // -- Construct the bit patterns for ones and zeros
        if (!i2s_is_initialized()) {
            i2s_define_bit_patterns(T1, T2, T3);
            i2s_init(I2S_DEVICE);
            i2s_set_fill_buffer_callback(fillBuffer);
        }

        gControllers[gNumControllers] = this;
        int my_index = gNumControllers;
        ++gNumControllers;

        // -- Set up the pin We have to do two things: configure the
        //    actual GPIO pin, and route the output from the default
        //    pin (determined by the I2S device) to the pin we
        //    want. We compute the default pin using the index of this
        //    controller in the array. This order is crucial because
        //    the bits must go into the DMA buffer in the same order.
        mPin = gpio_num_t(DATA_PIN);
        i2s_setup_pin(DATA_PIN, my_index);
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

  protected:
    /** Clear DMA buffer
     *
     *  Yves' clever trick: initialize the bits that we know must be 0
     *  or 1 regardless of what bit they encode.
     */
    static void empty(uint32_t *buf) { i2s_clear_dma_buffer(buf); }

    /** Fill DMA buffer
     *
     *  This is where the real work happens: take a row of pixels (one
     *  from each strip), transpose and encode the bits, and store
     *  them in the DMA buffer for the I2S peripheral to read.
     */
    static void fillBuffer() {
        // -- Alternate between buffers
        volatile uint32_t *buf = (uint32_t *)dmaBuffers[gCurBuffer]->buffer;
        gCurBuffer = (gCurBuffer + 1) % NUM_DMA_BUFFERS;

        // -- Get the requested pixel from each controller. Store the
        //    data for each color channel in a separate array.
        uint32_t has_data_mask = 0;
        for (int i = 0; i < gNumControllers; ++i) {
            // -- Store the pixels in reverse controller order starting at index
            // 23
            //    This causes the bits to come out in the right position after
            //    we transpose them.
            int bit_index = 23 - i;
            ClocklessController *pController =
                static_cast<ClocklessController *>(gControllers[i]);
            if (pController->mPixels->has(1)) {
                gPixelRow[0][bit_index] = pController->mPixels->loadAndScale0();
                gPixelRow[1][bit_index] = pController->mPixels->loadAndScale1();
                gPixelRow[2][bit_index] = pController->mPixels->loadAndScale2();
                pController->mPixels->advanceData();
                pController->mPixels->stepDithering();

                // -- Record that this controller still has data to send
                has_data_mask |= (1 << (i + 8));
            }
        }

        // -- None of the strips has data? We are done.
        if (has_data_mask == 0) {
            gDoneFilling = true;
            return;
        }
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
        gCntBuffer++;
#endif
        // -- Transpose and encode the pixel data for the DMA buffer
        // int buf_index = 0;
        for (int channel = 0; channel < NUM_COLOR_CHANNELS; ++channel) {
            i2s_transpose_and_encode(channel, has_data_mask, buf);
        }
    }

    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) {
        if (gNumStarted == 0) {
            // -- First controller: make sure everything is set up
            i2s_begin();
        }

        // -- Initialize the local state, save a pointer to the pixel
        //    data. We need to make a copy because pixels is a local
        //    variable in the calling function, and this data structure
        //    needs to outlive this call to showPixels.
        (*mPixels) = pixels;

        // -- Keep track of the number of strips we've seen
        ++gNumStarted;

        // Serial.print("Show pixels ");
        // Serial.println(gNumStarted);

        // -- The last call to showPixels is the one responsible for doing
        //    all of the actual work
        if (gNumStarted == gNumControllers) {
            for (int i = 0; i < NUM_DMA_BUFFERS; i++) {
                empty((uint32_t *)dmaBuffers[i]->buffer);
            }
            gCurBuffer = 0;
            gDoneFilling = false;
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
            // reset buffer counter (sometimes this value != 0 after last send,
            // why?)
            gCntBuffer = 0;
#endif
            // -- Prefill all buffers
            for (int i = 0; i < NUM_DMA_BUFFERS; i++)
                fillBuffer();
            // -- Make sure it's been at least 50us since last show
            mWait.wait();
            i2s_start();
            // -- Wait here while the rest of the data is sent. The interrupt
            // handler
            //    will keep refilling the DMA buffers until it is all sent; then
            //    it gives the semaphore back.
            i2s_wait();
            i2s_stop();
            mWait.mark();

            // -- Reset the counters
            gNumStarted = 0;
        }
    }

    /** Start I2S transmission
     */
    static void i2sStart() // legacy, may not be called.
    {
        i2s_start();
    }

    static void i2sReset() // legacy, may not be called.
    {
        /*
        // Serial.println("I2S reset");
        const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M |
        I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M; i2s->lc_conf.val |=
        lc_conf_reset_flags; i2s->lc_conf.val &= ~lc_conf_reset_flags;

        const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M |
        I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M; i2s->conf.val |= conf_reset_flags;
        i2s->conf.val &= ~conf_reset_flags;
        */
        i2s_reset();
    }

    static void i2sReset_DMA() // legacy, may not be called.
    {
        i2s_reset_dma();
    }

    static void i2sReset_FIFO() // legacy, may not be called.
    {
        i2s_reset_fifo();
    }

    static void i2sStop() // legacy, may not be called.
    {
        i2s_stop();
    }
};

FASTLED_NAMESPACE_END
