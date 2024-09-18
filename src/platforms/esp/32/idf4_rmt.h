/*
 * Integration into FastLED ClocklessController
 * Copyright (c) 2024, Zach Vorhies
 * Copyright (c) 2018,2019,2020 Samuel Z. Guyer
 * Copyright (c) 2017 Thomas Basler
 * Copyright (c) 2017 Martin F. Falatic
 *
 * ESP32 support is provided using the RMT peripheral device -- a unit
 * on the chip designed specifically for generating (and receiving)
 * precisely-timed digital signals. Nominally for use in infrared
 * remote controls, we use it to generate the signals for clockless
 * LED strips. The main advantage of using the RMT device is that,
 * once programmed, it generates the signal asynchronously, allowing
 * the CPU to continue executing other code. It is also not vulnerable
 * to interrupts or other timing problems that could disrupt the signal.
 *
 * The implementation strategy is borrowed from previous work and from
 * the RMT support built into the ESP32 IDF. The RMT device has 8
 * channels, which can be programmed independently to send sequences
 * of high/low bits. Memory for each channel is limited, however, so
 * in order to send a long sequence of bits, we need to continuously
 * refill the buffer until all the data is sent. To do this, we fill
 * half the buffer and then set an interrupt to go off when that half
 * is sent. Then we refill that half while the second half is being
 * sent. This strategy effectively overlaps computation (by the CPU)
 * and communication (by the RMT).
 *
 * Since the RMT device only has 8 channels, we need a strategy to
 * allow more than 8 LED controllers. Our driver assigns controllers
 * to channels on the fly, queuing up controllers as necessary until a
 * channel is free. The main showPixels routine just fires off the
 * first 8 controllers; the interrupt handler starts new controllers
 * asynchronously as previous ones finish. So, for example, it can
 * send the data for 8 controllers simultaneously, but 16 controllers
 * would take approximately twice as much time.
 *
 * There is a #define that allows a program to control the total
 * number of channels that the driver is allowed to use. It defaults
 * to 8 -- use all the channels. Setting it to 1, for example, results
 * in fully serial output:
 *
 *     #define FASTLED_RMT_MAX_CHANNELS 1
 *
 * OTHER RMT APPLICATIONS
 *
 * There may be a performance penalty for using this mode. We need to
 * compute the RMT signal for the entire LED strip ahead of time,
 * rather than overlapping it with communication. We also need a large
 * buffer to hold the signal specification. Each bit of pixel data is
 * represented by a 32-bit pulse specification, so it is a 32X blow-up
 * in memory use.
 *
 * NEW: Use of Flash memory on the ESP32 can interfere with the timing
 *      of pixel output. The ESP-IDF system code disables all other
 *      code running on *either* core during these operation. To prevent
 *      this from happening, define this flag. It will force flash
 *      operations to wait until the show() is done.
 *
 * #define FASTLED_ESP32_FLASH_LOCK 1
 *
 * NEW (June 2020): The RMT controller has been split into two
 *      classes: ClocklessController, which is an instantiation of the
 *      FastLED CPixelLEDController template, and ESP32RMTController,
 *      which just handles driving the RMT peripheral. One benefit of
 *      this design is that ESP32RMTContoller is not a template, so
 *      its methods can be marked with the IRAM_ATTR and end up in
 *      IRAM memory. Another benefit is that all of the color channel
 *      processing is done up-front, in the templated class, so we
 *      can fill the RMT buffers more quickly.
 *
 *      IN THEORY, this design would also allow FastLED.show() to
 *      send the data while the program continues to prepare the next
 *      frame of data.
 *
 * #define FASTLED_RMT_SERIAL_DEBUG 1
 *
 * NEW (Oct 2021): If set enabled (Set to 1), output errorcodes to
 *      Serial for debugging if not ESP_OK. Might be useful to find
 *      bugs or problems with GPIO PINS.
 *
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com *
 *
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

#if defined(CONFIG_RMT_SUPPRESS_DEPRECATE_WARN)
#undef CONFIG_RMT_SUPPRESS_DEPRECATE_WARN
#endif
#define CONFIG_RMT_SUPPRESS_DEPRECATE_WARN 1

#include "namespace.h"
#include "eorder.h"
#include "rgbw.h"
#include "pixel_iterator.h"

FASTLED_NAMESPACE_BEGIN



// NOT CURRENTLY IMPLEMENTED:
// -- Set to true to print debugging information about timing
//    Useful for finding out if timing is being messed up by other things
//    on the processor (WiFi, for example)
// #ifndef FASTLED_RMT_SHOW_TIMER
// #define FASTLED_RMT_SHOW_TIMER false
// #endif

class ESP32RMTController;

class RmtController
{
public:
    // When built_in_driver is true then the entire RMT datablock is
    // generated ahead of time. This eliminates the flicker at the
    // cost of using WAY more memory (24x the size of the pixel data
    // as opposed to 2x - the size of an additional pixel buffer for
    // stream encoding).
    static void init(int pin, bool built_in_driver);
    RmtController() = delete;
    RmtController(RmtController &&) = delete;
    RmtController &operator=(const RmtController &) = delete;
    RmtController(const RmtController &) = delete;
    RmtController(
        int DATA_PIN,
        int T1, int T2, int T3,  // Bit timings.
        int maxChannel,
        bool built_in_driver);
    ~RmtController();

    void showPixels(PixelIterator &pixels);

private:
    void ingest(uint8_t val);
    void showPixels();
    bool built_in_driver();
    uint8_t *getPixelBuffer(int size_in_bytes);
    void initPulseBuffer(int size_in_bytes);
    void loadAllPixelsToRmtSymbolData(PixelIterator& pixels);

    void loadPixelDataForStreamEncoding(PixelIterator& pixels);
    ESP32RMTController *pImpl = nullptr;
};

FASTLED_NAMESPACE_END
