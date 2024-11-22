#pragma once

#include "env.h"

#include "driver.h"

namespace fl {

static void IRAM_ATTR i2sStop(I2SClocklessVirtualLedDriver *cont) {
#ifdef CONFIG_IDF_TARGET_ESP32S3

    // gdma_disconnect(dma_chan);
    LCD_CAM.lcd_user.lcd_start = 0;

    while (LCD_CAM.lcd_user.lcd_start) {
    }
    gdma_stop(dma_chan);
    // esp_intr_disable(dma_chan->intr);
#else

    // delay(1);
    esp_intr_disable(cont->_gI2SClocklessDriver_intr_handle);
    ets_delay_us(16);
    (&I2S0)->conf.tx_start = 0;
    while ((&I2S0)->conf.tx_start == 1) {
    }
#endif
    // cont->i2sReset();
    i2sReset();
    cont->isDisplaying = false;
    // cont->leds=cont->saveleds;
    /*
     We have finished to display the strips
     */
    // ets_delay_us(1000);
    if (cont->wasWaitingtofinish == true) // and cont->__displayMode==NO_WAIT
    {
        cont->wasWaitingtofinish = false;
        xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_waitDisp);
    }
    if (cont->isWaiting) {
        // printf("on debloqu\n");
        xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_sem);
    }
    // printf("hehe\n");
}
#ifdef CONFIG_IDF_TARGET_ESP32S3
static IRAM_ATTR bool
_I2SClocklessVirtualLedDriverinterruptHandler(gdma_channel_handle_t dma_chan,
                                              gdma_event_data_t *event_data,
                                              void *user_data) {
    // This DMA callback seems to trigger a moment before the last data has
    // issued (buffering between DMA & LCD peripheral?), so pause a moment
    // before stopping LCD data out. The ideal delay may depend on the LCD
    // clock rate...this one was determined empirically by monitoring on a
    // logic analyzer. YMMV.
    // vTaskDelay(100);
    // The LCD peripheral stops transmitting at the end of the DMA xfer, but
    // clear the lcd_start flag anyway -- we poll it in loop() to decide when
    // the transfer has finished, and the same flag is set later to trigger
    // the next transfer.
    I2SClocklessVirtualLedDriver *cont =
        (I2SClocklessVirtualLedDriver *)user_data;

    if (!cont->__enableDriver) {
        // cont->i2sStop(cont);
        i2sStop(cont);
        return true;
    }

    cont->framesync = !cont->framesync;

    // cont->ledToDisplay_in[cont->ledToDisplay_out]=cont->ledToDisplay+1;
    // cont->ledToDisplay_inbuffer[cont->ledToDisplay_out]=cont->dmaBufferActive;

    if (cont->transpose) {
        cont->ledToDisplay = cont->ledToDisplay + 1;
        if (cont->ledToDisplay < cont->num_led_per_strip) {

            loadAndTranspose(cont);

            if (cont->ledToDisplay_out ==
                (cont->num_led_per_strip -
                 (__NB_DMA_BUFFER))) // here it's not -1 because it takes time
                                     // top have the change into account and it
                                     // reread the buufer
            {
                cont->DMABuffersTampon[(cont->dmaBufferActive) %
                                       __NB_DMA_BUFFER]
                    ->next = (cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]);
                // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=cont->dmaBufferActive;
            }

            cont->dmaBufferActive =
                (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
        }
        cont->ledToDisplay_out = cont->ledToDisplay_out + 1;
        if (cont->ledToDisplay >= NUM_LEDS_PER_STRIP + __NB_DMA_BUFFER - 1) {

            i2sStop(cont);
        }
    } else {
        if (cont->framesync) {
            portBASE_TYPE HPTaskAwoken = 0;
            xSemaphoreGiveFromISR(cont->I2SClocklessVirtualLedDriver_semSync,
                                  &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE)
                portYIELD_FROM_ISR();
        }
    }
    return true;
}
#else
static void IRAM_ATTR _I2SClocklessVirtualLedDriverinterruptHandler(void *arg) {

    // REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG( 0 )) &
    // 0xffffffc0) | 0x3f); return;
    I2SClocklessVirtualLedDriver *cont = (I2SClocklessVirtualLedDriver *)arg;

    if (!cont->__enableDriver) {
        REG_WRITE(I2S_INT_CLR_REG(0),
                  (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
        i2sStop(cont);

        return;
    }
    if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_S,
                          I2S_OUT_EOF_INT_ST_S)) {
        cont->framesync = !cont->framesync;

        // cont->ledToDisplay_in[cont->ledToDisplay_out]=cont->ledToDisplay+1;
        // cont->ledToDisplay_inbuffer[cont->ledToDisplay_out]=cont->dmaBufferActive;

        if (((I2SClocklessVirtualLedDriver *)arg)->transpose) {
            cont->ledToDisplay++;
            if (cont->ledToDisplay < cont->num_led_per_strip) {

                loadAndTranspose(cont);

                if (cont->ledToDisplay_out ==
                    (cont->num_led_per_strip -
                     (__NB_DMA_BUFFER))) // here it's not -1 because it takes
                                         // time top have the change into
                                         // account and it reread the buufer
                {
                    cont->DMABuffersTampon[(cont->dmaBufferActive) %
                                           __NB_DMA_BUFFER]
                        ->descriptor.qe.stqe_next =
                        &(cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]
                              ->descriptor);
                    // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=cont->dmaBufferActive;
                }

                cont->dmaBufferActive =
                    (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
            }
            cont->ledToDisplay_out++;
        } else {
            if (cont->framesync) {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(
                    cont->I2SClocklessVirtualLedDriver_semSync, &HPTaskAwoken);
                if (HPTaskAwoken == pdTRUE)
                    portYIELD_FROM_ISR();
            }
        }
    }

    if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE),
                          I2S_OUT_TOTAL_EOF_INT_ST_S,
                          I2S_OUT_TOTAL_EOF_INT_ST_S)) {

        // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=9999;
        i2sStop(cont);
    }

    REG_WRITE(I2S_INT_CLR_REG(0),
              (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
}
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION) == 0
static inline __attribute__((always_inline)) void IRAM_ATTR
loadAndTranspose(I2SClocklessVirtualLedDriver *driver) {

#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount();
#endif
    uint8_t *ledt = driver->leds;
    uint16_t *buff =
        (uint16_t *)driver->DMABuffersTampon[driver->dmaBufferActive]->buffer;
#if ((I2S_MAPPING_MODE & 0xFFF) ==                                             \
     I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or              \
    ((I2S_MAPPING_MODE & 0xFFF) ==                                             \
     I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE) or                              \
    ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_NONE)
    int ledtodisp = driver->ledToDisplay;
#endif
#ifndef __HARDWARE_BRIGHTNESS
    uint8_t *mapg = driver->__green_map;
    uint8_t *mapr = driver->__red_map;
    uint8_t *mapb = driver->__blue_map;
#endif
#ifdef _USE_PALETTE
    uint8_t *palette = driver->palette;
#endif
#if nb_components > 3
    uint8_t *mapw = driver->__white_map;
#endif
#if STATICCOLOR == 0
    uint8_t *r_map = driver->r_map;
    uint8_t *g_map = driver->g_map;
    uint8_t *b_map = driver->b_map;
#endif
    Lines *firstPixel = driver->firstPixel;

    uint8_t *poli_b, *poli;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
    uint8_t *_poli;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0

    _poli = ledt + ledtodisp * _palette_size;
#else

#if ((I2S_MAPPING_MODE & 0xFFF) ==                                             \
     I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or              \
    ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
    uint16_t _led_tmp = ledtodisp;
    // #if !((I2S_MAPPING_MODE &
    // I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0)
    uint16_t led_tmp;
    // #endif
#endif

#endif
    buff += OFFSET;
    // jump en deux

    //.for()

    for (int pin74HC595 = 0; pin74HC595 < 8; pin74HC595++) {

#if ((I2S_MAPPING_MODE & 0xFFF) ==                                             \
     I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or              \
    ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
        led_tmp = _led_tmp;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
        poli = _poli;
#endif
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY |           \
                         I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) >  \
    0
#ifdef CONFIG_IDF_TARGET_ESP32S3
        int pin = (pin74HC595 ^ 1) << 4;
#else
        int pin = (pin74HC595) << 4;
#endif
#else
#ifdef CONFIG_IDF_TARGET_ESP32S3
        int pin = (pin74HC595) << 4;
#else
        int pin = (pin74HC595 ^ 1) << 4;
#endif
#endif
        for (int vpin = 0; vpin < NBIS2SERIALPINS; vpin++) {
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE
            poli = ledt + driver->mapLed(led_tmp) * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE
            poli = ledt + driver->remapStatic() * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif

#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE
            uint16_t val;
            driver->_hmapoff = &val;
            val = driver->mapLed(led_tmp);
            poli = ledt + driver->remapStatic() * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif

#ifdef _USE_PALETTE
            poli_b = palette + *(poli)*nb_components;
#else
            poli_b = poli;
#endif

#if STATICCOLOR == 1
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[p_g].bytes[pin + vpin] = mapg[*(poli_b + 1)];
            firstPixel[p_r].bytes[pin + vpin] = mapr[*(poli_b)];
            firstPixel[p_b].bytes[pin + vpin] = mapb[*(poli_b + 2)];
#else

            firstPixel[p_g].bytes[pin + vpin] = *(poli_b + 1);
            firstPixel[p_r].bytes[pin + vpin] = *(poli_b);
            firstPixel[p_b].bytes[pin + vpin] = *(poli_b + 2);

#endif

#else
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[g_map[8 * vpin]].bytes[pin + vpin] = mapg[*(poli_b + 1)];
            firstPixel[r_map[8 * vpin]].bytes[pin + vpin] = mapr[*(poli_b)];
            firstPixel[b_map[8 * vpin]].bytes[pin + vpin] = mapb[*(poli_b + 2)];
#else
            firstPixel[g_map[8 * vpin]].bytes[pin + vpin] = *(poli_b + 1);
            firstPixel[r_map[8 * vpin]].bytes[pin + vpin] = *(poli_b);
            firstPixel[b_map[8 * vpin]].bytes[pin + vpin] = *(poli_b + 2);
#endif
#endif
#if nb_components > 3
            firstPixel[3].bytes[pin + vpin] = mapw[*(poli_b + 3)];
#endif

            // pin++;
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE
            led_tmp += I2S_OFF_MAP;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE
            led_tmp += I2S_OFF_MAP;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) ==                                              \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY
            driver->_hmapoff++;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
            poli += I2S_OFF;
#endif
        }
#if ((I2S_MAPPING_MODE & 0xFFF) ==                                             \
     I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or              \
    ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
        _led_tmp += NUM_LEDS_PER_STRIP;
#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
        _poli += NUM_LEDS_PER_STRIP * _palette_size;
#endif
    }

    transpose16x1_noinline2(firstPixel[0].bytes, (uint8_t *)(buff));

    transpose16x1_noinline2(firstPixel[1].bytes, (uint8_t *)(buff + 192));
    transpose16x1_noinline2(firstPixel[2].bytes, (uint8_t *)(buff + 384));
#if nb_components > 3
    transpose16x1_noinline2(firstPixel[3].bytes, (uint8_t *)(buff + 576));
#endif

#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] =
        ESP.getCycleCount() - driver->_times[driver->ledToDisplay];
#endif
}

#else // function instead of reading memory
static inline __attribute__((always_inline)) void IRAM_ATTR
loadAndTranspose(I2SClocklessVirtualLedDriver *driver) {
#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount();
#endif
    // int led_tmp;
    // uint8_t *ledt = driver->leds;
    uint16_t *buff =
        (uint16_t *)driver->DMABuffersTampon[driver->dmaBufferActive]->buffer;
    int ledtodisp = driver->ledToDisplay;
#ifndef __HARDWARE_BRIGHTNESS
    uint8_t *mapg = driver->__green_map;
    uint8_t *mapr = driver->__red_map;
    uint8_t *mapb = driver->__blue_map;
#endif
    Lines *firstPixel = driver->firstPixel;
#if STATICCOLOR == 0
    uint8_t *r_map = driver->r_map;
    uint8_t *g_map = driver->g_map;
    uint8_t *b_map = driver->b_map;
#endif
#ifdef _USE_PALETTE
    uint8_t *palette = driver->palette;
#endif
    buff += OFFSET;

    for (int pin74HC595 = 0; pin74HC595 < 8; pin74HC595++) {

        int pin = (pin74HC595 ^ 1) << 4;
        int vPin = pin74HC595 << 4;
        for (int pinEsp32 = 0; pinEsp32 < NBIS2SERIALPINS; pinEsp32++) {
#ifdef _USE_PALETTE
            Pixel p = *(Pixel *)(palette +
                                 driver->pixelCalc(ledtodisp, pinEsp32, vPin) *
                                     nb_components);
#else
            Pixel p = driver->pixelCalc(ledtodisp, pinEsp32, vPin);
#endif
#if STATICCOLOR == 1
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[p_g].bytes[pin + pinEsp32] = mapg[p.raw[1]];
            firstPixel[p_r].bytes[pin + pinEsp32] = mapr[p.raw[0]];
            firstPixel[p_b].bytes[pin + pinEsp32] = mapb[p.raw[2]];
#else
            firstPixel[p_g].bytes[pin + pinEsp32] = p.raw[1];
            firstPixel[p_r].bytes[pin + pinEsp32] = p.raw[0];
            firstPixel[p_b].bytes[pin + pinEsp32] = p.raw[2];
#endif

#else
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[g_map[8 * pinEsp32]].bytes[pin + pinEsp32] =
                mapg[p.raw[1]];
            firstPixel[r_map[8 * pinEsp32]].bytes[pin + pinEsp32] =
                mapr[p.raw[0]];
            firstPixel[b_map[8 * pinEsp32]].bytes[pin + pinEsp32] =
                mapb[p.raw[2]];
#else
            firstPixel[g_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[1];
            firstPixel[r_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[0];
            firstPixel[b_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[2];
#endif
#endif
        }
    }

    transpose16x1_noinline2(firstPixel[0].bytes, (uint8_t *)(buff));

    transpose16x1_noinline2(firstPixel[1].bytes, (uint8_t *)(buff + 192));
    transpose16x1_noinline2(firstPixel[2].bytes, (uint8_t *)(buff + 384));
#if nb_components > 3
    transpose16x1_noinline2(firstPixel[3].bytes, (uint8_t *)(buff + 576));
#endif
#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] =
        ESP.getCycleCount() - driver->_times[driver->ledToDisplay];
#endif
}

#endif

static void showPixelsTask(void *pvParameters) {
    I2SClocklessVirtualLedDriver *cont =
        (I2SClocklessVirtualLedDriver *)pvParameters;
#ifndef CONFIG_IDF_TARGET_ESP32S3

    if (cont->_gI2SClocklessDriver_intr_handle != NULL) {
        esp_intr_free(cont->_gI2SClocklessDriver_intr_handle);
    }
    ESP_LOGV(TAG, "setting interupt handler");
    esp_err_t e = esp_intr_alloc(interruptSource,
                                 ESP_INTR_FLAG_INTRDISABLED |
                                     ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM,
                                 &_I2SClocklessVirtualLedDriverinterruptHandler,
                                 cont, &cont->_gI2SClocklessDriver_intr_handle);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Impossible to create interupt allocation");
        return;
    }
    ESP_LOGV(TAG, "interupt handler set on core %d", xPortGetCoreID());

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        cont->___showPixels();
    }
#else
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        cont->___showPixels();
    }
#endif
}

} // namespace fl