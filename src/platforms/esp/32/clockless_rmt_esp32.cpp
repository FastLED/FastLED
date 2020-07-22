
#ifdef ESP32

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

// -- Channels that need a buffer refill
static bool gRefillChannel[FASTLED_RMT_MAX_CHANNELS];

// -- Channels that are done
static bool gDoneChannel[FASTLED_RMT_MAX_CHANNELS];

static int gNumControllers = 0;
static int gNumStarted = 0;
static int gNumDone = 0;
static int gNext = 0;

static intr_handle_t gRMT_intr_handle = NULL;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
static xSemaphoreHandle gTX_sem = NULL;

static bool gInitialized = false;

// -- Timing stuff
static uint32_t gTiming[500];
static int gTimeIndex;
static uint32_t gLastTime;
 

ESP32RMTController::ESP32RMTController(int DATA_PIN, int T1, int T2, int T3)
    : mPixelData(0), 
      mSize(0), 
      mCur(0), 
      mWhichHalf(0),
      mBuffer(0),
      mBufferSize(0),
      mCurPulse(0)
{
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

    mPin = gpio_num_t(DATA_PIN);
}

// -- Getters and setters for use in ClocklessController
uint8_t * ESP32RMTController::getPixelData(int size_in_bytes)
{
    if (mPixelData == 0) {
        mSize = size_in_bytes;
        mPixelData = (uint8_t *) calloc( mSize, sizeof(uint8_t));
    }
    return mPixelData;
}

// -- Initialize RMT subsystem
//    This only needs to be done once
void ESP32RMTController::init()
{
    if (gInitialized) return;

    for (int i = 0; i < FASTLED_RMT_MAX_CHANNELS; i++) {
        gOnChannel[i] = NULL;

        // -- RMT configuration for transmission
        rmt_config_t rmt_tx;
        rmt_tx.channel = rmt_channel_t(i);
        rmt_tx.rmt_mode = RMT_MODE_TX;
        rmt_tx.gpio_num = gpio_num_t(0);  // The particular pin will be assigned later
        rmt_tx.mem_block_num = 1;
        rmt_tx.clk_div = DIVIDER;
        rmt_tx.tx_config.loop_en = false;
        rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
        rmt_tx.tx_config.carrier_en = false;
        rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        rmt_tx.tx_config.idle_output_en = true;

        // -- Apply the configuration
        rmt_config(&rmt_tx);

        if (FASTLED_RMT_BUILTIN_DRIVER) {
            rmt_driver_install(rmt_channel_t(i), 0, 0);
        } else {
            // -- Set up the RMT to send 1 pixel of the pulse buffer and then
            //    generate an interrupt. When we get this interrupt we
            //    fill the other part in preparation (kind of like double-buffering)
            rmt_set_tx_thr_intr_en(rmt_channel_t(i), true, PULSES_PER_FILL);
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
}

// -- Show this string of pixels
//    This is the main entry point for the pixel controller
void ESP32RMTController::showPixels()
{
    if (gNumStarted == 0) {
        // -- First controller: make sure everything is set up
        ESP32RMTController::init();
        xSemaphoreTake(gTX_sem, portMAX_DELAY);

        for (int i = 0; i < FASTLED_RMT_MAX_CHANNELS; i++) {
            gRefillChannel[i] = false;
            gDoneChannel[i] = false;
        }

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

#if FASTLED_ESP32_SHOWTIMING == 1
        gTimeIndex = 0;
#endif

        // -- First, fill all the available channels
        int channel = 0;
        while (channel < FASTLED_RMT_MAX_CHANNELS && gNext < gNumControllers) {
            ESP32RMTController::startNext(channel);
            channel++;
        }

        // -- Make sure it's been at least 50us since last show
        mWait.wait();

        // -- Start them all
        for (int i = 0; i < channel; i++) {
            ESP32RMTController * pController = gControllers[i];
            pController->tx_start();
#if FASTLED_ESP32_SHOWTIMING == 1
            gLastTime = __clock_cycles();
#endif
        }

        bool all_done = false;
        do {
            xSemaphoreTake(gTX_sem, portMAX_DELAY);

            for (int i = 0; i < FASTLED_RMT_MAX_CHANNELS; i++) {
                if (gRefillChannel[i]) {
                    gOnChannel[i]->fillNext();
                    gRefillChannel[i] = false;
                }

                if (gDoneChannel[i]) {
                    doneOnChannel(rmt_channel_t(i), 0);
                    if (gNumDone == gNumControllers) {
                        all_done = true;
                    }
                    gDoneChannel[i] = false;
                }
            }

            xSemaphoreGive(gTX_sem);
        } while ( ! all_done);

        mWait.mark();

        // -- Reset the counters
        gNumStarted = 0;
        gNumDone = 0;
        gNext = 0;

#if FASTLED_ESP32_FLASH_LOCK == 1
        // -- Release the lock on flash operations
        spi_flash_op_unlock();
#endif
    }

#if FASTLED_ESP32_SHOWTIMING == 1
    for (int i = 0; i < gTimeIndex; i++) {
        if (gTiming[i] > 10000) {
            Serial.print(i);
            Serial.print(" ");
            Serial.println(gTiming[i]);
        }
    }
#endif
}

// -- Start up the next controller
//    This method is static so that it can dispatch to the
//    appropriate startOnChannel method of the given controller.
void ESP32RMTController::startNext(int channel)
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
void ESP32RMTController::startOnChannel(int channel)
{
    // -- Assign this channel and configure the RMT
    mRMT_channel = rmt_channel_t(channel);

    // -- Store a reference to this controller, so we can get it
    //    inside the interrupt handler
    gOnChannel[channel] = this;

    // -- Assign the pin to this channel
    rmt_set_pin(mRMT_channel, RMT_MODE_TX, mPin);

    if (FASTLED_RMT_BUILTIN_DRIVER) {
        // -- Use the built-in RMT driver to send all the data in one shot
        rmt_register_tx_end_callback(doneOnChannel, 0);
        rmt_write_items(mRMT_channel, mBuffer, mBufferSize, false);
    } else {
        // -- Use our custom driver to send the data incrementally

        // -- Initialize the counters that keep track of where we are in
        //    the pixel data.
        mRMT_mem_ptr = & (RMTMEM.chan[mRMT_channel].data32[0].val);
        mCur = 0;
        mWhichHalf = 0;

        // -- Store 2 pixels worth of data (two "buffers" full)
        fillNext();
        fillNext();

        // -- Turn on the interrupts
        rmt_set_tx_intr_en(mRMT_channel, true);
    }
}

// -- Start RMT transmission
//    Setting this RMT flag is what actually kicks off the peripheral
void ESP32RMTController::tx_start()
{
    // dev->conf_ch[channel].conf1.tx_start = 1;
    rmt_tx_start(mRMT_channel, true);
}

// -- A controller is done 
//    This function is called when a controller finishes writing
//    its data. It is called either by the custom interrupt
//    handler (below), or as a callback from the built-in
//    interrupt handler. It is static because we don't know which
//    controller is done until we look it up.
void ESP32RMTController::doneOnChannel(rmt_channel_t channel, void * arg)
{
    ESP32RMTController * pController = gOnChannel[channel];

    // -- Turn off output on the pin
    // SZG: Do I really need to do this?
    // gpio_matrix_out(pController->mPin, 0x100, 0, 0);

    gOnChannel[channel] = NULL;
    gNumDone++;

    if (gNumDone == gNumControllers) {
        // -- If this is the last controller, signal that we are all done
        if (FASTLED_RMT_BUILTIN_DRIVER) {
            xSemaphoreGive(gTX_sem);
        }
    } else {
        // -- Otherwise, if there are still controllers waiting, then
        //    start the next one on this channel
        if (gNext < gNumControllers) {
            startNext(channel);
            pController->tx_start();
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

#if FASTLED_ESP32_SHOWTIMING == 1
    uint32_t curt = __clock_cycles();
    gTiming[gTimeIndex++] = curt - gLastTime;
    gLastTime = curt;
#endif

    bool stuff_to_do = false;
    for (channel = 0; channel < FASTLED_RMT_MAX_CHANNELS; channel++) {
        int tx_done_bit = channel * 3;
        int tx_next_bit = channel + 24;

        ESP32RMTController * pController = gOnChannel[channel];
        if (pController != NULL) {

            // -- More to send on this channel
            if (intr_st & BIT(tx_next_bit)) {
                RMT.int_clr.val |= BIT(tx_next_bit);
                gRefillChannel[channel] = true;
                stuff_to_do = true;
            } else {
                // -- Transmission is complete on this channel
                if (intr_st & BIT(tx_done_bit)) {
                    RMT.int_clr.val |= BIT(tx_done_bit);
                    gDoneChannel[channel] = true;
                    stuff_to_do = true;
                }
            }
        }
    }

    if (stuff_to_do) {
        portBASE_TYPE HPTaskAwoken = 0;
        xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
        if (HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
    }
}

// -- Fill RMT buffer
//    Puts 32 bits of pixel data into the next 32 slots in the RMT memory
//    Each data bit is represented by a 32-bit RMT item that specifies how
//    long to hold the signal high, followed by how long to hold it low.
void IRAM_ATTR ESP32RMTController::fillNext()
{
    if (mCur < mSize) {
        // -- Get the zero and one values into local variables
        uint32_t one_val = mOne.val;
        uint32_t zero_val = mZero.val;

        // -- Fill 32 slots in the RMT memory
        uint8_t a = mPixelData[mCur++];
        uint8_t b = mPixelData[mCur++];
        uint8_t c = mPixelData[mCur++];
        uint8_t d = mPixelData[mCur++];
        register uint32_t pixeldata = a << 24 | b << 16 | c << 8 | d;

        // -- Use locals for speed
        volatile register uint32_t * pItem =  mRMT_mem_ptr;
            
        // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the 
        // rmt_item32_t value corresponding to the buffered bit value
        for (register uint32_t j = 0; j < PULSES_PER_FILL; j++) {
            *pItem++ = (pixeldata & 0x80000000L) ? one_val : zero_val;
            // Replaces: RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;

            pixeldata <<= 1;
        }

        // -- Flip to the other half, resetting the pointer if necessary
        mWhichHalf++;
        if (mWhichHalf == 2) {
            pItem = & (RMTMEM.chan[mRMT_channel].data32[0].val);
            mWhichHalf = 0;
        }

        // -- Store the new pointer back into the object
        mRMT_mem_ptr = pItem;
    } else {
        // -- No more data; signal to the RMT we are done
        for (uint32_t j = 0; j < PULSES_PER_FILL; j++) {
            * mRMT_mem_ptr++ = 0;
        }
    }
}

// -- Init pulse buffer
//    Set up the buffer that will hold all of the pulse items for this
//    controller. 
//    This function is only used when the built-in RMT driver is chosen
void ESP32RMTController::initPulseBuffer(int size_in_bytes)
{
    if (mBuffer == 0) {
        // -- Each byte has 8 bits, each bit needs a 32-bit RMT item
        int size = size_in_bytes * 8 * 4;

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
    for (register uint32_t j = 0; j < 8; j++) {
        mBuffer[mCurPulse] = (byteval & 0x80000000L) ? mOne : mZero;
        byteval <<= 1;
        mCurPulse++;
    }
}

#endif
