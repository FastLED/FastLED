
#pragma once

#ifdef ESP32
#ifndef FASTLED_ESP32_I2S

#include "third_party/espressif/led_strip/src/enabled.h"

#if !FASTLED_RMT5

#define FASTLED_INTERNAL

#include "FastLED.h"
#include "idf4_rmt.h"
#include "platforms/esp/32/clock_cycles.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp32-hal.h"
// ESP_IDF_VERSION_MAJOR is defined in ESP-IDF v3.3 or later
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR > 3
#include "esp_intr_alloc.h"
#else
#include "esp_intr.h"
#endif
#include "driver/gpio.h"
#include "driver/rmt.h"
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#include "esp_private/periph_ctrl.h"
#else
#include "driver/periph_ctrl.h"
#endif
#include "freertos/semphr.h"
#include "soc/rmt_struct.h"

#include "esp_log.h"

#ifdef __cplusplus
}
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"


#define FASTLED_INTERNAL




// -- Forward reference
class ESP32RMTController;

class ESP32RMTController
{
public:
    // -- Initialize RMT subsystem
    //    This only needs to be done once. The particular pin is not important,
    //    because we need to configure the RMT channels on the fly.
    static void init(gpio_num_t pin, bool built_in_driver);

private:
    // friend function esp_rmt_init
    friend void esp_rmt_init(gpio_num_t pin, bool built_in_driver);
    friend class RmtController;

    // -- RMT has 8 channels, numbered 0 to 7
    rmt_channel_t mRMT_channel;

    // -- Store the GPIO pin
    gpio_num_t mPin;

    // -- Timing values for zero and one bits, derived from T1, T2, and T3
    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // -- Total expected time to send 32 bits
    //    Each strip should get an interrupt roughly at this interval
    uint32_t mCyclesPerFill;
    uint32_t mMaxCyclesPerFill;
    uint32_t mLastFill;

    // -- Pixel data
    uint8_t *mPixelData;
    int mSize;
    int mCur;
    int mBufSize;

    // -- RMT memory
    volatile rmt_item32_t *mRMT_mem_ptr;
    volatile rmt_item32_t *mRMT_mem_start;
    int mWhichHalf;

    // -- Buffer to hold all of the pulses. For the version that uses
    //    the RMT driver built into the ESP core.
    rmt_item32_t *mBuffer;
    uint16_t mBufferSize; // bytes
    int mCurPulse;
    bool mBuiltInDriver;

    // -- These values need to be real variables, so we can access them
    //    in the cpp file
    static int gMaxChannel;
    static int gMemBlocks;

public:
    // -- Constructor
    //    Mainly just stores the template parameters from the LEDController as
    //    member variables.
    ESP32RMTController(int DATA_PIN, int T1, int T2, int T3, int maxChannel, bool built_in_driver);

    // -- Show this string of pixels
    //    This is the main entry point for the pixel controller
    void showPixels();

    // -- Init pulse buffer
    //    Set up the buffer that will hold all of the pulse items for this
    //    controller.
    //    This function is only used when the built-in RMT driver is chosen
    void initPulseBuffer(int size_in_bytes);

    // -- Convert a byte into RMT pulses
    //    This function is only used when the built-in RMT driver is chosen
    void ingest(uint8_t byteval);

private:
    // -- Start up the next controller
    //    This method is static so that it can dispatch to the
    //    appropriate startOnChannel method of the given controller.
    static void startNext(int channel);

    // -- Start this controller on the given channel
    //    This function just initiates the RMT write; it does not wait
    //    for it to finish.
    void startOnChannel(int channel);

    // -- Start RMT transmission
    //    Setting this RMT flag is what actually kicks off the peripheral
    void tx_start();

    // -- A controller is done
    //    This function is called when a controller finishes writing
    //    its data. It is called either by the custom interrupt
    //    handler (below), or as a callback from the built-in
    //    interrupt handler. It is static because we don't know which
    //    controller is done until we look it up.
    static void IRAM_ATTR doneOnChannel(rmt_channel_t channel, void *arg);

    // -- Custom interrupt handler
    //    This interrupt handler handles two cases: a controller is
    //    done writing its data, or a controller needs to fill the
    //    next half of the RMT buffer with data.
    static void IRAM_ATTR interruptHandler(void *arg);

    // -- Fill RMT buffer
    //    Puts 32 bits of pixel data into the next 32 slots in the RMT memory
    //    Each data bit is represented by a 32-bit RMT item that specifies how
    //    long to hold the signal high, followed by how long to hold it low.
    //    NOTE: Now the default is to use 128-bit buffers, so half a buffer is
    //          is 64 bits. See FASTLED_RMT_MEM_BLOCKS
    void IRAM_ATTR fillNext(bool check_time);

    // -- Get or create the pixel data buffer
    uint8_t *getPixelBuffer(int size_in_bytes);
};


#pragma GCC diagnostic pop

#endif // FASTLED_RMT5

#endif // ! FASTLED_ESP32_I2S

#endif // ESP32
