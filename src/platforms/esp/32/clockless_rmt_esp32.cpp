
#ifdef ESP32

#ifndef FASTLED_ESP32_I2S

#define FASTLED_INTERNAL
#include "FastLED.h"

// -- Forward reference
class ESP32RMTController;

// -- Array of all controllers
//    This array is filled at the time controllers are registered 
//    (Usually when the sketch calls addLeds)
static ESP32RMTController * gControllers[FASTLED_RMT_MAX_CONTROLLERS];

// -- Current set of active controllers, indexed by the RMT
//    channel assigned to them.
static ESP32RMTController * gOnChannel[FASTLED_RMT_MAX_CHANNELS];

static int gNumControllers = 0;
static int gNumStarted = 0;
static int gNumDone = 0;
static int gNext = 0;

static intr_handle_t gRMT_intr_handle = NULL;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
static xSemaphoreHandle gTX_sem = NULL;

// -- Make sure we can't call show() too quickly
CMinWait<50>   gWait;

static bool gInitialized = false;

// -- Stored values for FASTLED_RMT_MAX_CHANNELS and FASTLED_RMT_MEM_BLOCKS
int ESP32RMTController::gMaxChannel;
int ESP32RMTController::gMemBlocks;


ESP32RMTController::ESP32RMTController(int DATA_PIN, int T1, int T2, int T3, int maxChannel, int memBlocks)
    : mPixelData(0), 
      mSize(0), 
      mCur(0),
      mBufSize(0), 
      mWhichHalf(0),
      mBuffer(0),
      mBufferSize(0),
      mCurPulse(0)
{
    // -- Store the max channel and mem blocks parameters
    gMaxChannel = maxChannel;
    gMemBlocks = memBlocks;

    // -- Precompute rmt items corresponding to a zero bit and a one bit
    //    according to the timing values given in the template instantiation
    // T1H
    mOne.level0 = 1;
    mOne.duration0 = ESP_TO_RMT_CYCLES(T1+T2); // TO_RMT_CYCLES(T1+T2);
    // T1L
    mOne.level1 = 0;
    mOne.duration1 = ESP_TO_RMT_CYCLES(T3); // TO_RMT_CYCLES(T3);

    // T0H
    mZero.level0 = 1;
    mZero.duration0 = ESP_TO_RMT_CYCLES(T1); // TO_RMT_CYCLES(T1);
    // T0L
    mZero.level1 = 0;
    mZero.duration1 = ESP_TO_RMT_CYCLES(T2+T3); // TO_RMT_CYCLES(T2 + T3);

    gControllers[gNumControllers] = this;
    gNumControllers++;

    // -- Expected number of CPU cycles between buffer fills
    mCyclesPerFill = (T1 + T2 + T3) * PULSES_PER_FILL;

    // -- If there is ever an interval greater than 1.5 times
    //    the expected time, then bail out.
    mMaxCyclesPerFill = mCyclesPerFill + mCyclesPerFill/2;

    mPin = gpio_num_t(DATA_PIN);
}

// -- Get or create the buffer for the pixel data
//    We can't allocate it ahead of time because we don't have
//    the PixelController object until show is called.
uint8_t * ESP32RMTController::getPixelBuffer(int size_in_bytes)
{
    // -- Free the old buffer if it will be too small
    if (mPixelData != 0 and mBufSize < size_in_bytes) {
        free(mPixelData);
        mPixelData = 0;
    }

    if (mPixelData == 0) {
        mBufSize = size_in_bytes;
        mPixelData = (uint8_t *) malloc(mBufSize);
    }

    mSize = size_in_bytes;

    return mPixelData;
}

// -- Initialize RMT subsystem
//    This only needs to be done once
void ESP32RMTController::init(gpio_num_t pin)
{
    if (gInitialized) return;
    esp_err_t espErr = ESP_OK;

    for (int i = 0; i < gMaxChannel; i += gMemBlocks) {
        gOnChannel[i] = NULL;

        // -- RMT configuration for transmission
        rmt_config_t rmt_tx;
        memset(&rmt_tx, 0, sizeof(rmt_config_t));
        rmt_tx.channel = rmt_channel_t(i);
        rmt_tx.rmt_mode = RMT_MODE_TX;
        rmt_tx.gpio_num = pin;
        rmt_tx.mem_block_num = gMemBlocks;
        rmt_tx.clk_div = DIVIDER;
        rmt_tx.tx_config.loop_en = false;
        rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
        rmt_tx.tx_config.carrier_en = false;
        rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        rmt_tx.tx_config.idle_output_en = true;

        // -- Apply the configuration
        espErr = rmt_config(&rmt_tx);
        FASTLED_DEBUG("rmt_config result: %d", espErr);

        if (FASTLED_RMT_BUILTIN_DRIVER) {
            rmt_driver_install(rmt_channel_t(i), 0, 0);
        } else {
            // -- Set up the RMT to send 32 bits of the pulse buffer and then
            //    generate an interrupt. When we get this interrupt we
            //    fill the other part in preparation (like double-buffering)
            espErr = rmt_set_tx_thr_intr_en(rmt_channel_t(i), true, PULSES_PER_FILL);
            FASTLED_DEBUG("rmt_set_tx_thr_intr_en result: %d", espErr);
        }
    }

    // -- Create a semaphore to block execution until all the controllers are done
    if (gTX_sem == NULL) {
        gTX_sem = xSemaphoreCreateBinary();
        xSemaphoreGive(gTX_sem);
    }
                
    if ( ! FASTLED_RMT_BUILTIN_DRIVER) {
        // -- Allocate the interrupt if we have not done so yet. This
        //    interrupt handler must work for all different kinds of
        //    strips, so it delegates to the refill function for each
        //    specific instantiation of ClocklessController.
        if (gRMT_intr_handle == NULL)
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, interruptHandler, 0, &gRMT_intr_handle);
    }

    gInitialized = true;
    (void)espErr;
}

// -- Show this string of pixels
//    This is the main entry point for the pixel controller
void IRAM_ATTR ESP32RMTController::showPixels()
{
    if (gNumStarted == 0) {
        // -- First controller: make sure everything is set up
        ESP32RMTController::init(mPin);

#if FASTLED_ESP32_FLASH_LOCK == 1
        // -- Make sure no flash operations happen right now
        spi_flash_op_lock();
#endif
    }

    // -- Keep track of the number of strips we've seen
    gNumStarted++;

    // -- The last call to showPixels is the one responsible for doing
    //    all of the actual worl
    if (gNumStarted == gNumControllers) {
        gNext = 0;

        // -- This Take always succeeds immediately
        xSemaphoreTake(gTX_sem, portMAX_DELAY);

        // -- Make sure it's been at least 50us since last show
        gWait.wait();

        // -- First, fill all the available channels
        int channel = 0;
        while (channel < gMaxChannel && gNext < gNumControllers) {
            ESP32RMTController::startNext(channel);
            // -- Important: when we use more than one memory block, we need to
            //    skip the channels that would otherwise overlap in memory.
            channel += gMemBlocks;
        }

        // -- Wait here while the data is sent. The interrupt handler
        //    will keep refilling the RMT buffers until it is all
        //    done; then it gives the semaphore back.
        xSemaphoreTake(gTX_sem, portMAX_DELAY);
        xSemaphoreGive(gTX_sem);

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
void IRAM_ATTR ESP32RMTController::startNext(int channel)
{
    if (gNext < gNumControllers) {
        ESP32RMTController * pController = gControllers[gNext];
        pController->startOnChannel(channel);
        gNext++;
    }
}

// -- Start this controller on the given channel
//    This function just initiates the RMT write; it does not wait
//    for it to finish.
void IRAM_ATTR ESP32RMTController::startOnChannel(int channel)
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

    if (FASTLED_RMT_BUILTIN_DRIVER) {
        // -- Use the built-in RMT driver to send all the data in one shot
        rmt_register_tx_end_callback(doneOnChannel, 0);
        rmt_write_items(mRMT_channel, mBuffer, mBufferSize, false);
    } else {
        // -- Use our custom driver to send the data incrementally

        // -- Initialize the counters that keep track of where we are in
        //    the pixel data and the RMT buffer
        mRMT_mem_start = & (RMTMEM.chan[mRMT_channel].data32[0].val);
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
        tx_start();
    }
    (void)espErr;
}

// -- Start RMT transmission
//    Setting this RMT flag is what actually kicks off the peripheral
void IRAM_ATTR ESP32RMTController::tx_start()
{
    // rmt_tx_start(mRMT_channel, true);
    // Inline the code for rmt_tx_start, so it can be placed in IRAM
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
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
#elif CONFIG_IDF_TARGET_ESP32S3
    // rmt_ll_tx_reset_pointer(&RMT, mRMT_channel)
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
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
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
#else
    #error Not yet implemented for unknown ESP32 target
#endif
    mLastFill = __clock_cycles();
}

// -- A controller is done 
//    This function is called when a controller finishes writing
//    its data. It is called either by the custom interrupt
//    handler (below), or as a callback from the built-in
//    interrupt handler. It is static because we don't know which
//    controller is done until we look it up.
void IRAM_ATTR ESP32RMTController::doneOnChannel(rmt_channel_t channel, void * arg)
{
    ESP32RMTController * pController = gOnChannel[channel];

    // -- Turn off output on the pin
    // SZG: Do I really need to do this?
    gpio_matrix_out(pController->mPin, 0x100, 0, 0);

    // -- Turn off the interrupts
    // rmt_set_tx_intr_en(channel, false);

    // Inline the code for rmt_set_tx_intr_en(channel, false) and rmt_tx_stop, so it can be placed in IRAM
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
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
#elif CONFIG_IDF_TARGET_ESP32S3
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << channel);
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.chnconf0[channel].tx_stop_n = 1;
    RMT.chnconf0[channel].conf_update_n = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.chnconf0[channel].mem_rd_rst_n = 1;
    RMT.chnconf0[channel].mem_rd_rst_n = 0;
    RMT.chnconf0[channel].apb_mem_rst_n = 1;
    RMT.chnconf0[channel].apb_mem_rst_n = 0;
#elif CONFIG_IDF_TARGET_ESP32S2
    // rmt_ll_enable_tx_end_interrupt(&RMT, channel)
    RMT.int_ena.val &= ~(1 << (channel * 3));
    // rmt_ll_tx_stop(&RMT, channel)
    RMT.conf_ch[channel].conf1.tx_stop = 1;
    // rmt_ll_tx_reset_pointer(&RMT, channel)
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
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

    gOnChannel[channel] = NULL;
    gNumDone++;

    if (gNumDone == gNumControllers) {
        // -- If this is the last controller, signal that we are all done
        if (FASTLED_RMT_BUILTIN_DRIVER) {
            xSemaphoreGive(gTX_sem);
        } else {
            portBASE_TYPE HPTaskAwoken = 0;
            xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
        }
    } else {
        // -- Otherwise, if there are still controllers waiting, then
        //    start the next one on this channel
        if (gNext < gNumControllers) {
            startNext(channel);
        }
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
    uint32_t intr_st = RMT.int_st.val;
    uint8_t channel;

    for (channel = 0; channel < gMaxChannel; channel++) {
        #if CONFIG_IDF_TARGET_ESP32S2
        int tx_done_bit = channel * 3;
        int tx_next_bit = channel + 12;
        #elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
        int tx_done_bit = channel;
        int tx_next_bit = channel + 8;
        #elif CONFIG_IDF_TARGET_ESP32
        int tx_done_bit = channel * 3;
        int tx_next_bit = channel + 24;
        #else
        #error Not yet implemented for unknown ESP32 target
        #endif

        ESP32RMTController * pController = gOnChannel[channel];
        if (pController != NULL) {
            if (intr_st & BIT(tx_next_bit)) {
                // -- More to send on this channel
                pController->fillNext(true);
                RMT.int_clr.val |= BIT(tx_next_bit);
            } else {
                // -- Transmission is complete on this channel
                if (intr_st & BIT(tx_done_bit)) {
                    RMT.int_clr.val |= BIT(tx_done_bit);
                    doneOnChannel(rmt_channel_t(channel), 0);
                }
            }
        }
    }
}

// -- Fill RMT buffer
//    Puts 32 bits of pixel data into the next 32 slots in the RMT memory
//    Each data bit is represented by a 32-bit RMT item that specifies how
//    long to hold the signal high, followed by how long to hold it low.
void IRAM_ATTR ESP32RMTController::fillNext(bool check_time)
{
    uint32_t now = __clock_cycles();
    if (check_time) {
        if (mLastFill != 0) {
            int32_t delta = (now - mLastFill);
            if (delta > (int32_t)mMaxCyclesPerFill) {
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
    REGISTER uint32_t one_val = mOne.val;
    REGISTER uint32_t zero_val = mZero.val;

    // -- Use locals for speed
    volatile REGISTER uint32_t * pItem =  mRMT_mem_ptr;

    for (REGISTER int i = 0; i < PULSES_PER_FILL/8; i++) {
        if (mCur < mSize) {

            // -- Get the next four bytes of pixel data
            REGISTER uint32_t pixeldata = mPixelData[mCur] << 24;
            mCur++;
            
            // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the 
            // rmt_item32_t value corresponding to the buffered bit value
            for (REGISTER uint32_t j = 0; j < 8; j++) {
                *pItem++ = (pixeldata & 0x80000000L) ? one_val : zero_val;
                // Replaces: RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;

                pixeldata <<= 1;
            }
        } else {
            // -- No more data; signal to the RMT we are done by filling the
            //    rest of the buffer with zeros
            *pItem++ = 0;
        }
    }

    // -- Flip to the other half, resetting the pointer if necessary
    mWhichHalf++;
    if (mWhichHalf == 2) {
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
    if (mBuffer == 0) {
        // -- Each byte has 8 bits, each bit needs a 32-bit RMT item
        mBufferSize = size_in_bytes * 8 * 4;
        mBuffer = (rmt_item32_t *) calloc( mBufferSize, sizeof(rmt_item32_t));
    }
    mCurPulse = 0;
}

// -- Convert a byte into RMT pulses
//    This function is only used when the built-in RMT driver is chosen
void ESP32RMTController::convertByte(uint32_t byteval)
{
    // -- Write one byte's worth of RMT pulses to the big buffer
    byteval <<= 24;
    for (REGISTER uint32_t j = 0; j < 8; j++) {
        mBuffer[mCurPulse] = (byteval & 0x80000000L) ? mOne : mZero;
        byteval <<= 1;
        mCurPulse++;
    }
}

#endif // ! FASTLED_ESP32_I2S

#endif // ESP32
