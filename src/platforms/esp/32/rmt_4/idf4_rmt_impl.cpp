// allow-include-after-namespace

#ifdef ESP32
#ifndef FASTLED_ESP32_I2S
#define FASTLED_INTERNAL

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_ESP32_HAS_RMT && !FASTLED_RMT5


// Inlines the rmt_set_tx_intr_en function to avoid the overhead of a function call
#define INLINE_RMT_SET_TX_INTR_DISABLE 0

#include "FastLED.h"
#include "fl/force_inline.h"
#include "fl/assert.h"
#include "platforms/esp/32/rmt_4/idf4_rmt.h"
#include "platforms/esp/32/rmt_4/idf4_rmt_impl.h"
#include "platforms/esp/32/clock_cycles.h"
#include "freertos/semphr.h"

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

#include "platforms/esp/32/esp_log_control.h"  // Control ESP logging before including esp_log.h
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

#ifndef IRAM_ATTR  // Fix for Arduino Cloud Compiler
#warning "IRAM_ATTR not defined, are you in the Arduino Cloud compiler?, disbaling IRAM_ATTR."
#define IRAM_ATTR
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"

// ignore warnings like: ignoring attribute 'section (".iram1.11")' because it conflicts with previous 'section (".iram1.2")' [-Wattributes]
// #pragma GCC diagnostic ignored "-Wattributes"


#ifndef FASTLED_RMT_SERIAL_DEBUG
#define FASTLED_RMT_SERIAL_DEBUG 0
#endif


#if FASTLED_RMT_SERIAL_DEBUG == 1
#define FASTLED_DEBUG(format, errcode, ...) FASTLED_ASSERT(format, errcode, ##__VA_ARGS__)
#else
#define FASTLED_DEBUG(format, errcode, ...) (void)errcode;
#endif

// 64 for ESP32, ESP32S2
// 48 for ESP32S3, ESP32C3, ESP32H2
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#else
// ESP32 value (only chip variant supported on older IDF)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 64
#endif
#endif

// -- RMT memory configuration
//    By default we use two memory blocks for each RMT channel instead of 1. The
//    reason is that one memory block is only 64 bits, which causes the refill
//    interrupt to fire too often. When combined with WiFi, this leads to conflicts
//    between interrupts and weird flashy effects on the LEDs. Special thanks to
//    Brian Bulkowski for finding this problem and developing a fix.
#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2
#endif

#define MAX_PULSES (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define PULSES_PER_FILL (MAX_PULSES / 2) /* Half of the channel buffer */

// -- Configuration constants
#define DIVIDER 2 /* 4, 8 still seem to work, but timings become marginal */

// -- Max number of controllers we can support
#ifndef FASTLED_RMT_MAX_CONTROLLERS
#define FASTLED_RMT_MAX_CONTROLLERS 32
#endif

// -- Max RMT TX channel
#ifndef FASTLED_RMT_MAX_CHANNELS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
// 8 for (ESP32)  4 for (ESP32S2, ESP32S3)  2 for (ESP32C3, ESP32H2)
#define FASTLED_RMT_MAX_CHANNELS SOC_RMT_TX_CANDIDATES_PER_GROUP
#else
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define FASTLED_RMT_MAX_CHANNELS 4
#else
#define FASTLED_RMT_MAX_CHANNELS 8
#endif
#endif
#endif

namespace {
    bool gUseBuiltInDriver = false;
}

// @davidlmorris 2024-08-03
// This is work-around for the issue of random fastLed freezes randomly sometimes minutes
// but usually hours after the start, probably caused by interrupts
// being swallowed by the system so that the gTX_sem semaphore is never released
// by the RMT interrupt handler causing FastLED.Show() never to return.

// The default is never return (or max ticks aka portMAX_DELAY).
// To resolve this we need to set a maximum time to hold the semaphore.
// For example: To wait a maximum of two seconds (enough time for the Esp32 to have sorted
// itself out and fast enough that it probably won't be greatly noticed by the audience)
// use:
// # define FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM (2000/portTICK_PERIOD_MS)
// (Place this in your code directly before the first call to FastLED.h)
#ifndef FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM
#define FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM (portMAX_DELAY)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    extern void spi_flash_op_lock(void);
    extern void spi_flash_op_unlock(void);

#ifdef __cplusplus
}
#endif

#define FASTLED_INTERNAL

// On some platforms like c6 and h2, the RMT clock is 40MHz but
// there seems to be an issue either with arduino or the ESP-IDF
// that the APB_CLK_FREQ is not defined correctly. So we define
// it here for the RMT. This can probably be taken out later and
// was put in on 8/29/2024 by @zackees.
#ifndef F_CPU_RMT_CLOCK_MANUALLY_DEFINED // if user has not defined it manually then...
#if defined(CONFIG_IDF_TARGET_ESP32C6) && CONFIG_IDF_TARGET_ESP32C6 == 1
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#elif defined(CONFIG_IDF_TARGET_ESP32H2) && CONFIG_IDF_TARGET_ESP32H2 == 1
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#endif

#ifdef F_CPU_RMT_CLOCK_MANUALLY_DEFINED
#define F_CPU_RMT (F_CPU_RMT_CLOCK_MANUALLY_DEFINED)
#else
#define F_CPU_RMT (APB_CLK_FREQ)
#endif
#endif //  F_CPU_RMT_CLOCK_MANUALLY_DEFINED

// -- Convert ESP32 CPU cycles to RMT device cycles, taking into account the divider
// RMT Clock is typically APB CLK, which is 80MHz on most devices, but 40MHz on ESP32-H2 and ESP32-C6
#define RMT_CYCLES_PER_SEC (F_CPU_RMT / DIVIDER)
#define RMT_CYCLES_PER_ESP_CYCLE (F_CPU / RMT_CYCLES_PER_SEC)
#define ESP_TO_RMT_CYCLES(n) ((n) / (RMT_CYCLES_PER_ESP_CYCLE))

// -- Forward reference
class ESP32RMTController;

// -- Array of all controllers
//    This array is filled at the time controllers are registered
//    (Usually when the sketch calls addLeds)
static ESP32RMTController *gControllers[FASTLED_RMT_MAX_CONTROLLERS];

// -- Current set of active controllers, indexed by the RMT
//    channel assigned to them.
static ESP32RMTController *gOnChannel[FASTLED_RMT_MAX_CHANNELS];

static int gNumControllers = 0;
static int gNumStarted = 0;
static int gNumDone = 0;
static int gNext = 0;

static portMUX_TYPE rmt_spinlock = portMUX_INITIALIZER_UNLOCKED;
static intr_handle_t gRMT_intr_handle = NULL;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
#if tskKERNEL_VERSION_MAJOR >= 7
static SemaphoreHandle_t gTX_sem = NULL;
#else
static xSemaphoreHandle gTX_sem = NULL;
#endif

// -- Make sure we can't call show() too quickly
CMinWait<50> gWait;

static bool gInitialized = false;
// -- Stored values for FASTLED_RMT_MAX_CHANNELS and FASTLED_RMT_MEM_BLOCKS
int ESP32RMTController::gMaxChannel;
int ESP32RMTController::gMemBlocks;

#if ESP_IDF_VERSION_MAJOR >= 5

// for gpio_matrix_out
#include <rom/gpio.h>

#include "fl/memfill.h"
// copied from rmt_private.h with slight changes to match the idf 4.x syntax
typedef struct
{
    struct
    {
        rmt_item32_t data32[SOC_RMT_MEM_WORDS_PER_CHANNEL];
    } chan[SOC_RMT_CHANNELS_PER_GROUP];
} rmt_block_mem_t;

// RMTMEM address is declared in <target>.peripherals.ld
extern rmt_block_mem_t RMTMEM;
#endif

// -- Write one byte's worth of RMT pulses to the big buffer
//    out: A pointer into an array at least 8 units long (one unit for each bit).
FASTLED_FORCE_INLINE void IRAM_ATTR convert_byte_to_rmt(
    FASTLED_REGISTER uint8_t byteval,
    FASTLED_REGISTER uint32_t zero,
    FASTLED_REGISTER uint32_t one,
    volatile rmt_item32_t *out)
{
    FASTLED_REGISTER uint32_t pixel_u32 = byteval;
    pixel_u32 <<= 24;
    uint32_t tmp[8];
    for (FASTLED_REGISTER uint32_t j = 0; j < 8; j++)
    {
        FASTLED_REGISTER uint32_t new_val = (pixel_u32 & 0x80000000L) ? one : zero;
        pixel_u32 <<= 1;
        // Write to a non volatile buffer to keep this fast and
        // allow the compiler to optimize this loop.
        tmp[j] = new_val;
    }

    // Now write out the values to the volatile buffer
    out[0].val = tmp[0];
    out[1].val = tmp[1];
    out[2].val = tmp[2];
    out[3].val = tmp[3];
    out[4].val = tmp[4];
    out[5].val = tmp[5];
    out[6].val = tmp[6];
    out[7].val = tmp[7];
}

void GiveGTX_sem()
{
    if (gTX_sem != NULL)
    {
        // stop waiting for more
        gNumDone = gNumControllers;
        xSemaphoreGive(gTX_sem);
    }
}

ESP32RMTController::ESP32RMTController(int DATA_PIN, int T1, int T2, int T3, int maxChannel, bool built_in_driver)
    : mPixelData(0),
      mSize(0),
      mCur(0),
      mBufSize(0),
      mWhichHalf(0),
      mBuffer(0),
      mBufferSize(0),
      mCurPulse(0),
      mBuiltInDriver(built_in_driver)
{
    // -- Store the max channel and mem blocks parameters
    gMaxChannel = maxChannel;
    gMemBlocks = FASTLED_RMT_MEM_BLOCKS;

    // -- Precompute rmt items corresponding to a zero bit and a one bit
    //    according to the timing values given in the template instantiation
    // T1H
    mOne.level0 = 1;
    mOne.duration0 = ESP_TO_RMT_CYCLES(T1 + T2); // TO_RMT_CYCLES(T1+T2);
    // T1L
    mOne.level1 = 0;
    mOne.duration1 = ESP_TO_RMT_CYCLES(T3); // TO_RMT_CYCLES(T3);

    // T0H
    mZero.level0 = 1;
    mZero.duration0 = ESP_TO_RMT_CYCLES(T1); // TO_RMT_CYCLES(T1);
    // T0L
    mZero.level1 = 0;
    mZero.duration1 = ESP_TO_RMT_CYCLES(T2 + T3); // TO_RMT_CYCLES(T2 + T3);

    gControllers[gNumControllers] = this;
    gNumControllers++;

    // -- Expected number of CPU cycles between buffer fills
    mCyclesPerFill = (T1 + T2 + T3) * PULSES_PER_FILL;

    // -- If there is ever an interval greater than 1.5 times
    //    the expected time, then bail out.
    mMaxCyclesPerFill = mCyclesPerFill + mCyclesPerFill / 2;

    mPin = gpio_num_t(DATA_PIN);
}

// -- Get or create the buffer for the pixel data
//    We can't allocate it ahead of time because we don't have
//    the PixelController object until show is called.
uint8_t *ESP32RMTController::getPixelBuffer(int size_in_bytes)
{
    // -- Free the old buffer if it will be too small
    if (mPixelData != 0 and mBufSize < size_in_bytes)
    {
        free(mPixelData);
        mPixelData = 0;
    }

    if (mPixelData == 0)
    {
        mBufSize = size_in_bytes;
        mPixelData = (uint8_t *)malloc(mBufSize);
    }

    mSize = size_in_bytes;

    return mPixelData;
}



// -- Initialize RMT subsystem
//    This only needs to be done once
void ESP32RMTController::init(gpio_num_t pin, bool built_in_driver)
{
    gUseBuiltInDriver = built_in_driver;
    if (gInitialized)
        return;
    esp_err_t espErr = ESP_OK;

    for (int i = 0; i < ESP32RMTController::gMaxChannel; i += ESP32RMTController::gMemBlocks)
    {
        gOnChannel[i] = NULL;

        // -- RMT configuration for transmission
        rmt_config_t rmt_tx;
        fl::memfill(&rmt_tx, 0, sizeof(rmt_config_t));
        rmt_tx.channel = rmt_channel_t(i);
        rmt_tx.rmt_mode = RMT_MODE_TX;
        rmt_tx.gpio_num = pin;
        rmt_tx.mem_block_num = ESP32RMTController::gMemBlocks;
        rmt_tx.clk_div = DIVIDER;
        rmt_tx.tx_config.loop_en = false;
        rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
        rmt_tx.tx_config.carrier_en = false;
        rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        rmt_tx.tx_config.idle_output_en = true;

        // -- Apply the configuration
        espErr = rmt_config(&rmt_tx);
        FASTLED_DEBUG("rmt_config result: %d", espErr);

        // TODO: Move the value out of a define for this class and use
        // the value from the define to pass into the RMT driver.
        if (built_in_driver)
        {
            rmt_driver_install(rmt_channel_t(i), 0, 0);
        }
        else
        {
            // -- Set up the RMT to send 32 bits of the pulse buffer and then
            //    generate an interrupt. When we get this interrupt we
            //    fill the other part in preparation (like double-buffering)
            espErr = rmt_set_tx_thr_intr_en(rmt_channel_t(i), true, PULSES_PER_FILL);
            FASTLED_DEBUG("rmt_set_tx_thr_intr_en result: %d", espErr);
        }
    }

    // -- Create a semaphore to block execution until all the controllers are done
    if (gTX_sem == NULL)
    {
        gTX_sem = xSemaphoreCreateBinary();
        xSemaphoreGive(gTX_sem);
    }

    if (!built_in_driver)
    {
        // -- Allocate the interrupt if we have not done so yet. This
        //    interrupt handler must work for all different kinds of
        //    strips, so it delegates to the refill function for each
        //    specific instantiation of ClocklessController.
        if (gRMT_intr_handle == NULL)
        {
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, ESP32RMTController::interruptHandler, 0, &gRMT_intr_handle);
        }
    }

    gInitialized = true;
    (void)espErr;
}

// -- Show this string of pixels
//    This is the main entry point for the pixel controller
void ESP32RMTController::showPixels()
{
    if (gNumStarted == 0)
    {
        // -- First controller: make sure everything is set up
        init(mPin, mBuiltInDriver);

#if FASTLED_ESP32_FLASH_LOCK == 1
        // -- Make sure no flash operations happen right now
        spi_flash_op_lock();
#endif
    }

    // -- Keep track of the number of strips we've seen
    gNumStarted++;

    // -- The last call to showPixels is the one responsible for doing
    //    all of the actual work
    if (gNumStarted == gNumControllers)
    {
        gNext = 0;

        // -- This Take always succeeds immediately
        xSemaphoreTake(gTX_sem, portMAX_DELAY);

        // -- Make sure it's been at least 50us since last show
        gWait.wait();

        // -- First, fill all the available channels
        int channel = 0;
        while (channel < gMaxChannel && gNext < gNumControllers)
        {
            ESP32RMTController::startNext(channel);
            // -- Important: when we use more than one memory block, we need to
            //    skip the channels that would otherwise overlap in memory.
            channel += gMemBlocks;
        }

        // -- Wait here while the data is sent. The interrupt handler
        //    will keep refilling the RMT buffers until it is all
        //    done; then it gives the semaphore back.


        while (gNumDone != gNumControllers)
        {
          bool failed = !xSemaphoreTake(gTX_sem, FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM);
          xSemaphoreGive(gTX_sem);
          if (failed) 
          {
              FASTLED_DEBUG("sending controller data failed: total %d sent: %d", gNumControllers, gNumDone);
              break;
          }
          
          if (gNext < gNumControllers) 
          {
            // find a free channel
            for (int i = 0; i < ESP32RMTController::gMaxChannel; i += ESP32RMTController::gMemBlocks)
            {
              if(gOnChannel[i] == NULL)
              {
                  ESP32RMTController::startNext(i);
                  continue;
              }
            }
          }
        }

        // -- Make sure we don't call showPixels too quickly
        gWait.mark();

        // -- Reset the counters
        gNumStarted = 0;
        gNumDone = 0;
        gNext = 0;

#if FASTLED_ESP32_FLASH_LOCK == 1
        // -- Release the lock on flash operations
        spi_flash_op_unlock();
#endif
    }
}

// -- Start up the next controller
//    This method is static so that it can dispatch to the
//    appropriate startOnChannel method of the given controller.
void ESP32RMTController::startNext(int channel)
{
    if (gNext < gNumControllers)
    {
        ESP32RMTController *pController = gControllers[gNext];
        pController->startOnChannel(channel);
        gNext++;
    }
}

// -- Start this controller on the given channel
//    This function just initiates the RMT write; it does not wait
//    for it to finish.
void ESP32RMTController::startOnChannel(int channel)
{
    esp_err_t espErr = ESP_OK;
    // -- Assign this channel and configure the RMT
    mRMT_channel = rmt_channel_t(channel);

    // -- Store a reference to this controller, so we can get it
    //    inside the interrupt handler
    gOnChannel[channel] = this;

    // -- Assign the pin to this channel
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    espErr = rmt_set_gpio(mRMT_channel, RMT_MODE_TX, mPin, false);
    FASTLED_DEBUG("rmt_set_gpio result: %d", espErr);
#else
    espErr = rmt_set_pin(mRMT_channel, RMT_MODE_TX, mPin);
    FASTLED_DEBUG("rrmt_set_pin result: %d", espErr);
#endif

    if (mBuiltInDriver)
    {
        // -- Use the built-in RMT driver to send all the data in one shot
        rmt_register_tx_end_callback(doneOnChannel, 0);
        rmt_write_items(mRMT_channel, mBuffer, mBufferSize, false);
    }
    else
    {
        // -- Use our custom driver to send the data incrementally

        // -- Initialize the counters that keep track of where we are in
        //    the pixel data and the RMT buffer
        mRMT_mem_start = &(RMTMEM.chan[mRMT_channel].data32[0]);
        mRMT_mem_ptr = mRMT_mem_start;
        mCur = 0;
        mWhichHalf = 0;
        mLastFill = 0;

        // -- Fill both halves of the RMT buffer (a totaly of 64 bits of pixel data)
        fillNext(false);
        fillNext(false);

        // -- Turn on the interrupts
        espErr = rmt_set_tx_intr_en(mRMT_channel, true);
        FASTLED_DEBUG("rmt_set_tx_intr_en result: %d", espErr);

        // -- Kick off the transmission
        portENTER_CRITICAL(&rmt_spinlock);
        tx_start();
        portEXIT_CRITICAL(&rmt_spinlock);
    }
    (void)espErr;
}

// -- Start RMT transmission
//    Setting this RMT flag is what actually kicks off the peripheral
void ESP32RMTController::tx_start()
{
    // rmt_tx_start(mRMT_channel, true);
    // Inline the code for rmt_tx_start, so it can be placed in IRAM
#if CONFIG_IDF_TARGET_ESP32C3
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.tx_conf[mRMT_channel].mem_rd_rst = 1;
    RMT.tx_conf[mRMT_channel].mem_rd_rst = 0;
    RMT.tx_conf[mRMT_channel].mem_rst = 1;
    RMT.tx_conf[mRMT_channel].mem_rst = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.tx_conf[mRMT_channel].conf_update = 1;
    RMT.tx_conf[mRMT_channel].tx_start = 1;
#elif CONFIG_IDF_TARGET_ESP32H2
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].conf_update_chn = 1;
    RMT.chnconf0[mRMT_channel].tx_start_chn = 1;
#elif CONFIG_IDF_TARGET_ESP32S3
// rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].conf_update_chn = 1;
    RMT.chnconf0[mRMT_channel].tx_start_chn = 1;
#else
    RMT.chnconf0[mRMT_channel].mem_rd_rst_n = 1;
    RMT.chnconf0[mRMT_channel].mem_rd_rst_n = 0;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_n = 1;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_n = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].conf_update_n = 1;
    RMT.chnconf0[mRMT_channel].tx_start_n = 1;
#endif
#elif CONFIG_IDF_TARGET_ESP32C6
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].conf_update_chn = 1;
    RMT.chnconf0[mRMT_channel].tx_start_chn = 1;
#elif CONFIG_IDF_TARGET_ESP32S2
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    FASTLED_DEBUG(false, "tx_start Not yet implemented for ESP32-S2 in idf 5.x");
#else
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel * 3));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val &= ~(1 << (mRMT_channel * 3));
    RMT.int_ena.val |= (1 << (mRMT_channel * 3));
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.conf_ch[mRMT_channel].conf1.tx_start = 1;
#endif
#elif CONFIG_IDF_TARGET_ESP32
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel * 3));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val &= ~(1 << (mRMT_channel * 3));
    RMT.int_ena.val |= (1 << (mRMT_channel * 3));
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.conf_ch[mRMT_channel].conf1.tx_start = 1;
#elif CONFIG_IDF_TARGET_ESP32C6
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[mRMT_channel].apb_mem_rst_chn = 0;
    // rmt_ll_clear_tx_end_interrupt(&RMT, mRMT_channel)
    RMT.int_clr.val = (1 << (mRMT_channel));
    // rmt_ll_enable_tx_end_interrupt(&RMT, mRMT_channel, true)
    RMT.int_ena.val |= (1 << mRMT_channel);
    // rmt_ll_tx_start(&RMT, mRMT_channel)
    RMT.chnconf0[mRMT_channel].conf_update_chn = 1;
    RMT.chnconf0[mRMT_channel].tx_start_chn = 1;
#else
#error Not yet implemented for unknown ESP32 target
#endif
    mLastFill = __clock_cycles();
}

void IRAM_ATTR _rmt_set_tx_intr_disable(rmt_channel_t channel)
{
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
#if INLINE_RMT_SET_TX_INTR_DISABLE
    rmt_set_tx_intr_en(channel, false);
    return;
#else

    // Inline the code for rmt_set_tx_intr_en(channel, false) and rmt_tx_stop, so it can be placed in IRAM
#if CONFIG_IDF_TARGET_ESP32C3
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << channel);
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.tx_conf[channel].tx_stop = 1;
    RMT.tx_conf[channel].conf_update = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.tx_conf[channel].mem_rd_rst = 1;
    RMT.tx_conf[channel].mem_rd_rst = 0;
    RMT.tx_conf[channel].mem_rst = 1;
    RMT.tx_conf[channel].mem_rst = 0;
#elif CONFIG_IDF_TARGET_ESP32H2
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << channel);
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.chnconf0[channel].tx_stop_chn = 1;
    RMT.chnconf0[channel].conf_update_chn = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.chnconf0[channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel].apb_mem_rst_chn = 0;
#elif CONFIG_IDF_TARGET_ESP32S3
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << channel);
// rmt_ll_tx_stop(&RMT, channel)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    RMT.chnconf0[channel].tx_stop_chn = 1;
    RMT.chnconf0[channel].conf_update_chn = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.chnconf0[channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel].apb_mem_rst_chn = 0;
#else
    RMT.chnconf0[channel].tx_stop_n = 1;
    RMT.chnconf0[channel].conf_update_n = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.chnconf0[channel].mem_rd_rst_n = 1;
    RMT.chnconf0[channel].mem_rd_rst_n = 0;
    RMT.chnconf0[channel].apb_mem_rst_n = 1;
    RMT.chnconf0[channel].apb_mem_rst_n = 0;
#endif
#elif CONFIG_IDF_TARGET_ESP32C6
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << channel);
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.chnconf0[channel].tx_stop_chn = 1;
    RMT.chnconf0[channel].conf_update_chn = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.chnconf0[channel].mem_rd_rst_chn = 1;
    RMT.chnconf0[channel].mem_rd_rst_chn = 0;
    RMT.chnconf0[channel].apb_mem_rst_chn = 1;
    RMT.chnconf0[channel].apb_mem_rst_chn = 0;
#elif CONFIG_IDF_TARGET_ESP32S2
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    FASTLED_ASSERT(false, "doneOnChannel not yet implemented for ESP32-S2 in idf 5.x");
#else
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << (channel * 3));
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.conf_ch[channel].conf1.tx_stop = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
#endif
#elif CONFIG_IDF_TARGET_ESP32
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << (channel * 3));
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.conf_ch[channel].conf1.tx_start = 0;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    // RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    // RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
#else
#error Not yet implemented for unknown ESP32 target
#endif
#endif
}

// -- A controller is done
//    This function is called when a controller finishes writing
//    its data. It is called either by the custom interrupt
//    handler (below), or as a callback from the built-in
//    interrupt handler. It is static because we don't know which
//    controller is done until we look it up.
void IRAM_ATTR ESP32RMTController::doneOnChannel(rmt_channel_t channel, void *arg)
{
    ESP32RMTController *pController = gOnChannel[channel];

    // -- Turn off output on the pin
    //    Otherwise the pin will stay connected to the RMT controller,
    //    and if the same RMT controller is used for another output
    //    pin the RMT output will be routed to both pins.
    gpio_matrix_out(pController->mPin, SIG_GPIO_OUT_IDX, 0, 0);

    // -- Turn off the interrupts
    _rmt_set_tx_intr_disable(channel);

    gOnChannel[channel] = NULL;
    gNumDone++;

    if (gUseBuiltInDriver)
    {
        xSemaphoreGive(gTX_sem);
    }
    else
    {
        portBASE_TYPE HPTaskAwoken = 0;
        xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
        if (HPTaskAwoken == pdTRUE)
            portYIELD_FROM_ISR();
    }
}

// -- Custom interrupt handler
//    This interrupt handler handles two cases: a controller is
//    done writing its data, or a controller needs to fill the
//    next half of the RMT buffer with data.
void IRAM_ATTR ESP32RMTController::interruptHandler(void *arg)
{
    // -- The basic structure of this code is borrowed from the
    //    interrupt handler in esp-idf/components/driver/rmt.c
    portENTER_CRITICAL_ISR(&rmt_spinlock);
    uint32_t intr_st = RMT.int_st.val;
    portEXIT_CRITICAL_ISR(&rmt_spinlock);
    uint8_t channel;

    for (channel = 0; channel < gMaxChannel; channel++)
    {
#if CONFIG_IDF_TARGET_ESP32S2
        int tx_done_bit = channel * 3;
        int tx_next_bit = channel + 12;
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
        int tx_done_bit = channel;
        int tx_next_bit = channel + 8;
#elif CONFIG_IDF_TARGET_ESP32H2
        int tx_done_bit = channel;
        int tx_next_bit = channel + 8;
#elif CONFIG_IDF_TARGET_ESP32C6
        int tx_done_bit = channel;     // TODO correct?
        int tx_next_bit = channel + 8; // TODO correct?
#elif CONFIG_IDF_TARGET_ESP32
        int tx_done_bit = channel * 3;
        int tx_next_bit = channel + 24;
#else
#error Not yet implemented for unknown ESP32 target
#endif

        ESP32RMTController *pController = gOnChannel[channel];
        if (pController != NULL)
        {
            if (intr_st & BIT(tx_next_bit))
            {
                // -- More to send on this channel
                portENTER_CRITICAL_ISR(&rmt_spinlock);
                pController->fillNext(true);
                RMT.int_clr.val |= BIT(tx_next_bit);
                portEXIT_CRITICAL_ISR(&rmt_spinlock);
            }
            else
            {
                // -- Transmission is complete on this channel
                if (intr_st & BIT(tx_done_bit))
                {
                    portENTER_CRITICAL_ISR(&rmt_spinlock);
                    RMT.int_clr.val |= BIT(tx_done_bit);
                    doneOnChannel(rmt_channel_t(channel), 0);
                    portEXIT_CRITICAL_ISR(&rmt_spinlock);
                }
            }
        }
    }
}

// -- Fill RMT buffer
//    Puts 32 bits of pixel data into the next 32 slots in the RMT memory
//    Each data bit is represented by a 32-bit RMT item that specifies how
//    long to hold the signal high, followed by how long to hold it low.
void ESP32RMTController::fillNext(bool check_time)
{
    uint32_t now = __clock_cycles();
    if (check_time)
    {
        if (mLastFill != 0)
        {
            int32_t delta = (now - mLastFill);
            if (delta > (int32_t)mMaxCyclesPerFill)
            {
                // Serial.print(delta);
                // Serial.print(" BAIL ");
                // Serial.println(mCur);
                // rmt_tx_stop(mRMT_channel);
                // Inline the code for rmt_tx_stop, so it can be placed in IRAM
                /** -- Go back to the original strategy of just setting mCur = mSize
                       and letting the regular 'stop' process happen
                * mRMT_mem_start = 0;
                RMT.int_ena.val &= ~(1 << (mRMT_channel * 3));
                RMT.conf_ch[mRMT_channel].conf1.tx_start = 0;
                RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 1;
                RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 0;
                */
                mCur = mSize;
            }
        }
    }
    mLastFill = now;

    // -- Get the zero and one values into local variables
    FASTLED_REGISTER uint32_t one_val = mOne.val;
    FASTLED_REGISTER uint32_t zero_val = mZero.val;

    // -- Use locals for speed
    volatile FASTLED_REGISTER rmt_item32_t *pItem = mRMT_mem_ptr;

    for (FASTLED_REGISTER int i = 0; i < PULSES_PER_FILL / 8; i++)
    {
        if (mCur < mSize)
        {

            // -- Get the next four bytes of pixel data
            convert_byte_to_rmt(mPixelData[mCur], zero_val, one_val, pItem);
            pItem += 8;
            mCur++;
        }
        else
        {
            // -- No more data; signal to the RMT we are done by filling the
            //    rest of the buffer with zeros
            pItem->val = 0;
            pItem++;
        }
    }

    // -- Flip to the other half, resetting the pointer if necessary
    mWhichHalf++;
    if (mWhichHalf == 2)
    {
        pItem = mRMT_mem_start;
        mWhichHalf = 0;
    }

    // -- Store the new pointer back into the object
    mRMT_mem_ptr = pItem;
}

// -- Init pulse buffer
//    Set up the buffer that will hold all of the pulse items for this
//    controller.
//    This function is only used when the built-in RMT driver is chosen
void ESP32RMTController::initPulseBuffer(int size_in_bytes)
{
    if (mBuffer == 0)
    {
        // -- Each byte has 8 bits, each bit needs a 32-bit RMT item
        mBufferSize = size_in_bytes * 8 * 4;
        mBuffer = (rmt_item32_t *)calloc(mBufferSize, sizeof(rmt_item32_t));
    }
    mCurPulse = 0;
}

// -- Convert a byte into RMT pulses
//    This function is only used when the built-in RMT driver is chosen
void ESP32RMTController::ingest(uint8_t byteval)
{
    convert_byte_to_rmt(byteval, mZero.val, mOne.val, mBuffer + mCurPulse);
    mCurPulse += 8;
}

#pragma GCC diagnostic pop

#endif // FASTLED_RMT5

#endif // ! FASTLED_ESP32_I2S

#endif // ESP32
