

#ifdef ESP32

#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32

#include "i2s_esp32dev.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "i2s_esp32dev.h"
#include "fl/namespace.h"
#include <stdint.h>
#include <string.h> // for memset

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// Patches the i2s driver for compatibility with ESP-IDF v5.0.
// This has only been compile tested. If there are issues then please file a bug.
#include "soc/gpio_periph.h"
#define gpio_matrix_out esp_rom_gpio_connect_out_signal
#endif



FASTLED_NAMESPACE_BEGIN


// -- I2S clock
#define I2S_BASE_CLK (80000000L)
#define I2S_MAX_CLK                                                            \
    (20000000L) // more tha a certain speed and the I2s looses some bits
#define I2S_MAX_PULSE_PER_BIT                                                  \
    20 // put it higher to get more accuracy but it could decrease the refresh
       // rate without real improvement
// -- Convert ESP32 cycles back into nanoseconds
#define ESPCLKS_TO_NS(_CLKS) (((long)(_CLKS) * 1000L) / F_CPU_MHZ)

static int gPulsesPerBit = 0;
static uint32_t gOneBit[40] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint32_t gZeroBit[40] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// -- Counters to track progress
static int ones_for_one;
static int ones_for_zero;

// -- Temp buffers for pixels and bits being formatted for DMA
uint8_t gPixelRow[NUM_COLOR_CHANNELS][32];
uint8_t gPixelBits[NUM_COLOR_CHANNELS][8][4];

static int CLOCK_DIVIDER_N;
static int CLOCK_DIVIDER_A;
static int CLOCK_DIVIDER_B;
// -- Interrupt handler
static intr_handle_t gI2S_intr_handle = NULL;

// -- I2S goes to these pins until we remap them using the GPIO matrix
static int i2s_base_pin_index;

// -- A pointer to the memory-mapped structure: I2S0 or I2S1
static i2s_dev_t *i2s;

int gCntBuffer = 0;

// -- Counters to track progress
int gCurBuffer = 0;
bool gDoneFilling = false;
void_func_t gCallback = nullptr;

void i2s_set_fill_buffer_callback(void_func_t callback) {
    gCallback = callback;
}

DMABuffer *dmaBuffers[NUM_DMA_BUFFERS];

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
#if tskKERNEL_VERSION_MAJOR >= 7
static SemaphoreHandle_t gTX_sem = NULL;
#else
static xSemaphoreHandle gTX_sem = NULL;
#endif

// -- One-time I2S initialization
static bool gInitialized = false;

static DMABuffer *allocateDMABuffer(int bytes) {
    DMABuffer *b =
        (DMABuffer *)heap_caps_malloc(sizeof(DMABuffer), MALLOC_CAP_DMA);

    b->buffer = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
    memset(b->buffer, 0, bytes);

    b->descriptor.length = bytes;
    b->descriptor.size = bytes;
    b->descriptor.owner = 1;
    b->descriptor.sosf = 1;
    b->descriptor.buf = b->buffer;
    b->descriptor.offset = 0;
    b->descriptor.empty = 0;
    b->descriptor.eof = 1;
    b->descriptor.qe.stqe_next = 0;

    return b;
}

int pgcd(int smallest, int precision, int a, int b, int c) {
    int pgc_ = 1;
    for (int i = smallest; i > 0; --i) {

        if (a % i <= precision && b % i <= precision && c % i <= precision) {
            pgc_ = i;
            break;
        }
    }
    return pgc_;
}

// -- Custom interrupt handler
static IRAM_ATTR void interruptHandler(void *arg) {
    if (i2s->int_st.out_eof) {
        i2s->int_clr.val = i2s->int_raw.val;
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
        gCntBuffer--;
#endif
        if (!gDoneFilling) {
            gCallback();
        } else {
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
            // release semaphore only if all DMA buffers have been sent
            if (gCntBuffer == 0)
#endif
            {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
                if (HPTaskAwoken == pdTRUE)
                    portYIELD_FROM_ISR();
            }
        }
    }
}

/** Transpose 8x8 bit matrix
 *  From Hacker's Delight
 */
static void transpose8rS32(uint8_t *A, int m, int n, uint8_t *B) {
    uint32_t x, y, t;

    // Load the array and pack it into x and y.

    x = (A[0] << 24) | (A[m] << 16) | (A[2 * m] << 8) | A[3 * m];
    y = (A[4 * m] << 24) | (A[5 * m] << 16) | (A[6 * m] << 8) | A[7 * m];

    t = (x ^ (x >> 7)) & 0x00AA00AA;
    x = x ^ t ^ (t << 7);
    t = (y ^ (y >> 7)) & 0x00AA00AA;
    y = y ^ t ^ (t << 7);

    t = (x ^ (x >> 14)) & 0x0000CCCC;
    x = x ^ t ^ (t << 14);
    t = (y ^ (y >> 14)) & 0x0000CCCC;
    y = y ^ t ^ (t << 14);

    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    B[0] = x >> 24;
    B[n] = x >> 16;
    B[2 * n] = x >> 8;
    B[3 * n] = x;
    B[4 * n] = y >> 24;
    B[5 * n] = y >> 16;
    B[6 * n] = y >> 8;
    B[7 * n] = y;
}

static void transpose32(uint8_t *pixels, uint8_t *bits) {
    transpose8rS32(&pixels[0], 1, 4, &bits[0]);
    transpose8rS32(&pixels[8], 1, 4, &bits[1]);
    transpose8rS32(&pixels[16], 1, 4, &bits[2]);
    // transpose8rS32(& pixels[24], 1, 4, & bits[3]);  Can only use 24 bits
}

void i2s_define_bit_patterns(int T1, int T2, int T3) {

    // -- First, convert back to ns from CPU clocks
    uint32_t T1ns = ESPCLKS_TO_NS(T1);
    uint32_t T2ns = ESPCLKS_TO_NS(T2);
    uint32_t T3ns = ESPCLKS_TO_NS(T3);

    /*
     We calculate the best pcgd to the timing
     ie
     WS2811 77 77 154 => 1  1 2 => nb pulses= 4
     WS2812 60 150 90 => 2 5 3 => nb pulses=10
     */
    int smallest = 0;
    if (T1 > T2)
        smallest = T2;
    else
        smallest = T1;
    if (smallest > T3)
        smallest = T3;
    double freq = (double)1 / (double)(T1ns + T2ns + T3ns);
    // Serial.printf("chipset frequency:%f Khz\n", 1000000L*freq);
    // Serial.printf("smallest %d\n",smallest);
    int pgc_ = 1;
    int precision = 0;
    pgc_ = pgcd(smallest, precision, T1, T2, T3);
    // Serial.printf("%f\n",I2S_MAX_CLK/(1000000000L*freq));
    while (
        pgc_ == 1 ||
        (T1 / pgc_ + T2 / pgc_ + T3 / pgc_) >
            I2S_MAX_PULSE_PER_BIT) // while(pgc_==1 ||  (T1/pgc_ +T2/pgc_
                                   // +T3/pgc_)>I2S_MAX_CLK/(1000000000L*freq))
    {
        ++precision;
        pgc_ = pgcd(smallest, precision, T1, T2, T3);
        // Serial.printf("%d %d\n",pgc_,(a+b+c)/pgc_);
    }
    pgc_ = pgcd(smallest, precision, T1, T2, T3);
    // Serial.printf("pgcd %d precision:%d\n",pgc_,precision);
    // Serial.printf("nb pulse per bit:%d\n",T1/pgc_ +T2/pgc_ +T3/pgc_);
    gPulsesPerBit = (int)T1 / pgc_ + (int)T2 / pgc_ + (int)T3 / pgc_;
    /*
     we calculate the duration of one pulse nd htre base frequency of the led
     ie WS2812B F=1/(250+625+375)=800kHz or 1250ns
     as we need 10 pulses each pulse is 125ns => frequency 800Khz*10=8MHz
     WS2811 T=320+320+641=1281ns qnd we need 4 pulses => pulse duration 320.25ns
     =>frequency 3.1225605Mhz

     */

    freq = 1000000000L * freq * gPulsesPerBit;
    // Serial.printf("needed frequency (nbpiulse per bit)*(chispset
    // frequency):%f Mhz\n",freq/1000000);

    /*
     we do calculate the needed N a and b
     as f=basefred/(N+b/a);
     as a is max 63 the precision for the decimal is 1/63

     */

    CLOCK_DIVIDER_N = (int)((double)I2S_BASE_CLK / freq);
    double v = I2S_BASE_CLK / freq - CLOCK_DIVIDER_N;

    double prec = (double)1 / 63;
    int a = 1;
    int b = 0;
    CLOCK_DIVIDER_A = 1;
    CLOCK_DIVIDER_B = 0;
    for (a = 1; a < 64; ++a) {
        for (b = 0; b < a; ++b) {
            // printf("%d %d %f %f
            // %f\n",b,a,v,(double)v*(double)a,fabsf(v-(double)b/a));
            if (fabsf(v - (double)b / a) <= prec / 2)
                break;
        }
        if (fabsf(v - (double)b / a) == 0) {
            CLOCK_DIVIDER_A = a;
            CLOCK_DIVIDER_B = b;
            break;
        }
        if (fabsf(v - (double)b / a) < prec / 2) {
            if (fabsf(v - (double)b / a) <
                fabsf(v - (double)CLOCK_DIVIDER_B / CLOCK_DIVIDER_A)) {
                CLOCK_DIVIDER_A = a;
                CLOCK_DIVIDER_B = b;
            }
        }
    }
    // top take care of an issue with double 0.9999999999
    if (CLOCK_DIVIDER_A == CLOCK_DIVIDER_B) {
        CLOCK_DIVIDER_A = 1;
        CLOCK_DIVIDER_B = 0;
        ++CLOCK_DIVIDER_N;
    }

    // printf("%d %d %f %f
    // %d\n",CLOCK_DIVIDER_B,CLOCK_DIVIDER_A,(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A,v,CLOCK_DIVIDER_N);
    // Serial.printf("freq %f
    // %f\n",freq,I2S_BASE_CLK/(CLOCK_DIVIDER_N+(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A));
    freq = 1 / (CLOCK_DIVIDER_N + (double)CLOCK_DIVIDER_B / CLOCK_DIVIDER_A);
    freq = freq * I2S_BASE_CLK;
    // Serial.printf("calculted for i2s frequency:%f Mhz N:%d B:%d
    // A:%d\n",freq/1000000,CLOCK_DIVIDER_N,CLOCK_DIVIDER_B,CLOCK_DIVIDER_A);
    // double pulseduration=1000000000/freq;
    // Serial.printf("Pulse duration: %f ns\n",pulseduration);
    // gPulsesPerBit = (T1ns + T2ns + T3ns)/FASTLED_I2S_NS_PER_PULSE;

    // Serial.print("Pulses per bit: "); Serial.println(gPulsesPerBit);

    // int ones_for_one  = ((T1ns + T2ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
    ones_for_one = T1 / pgc_ + T2 / pgc_;
    // Serial.print("One bit:  target ");
    // Serial.print(T1ns+T2ns); Serial.print("ns --- ");
    // Serial.print(ones_for_one); Serial.print(" 1 bits");
    // Serial.print(" = "); Serial.print(ones_for_one *
    // FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
    //  Serial.printf("one bit : target %d  ns --- %d  pulses 1 bit = %f
    //  ns\n",T1ns+T2ns,ones_for_one ,ones_for_one*pulseduration);

    int i = 0;
    while (i < ones_for_one) {
        gOneBit[i] = 0xFFFFFF00;
        ++i;
    }
    while (i < gPulsesPerBit) {
        gOneBit[i] = 0x00000000;
        ++i;
    }

    // int ones_for_zero = ((T1ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
    ones_for_zero = T1 / pgc_;
    // Serial.print("Zero bit:  target ");
    // Serial.print(T1ns); Serial.print("ns --- ");
    // Serial.print(ones_for_zero); Serial.print(" 1 bits");
    // Serial.print(" = "); Serial.print(ones_for_zero *
    // FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
    // Serial.printf("Zero bit : target %d ns --- %d pulses  1 bit =   %f
    // ns\n",T1ns,ones_for_zero ,ones_for_zero*pulseduration);
    i = 0;
    while (i < ones_for_zero) {
        gZeroBit[i] = 0xFFFFFF00;
        ++i;
    }
    while (i < gPulsesPerBit) {
        gZeroBit[i] = 0x00000000;
        ++i;
    }

    memset(gPixelRow, 0, NUM_COLOR_CHANNELS * 32);
    memset(gPixelBits, 0, NUM_COLOR_CHANNELS * 32);
}

bool i2s_is_initialized() { return gInitialized; }

void i2s_init(int i2s_device) {

    // -- Choose whether to use I2S device 0 or device 1
    //    Set up the various device-specific parameters
    int interruptSource;
    if (i2s_device == 0) {
        i2s = &I2S0;
        periph_module_enable(PERIPH_I2S0_MODULE);
        interruptSource = ETS_I2S0_INTR_SOURCE;
        i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
    } else {
        i2s = &I2S1;
        periph_module_enable(PERIPH_I2S1_MODULE);
        interruptSource = ETS_I2S1_INTR_SOURCE;
        i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
    }

    // -- Reset everything
    i2s_reset();
    i2s_reset_dma();
    i2s_reset_fifo();

    // -- Main configuration
    i2s->conf.tx_msb_right = 1;
    i2s->conf.tx_mono = 0;
    i2s->conf.tx_short_sync = 0;
    i2s->conf.tx_msb_shift = 0;
    i2s->conf.tx_right_first = 1; // 0;//1;
    i2s->conf.tx_slave_mod = 0;

    // -- Set parallel mode
    i2s->conf2.val = 0;
    i2s->conf2.lcd_en = 1;
    i2s->conf2.lcd_tx_wrx2_en = 0; // 0 for 16 or 32 parallel output
    i2s->conf2.lcd_tx_sdx2_en = 0; // HN

    // -- Set up the clock rate and sampling
    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 32; // Number of parallel bits/pins
    i2s->sample_rate_conf.tx_bck_div_num = 1;
    i2s->clkm_conf.val = 0;
    i2s->clkm_conf.clka_en = 0;

    // -- Data clock is computed as Base/(div_num + (div_b/div_a))
    //    Base is 80Mhz, so 80/(10 + 0/1) = 8Mhz
    //    One cycle is 125ns
    i2s->clkm_conf.clkm_div_a = CLOCK_DIVIDER_A;
    i2s->clkm_conf.clkm_div_b = CLOCK_DIVIDER_B;
    i2s->clkm_conf.clkm_div_num = CLOCK_DIVIDER_N;

    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 3;  // 32-bit single channel data
    i2s->fifo_conf.tx_data_num = 32; // fifo length
    i2s->fifo_conf.dscr_en = 1;      // fifo will use dma

    i2s->conf1.val = 0;
    i2s->conf1.tx_stop_en = 0;
    i2s->conf1.tx_pcm_bypass = 1;

    i2s->conf_chan.val = 0;
    i2s->conf_chan.tx_chan_mod =
        1; // Mono mode, with tx_msb_right = 1, everything goes to right-channel

    i2s->timing.val = 0;

    // -- Allocate DMA buffers
    for (int i = 0; i < NUM_DMA_BUFFERS; i++) {
        dmaBuffers[i] =
            allocateDMABuffer(32 * NUM_COLOR_CHANNELS * gPulsesPerBit);
    }
    // -- Arrange them as a circularly linked list
    for (int i = 0; i < NUM_DMA_BUFFERS; i++) {
        dmaBuffers[i]->descriptor.qe.stqe_next =
            &(dmaBuffers[(i + 1) % NUM_DMA_BUFFERS]->descriptor);
    }

    // -- Allocate i2s interrupt
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(i2s_device), I2S_OUT_EOF_INT_ENA_V, 1,
                      I2S_OUT_EOF_INT_ENA_S);
    esp_intr_alloc(interruptSource,
                   0, // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3,
                   &interruptHandler, 0, &gI2S_intr_handle);

    // -- Create a semaphore to block execution until all the controllers are
    // done
    if (gTX_sem == NULL) {
        gTX_sem = xSemaphoreCreateBinary();
        xSemaphoreGive(gTX_sem);
    }

    // Serial.println("Init I2S");
    gInitialized = true;
}

void i2s_clear_dma_buffer(uint32_t *buf) {
    for (int i = 0; i < 8 * NUM_COLOR_CHANNELS; ++i) {
        int offset = gPulsesPerBit * i;
        for (int j = 0; j < ones_for_zero; ++j)
            buf[offset + j] = 0xffffffff;

        for (int j = ones_for_one; j < gPulsesPerBit; ++j)
            buf[offset + j] = 0;
    }
}

void i2s_start() {
    // esp_intr_disable(gI2S_intr_handle);
    // Serial.println("I2S start");
    i2s_reset();
    // Serial.println(dmaBuffers[0]->sampleCount());
    i2s->lc_conf.val =
        I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
    i2s->out_link.addr = (uint32_t) & (dmaBuffers[0]->descriptor);
    i2s->out_link.start = 1;
    ////vTaskDelay(5);
    i2s->int_clr.val = i2s->int_raw.val;
    // //vTaskDelay(5);
    i2s->int_ena.out_dscr_err = 1;
    // enable interrupt
    ////vTaskDelay(5);
    esp_intr_enable(gI2S_intr_handle);
    // //vTaskDelay(5);
    i2s->int_ena.val = 0;
    i2s->int_ena.out_eof = 1;

    // start transmission
    i2s->conf.tx_start = 1;
}

void i2s_reset() {
    // Serial.println("I2S reset");
    const unsigned long lc_conf_reset_flags =
        I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
    i2s->lc_conf.val |= lc_conf_reset_flags;
    i2s->lc_conf.val &= ~lc_conf_reset_flags;

    const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M |
                                      I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    i2s->conf.val |= conf_reset_flags;
    i2s->conf.val &= ~conf_reset_flags;
}

void i2s_reset_dma() {
    i2s->lc_conf.in_rst = 1;
    i2s->lc_conf.in_rst = 0;
    i2s->lc_conf.out_rst = 1;
    i2s->lc_conf.out_rst = 0;
}

void i2s_reset_fifo() {
    i2s->conf.rx_fifo_reset = 1;
    i2s->conf.rx_fifo_reset = 0;
    i2s->conf.tx_fifo_reset = 1;
    i2s->conf.tx_fifo_reset = 0;
}

void i2s_stop() {
    esp_intr_disable(gI2S_intr_handle);
    i2s_reset();
    i2s->conf.rx_start = 0;
    i2s->conf.tx_start = 0;
}

void i2s_begin() { xSemaphoreTake(gTX_sem, portMAX_DELAY); }

void i2s_wait() {
    // -- Wait here while the rest of the data is sent. The interrupt handler
    //    will keep refilling the DMA buffers until it is all sent; then it
    //    gives the semaphore back.
    xSemaphoreTake(gTX_sem, portMAX_DELAY);
    xSemaphoreGive(gTX_sem);
}

void i2s_setup_pin(int _pin, int offset) {
    gpio_num_t pin = (gpio_num_t)_pin;
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
    gpio_set_direction(pin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
    pinMode(pin, OUTPUT);
    gpio_matrix_out(pin, i2s_base_pin_index + offset, false, false);
}

void i2s_transpose_and_encode(int channel, uint32_t has_data_mask,
                              volatile uint32_t *buf) {

    // -- Tranpose each array: all the bit 7's, then all the bit 6's,
    // ...
    transpose32(gPixelRow[channel], gPixelBits[channel][0]);

    // Serial.print("Channel: "); Serial.print(channel); Serial.print("
    // ");
    for (int bitnum = 0; bitnum < 8; ++bitnum) {
        uint8_t *row = (uint8_t *)(gPixelBits[channel][bitnum]);
        uint32_t bit = (row[0] << 24) | (row[1] << 16) | (row[2] << 8) | row[3];

        /* SZG: More general, but too slow:
             for (int pulse_num = 0; pulse_num < gPulsesPerBit;
           ++pulse_num) { buf[buf_index++] = has_data_mask & ( (bit &
           gOneBit[pulse_num]) | (~bit & gZeroBit[pulse_num]) );
              }
        */

        // -- Only fill in the pulses that are different between the "0"
        // and "1" encodings
        for (int pulse_num = ones_for_zero; pulse_num < ones_for_one;
             ++pulse_num) {
            buf[bitnum * gPulsesPerBit + channel * 8 * gPulsesPerBit +
                pulse_num] = has_data_mask & bit;
        }
    }
}

FASTLED_NAMESPACE_END

#endif // ifdef CONFIG_IDF_TARGET_ESP32

#endif // ifdef ESP32