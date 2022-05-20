#ifndef __INC_CLOCKLESS_ARM_D21_DMA_H
#define __INC_CLOCKLESS_ARM_D21_DMA_H

// This is a quick port of Adafruit_NeoPixel_ZeroDMA but to work with FastLED.
// Using DMA allows for interrupts to be used without any flickering issues.
// DMA uses a lot more memory explained below.
// For more information see https://github.com/adafruit/Adafruit_NeoPixel_ZeroDMA

#include "Adafruit_ZeroDMA.h"
#include "SPI.h"
#include "bittable.h"       // Optional, see comments in show()
#include "dma_pins.h"       // SPI DMA capable DATA_PIN tables (per device)
#include "wiring_private.h" // pinPeripheral() function

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1


template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {

    // typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
    // typedef typename FastPinBB<DATA_PIN>::port_t data_t;

    // data_t mPinMask;
    // data_ptr_t mPort;
    // CMinWait<WAIT_TIME> mWait;
public:
    Adafruit_ZeroDMA dma; ///< The DMA manager for the SPI class
    SPIClass *spi;        ///< Underlying SPI hardware interface we use to DMA
    uint8_t *dmaBuf;      ///< The raw buffer we write to SPI to mimic NeoPixel
    bool isRunning;

    virtual void init() {
        isRunning = false;
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
        if(!isRunning) {
            // initialize the DMA
            uint8_t i;
            for (i = 0; (i < N_SERCOMS) && (sercomTable[i].mosi != DATA_PIN); i++)
                ;
            if (i >= N_SERCOMS)
            {
                return;
            }
            begin(sercomTable[i].sercom, sercomTable[i].sercomBase,
                         sercomTable[i].dmacID, sercomTable[i].mosi, sercomTable[i].padTX,
                         sercomTable[i].pinFunc, pixels);
            isRunning = true;
        }
        showRGBInternal(pixels);
    }

    // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
    // gcc will use register Y for the this pointer.
    void showRGBInternal(PixelController<RGB_ORDER> &pixels)
    {
        if(!isRunning) {
            return;
        }
        uint8_t c;
        uint8_t *out = dmaBuf;
        uint32_t expanded;
        while (pixels.has(1))
        {
            pixels.stepDithering();
            c = pixels.loadAndScale0();
            expanded = bitExpand[c];
            *out++ = expanded >> 16; // Shifting 32-bit table entry is
            *out++ = expanded >> 8;  // about 11% faster than copying
            *out++ = expanded;       // three values from a uint8_t table.

            c = pixels.loadAndScale1();
            expanded = bitExpand[c];
            *out++ = expanded >> 16; // Shifting 32-bit table entry is
            *out++ = expanded >> 8;  // about 11% faster than copying
            *out++ = expanded;       // three values from a uint8_t table.

            c = pixels.loadAndScale2();
            expanded = bitExpand[c];
            *out++ = expanded >> 16; // Shifting 32-bit table entry is
            *out++ = expanded >> 8;  // about 11% faster than copying
            *out++ = expanded;       // three values from a uint8_t table.
            
            pixels.advanceData();
        }
    }

    /** @brief Initialize the underlying SPI SERCOM for DMA transfers
        @param sercom Pointer to the underlying SERCOM from the Arduino core
        @param sercomBase the 'raw' Sercom register base address
        @param dmacID the DMAC id that matches the TX for the sercom (check DS)
        @param mosi The MOSI DATA_PIN (where we send data to the neopixel)
        @param padTX the pinmux set up for SPI SERCOM DATA_PIN config
        @param pinFunc The pinmux setup for which 'type' of pinmux we use
        @returns True or false on success
    */
    bool begin(SERCOM *sercom, Sercom *sercomBase,
                      uint8_t dmacID, uint8_t mosi,
                      SercomSpiTXPad padTX, EPioType pinFunc,
                      PixelController<RGB_ORDER> &pixels)
    {

        if (mosi != DATA_PIN)
            return false; // Invalid DATA_PIN

        // TO DO: Check for successful malloc in base class here

        // DMA buffer is 3X the NeoPixel buffer size.  Each bit is expanded
        // 3:1 to allow use of SPI peripheral to generate NeoPixel-like timing
        // (0b100 for a zero bit, 0b110 for a one bit).  SPI is clocked at
        // 2.4 MHz, the 3:1 sizing then creates NeoPixel-like 800 KHz bitrate.
        // The extra 90 bytes is the low-level latch at the end of the NeoPixel
        // data stream.  When idle, SPI logic level is normally HIGH, we need
        // LOW for latch.  There is no invert option.  Various tricks like
        // switching the DATA_PIN to a normal LOW output at end of data don't quite
        // work, there's still small glitches.  So, solution here is to keep
        // the SPI DMA transfer in an endless loop...it actually issues the
        // NeoPixel data over and over again forever (this doesn't cost us
        // anything, since it's 100% DMA, no CPU use)...and those 90 zero
        // bytes at the end provide the 300 microsecond EOD latch.  Hack!

        uint8_t bytesPerPixel = 3;
        uint32_t bytesTotal = (pixels.size() * bytesPerPixel * 8 * 3 + 7) / 8 + 90;
        if ((dmaBuf = (uint8_t *)malloc(bytesTotal)))
        {
            spi = NULL; // No SPIClass assigned yet,
                        // check MOSI DATA_PIN against existing defined SPI SERCOMs...
#if SPI_INTERFACES_COUNT > 0
            if (DATA_PIN == PIN_SPI_MOSI)
            {               // If NeoPixel DATA_PIN is main SPI MOSI...
                spi = &SPI; // Use the existing SPIClass object
                padTX = PAD_SPI_TX;
            }
#endif
#if SPI_INTERFACES_COUNT > 1
            else if (DATA_PIN == PIN_SPI1_MOSI)
            {                // If NeoPixel DATA_PIN = secondary SPI MOSI...
                spi = &SPI1; // Use the SPI1 SPIClass object
                padTX = PAD_SPI1_TX;
            }
#endif
#if SPI_INTERFACES_COUNT > 2
            else if (DATA_PIN == PIN_SPI2_MOSI)
            { // Ditto, tertiary SPI
                spi = &SPI2;
                padTX = PAD_SPI2_TX;
            }
#endif
#if SPI_INTERFACES_COUNT > 3
            else if (DATA_PIN == PIN_SPI3_MOSI)
            {
                spi = &SPI3;
                padTX = PAD_SPI3_TX;
            }
#endif
#if SPI_INTERFACES_COUNT > 4
            else if (DATA_PIN == PIN_SPI4_MOSI)
            {
                spi = &SPI4;
                padTX = PAD_SPI4_TX;
            }
#endif
#if SPI_INTERFACES_COUNT > 5
            else if (DATA_PIN == PIN_SPI5_MOSI)
            {
                spi = &SPI5;
                padTX = PAD_SPI5_TX;
            }
#endif
            // If NeoPixel DATA_PIN is not an existing SPI SERCOM, allocate a new one.
            if (spi == NULL)
            {
                // DIRTY POOL! The SPIClass constructor expects MISO, SCK and MOSI
                // pins, in that order. Our library only intends to ever use the MOSI
                // output, the others are never even set to SERCOM periph functions.
                // We just give the SPI constructor THE SAME PIN NUMBER for all three.
                // The SPI lib never checks if they're distinct and valid for each of
                // the three. It does set pinPeripheral for each (or in this case,
                // the same for the MOSI DATA_PIN three times)...but no matter, we set our
                // own pinPeripheral below. The SPI RX PAD also doesn't matter...we
                // always claim it's PAD 1 here, because (by hardware design) the TX
                // pad will always be 0, 2 or 3...this might collide with the SCK PAD
                // value, but we don't care, neither SCK nor MISO is actually used.
                // (This is tested across many SAMD devices and works, but it's
                // conceivable that this could fail spectacularly on some unforseen
                // future device, if the SERCOM pad assignment becomes hardwarily
                // strict.)
                spi = new SPIClass(sercom, mosi, mosi, mosi, padTX, SERCOM_RX_PAD_1);
            }
            if ((spi))
            {
                spi->begin();
                pinPeripheral(mosi, pinFunc);
                dma.setTrigger(dmacID);
                dma.setAction(DMA_TRIGGER_ACTON_BEAT);
                if (DMA_STATUS_OK == dma.allocate())
                {
                    if (dma.addDescriptor(dmaBuf,                              // move data from here
                                        (void *)(&sercomBase->SPI.DATA.reg), // to here
                                        bytesTotal,                          // this many...
                                        DMA_BEAT_SIZE_BYTE,                  // bytes/hword/words
                                        true,                                // increment source addr?
                                        false))
                    {                                  // increment dest addr?
                        dma.loop(true);                // DMA transaction loops forever! Latch is built in.
                        memset(dmaBuf, 0, bytesTotal); // IMPORTANT - clears latch data @ end
                        // SPI transaction is started BUT NEVER ENDS.  This is important.
                        // 800 khz * 3 = 2.4MHz
                        spi->beginTransaction(SPISettings(2400000, MSBFIRST, SPI_MODE0)); // 2400000
                        if (DMA_STATUS_OK == dma.startJob())
                            return true; // SUCCESS
                        // Else various errors, clean up partially-initialized stuff:
                        spi->endTransaction();
                    }
                    dma.free();
                }
                // Delete SPIClass object, UNLESS it's an existing (Arduino-defined) one
#if SPI_INTERFACES_COUNT > 0
                if (spi == &SPI)
                {
                    spi = NULL;
                }
#endif
#if SPI_INTERFACES_COUNT > 1
                else if (spi == &SPI1)
                {
                    spi = NULL;
                }
#endif
#if SPI_INTERFACES_COUNT > 2
                else if (spi == &SPI2)
                {
                    spi = NULL;
                }
#endif
#if SPI_INTERFACES_COUNT > 3
                else if (spi == &SPI3)
                {
                    spi = NULL;
                }
#endif
#if SPI_INTERFACES_COUNT > 4
                else if (spi == &SPI4)
                {
                    spi = NULL;
                }
#endif
#if SPI_INTERFACES_COUNT > 5
                else if (spi == &SPI5)
                {
                    spi = NULL;
                }
#endif

#ifdef SPI
                if (spi != NULL)
                {
                    delete spi;
                    spi = NULL;
                }
#endif
            }
        }
        free(dmaBuf);
        dmaBuf = NULL;
        return false;
    }
        
};

FASTLED_NAMESPACE_END
#endif // __INC_CLOCKLESS_ARM_D21_DMA_H
