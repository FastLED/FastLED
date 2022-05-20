#if !defined(_DMA_PINS_H_)
#define _DMA_PINS_H_

// INCOHERENT RAMBLING NOTES ARE AT THE BOTTOM OF THIS FILE.

// clang-format off

/*
The tables declared here identify compatible pins and their corresponding
SERCOMs and DMA-related registers and such. SAMD21/51 can actually handle
SPI DMA on many more pins than are indicated here, but the code design
INTENTIONALLY limits it to specific pins -- one "curated" pin per SERCOM.
Not for technical reasons, but as a matter of documentation and clarity.
Although each SERCOM could work with a choice of MOSI pins, it becomes
extremely verbose to try to explain "if you choose pin X for output, then
you can't use pins Y or Z" (and then repeating this for every SERCOM,
times every supported board). It's INFINITELY SIMPLER to explain and use
if one "good pin" has been preselected per SERCOM for each board. The user
then has a list of valid pins for any given board, they can use any one
(or more, if multiple Adafruit_NeoPixel_ZeroDMA instances) in whatever
combination, no complicated if/else/else/else explanations needed.
I tried to pick pins that are nicely spaced around the board and don't
knock out other vital peripherals. SERCOM pin selection is NOT a fun
process, believe me, it's SO MUCH EASIER this way. Most programs will be
using only one output anyway, maybe a couple (if you need lots, consider
the NeoPXL8 library instead). 
*/
struct {
  SERCOM        *sercom;
  Sercom        *sercomBase;
  uint8_t        dmacID;
  uint8_t        mosi;
  SercomSpiTXPad padTX;
  EPioType       pinFunc;
} sercomTable[] = {
// sercom   base     dmacID              mosi  padTX            pinFunc

#if defined(ADAFRUIT_FEATHER_M0)
  // Serial1 (TX/RX) is on SERCOM0, do not use
  // SERCOM1,2 are 100% in the clear
  // I2C is on SERCOM3, do not use
  // SPI is on SERCOM4, but OK to use (as SPI MOSI)
  // Serial5 is on SERCOM5, but OK to use (Arduino core detritus)
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   12, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    6, SPI_PAD_2_SCK_3, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_FEATHER_M0_EXPRESS)
  // Serial1 (TX/RX) is on SERCOM0, do not use
  // SERCOM1,5 are 100% in the clear
  // SPI FLASH is on SERCOM2, do not use
  // I2C is on SERCOM3, do not use
  // SPI is on SERCOM4, but OK to use (as SPI MOSI)
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   12, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    6, SPI_PAD_2_SCK_3, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_FEATHER_M4_EXPRESS)
  // SERCOM0,3,4 are 100% clear to use
  // SPI is on SERCOM1, but OK to use (as SPI MOSI)
  // I2C is on SERCOM2, do not use
  // Serial1 (TX/RX) is on SERCOM5, do not use
  // Feather M4 uses QSPI flash, not on a SERCOM
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A4, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX, MOSI, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   12, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,   A2, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_ITSYBITSY_M0)
  // Serial1 (TX/RX) is on SERCOM0, do not use
  // SERCOM1 is 100% OK to use!
  // I2C is on SERCOM3, do not use
  // SPI is on SERCOM4, but OK to use (as SPI MOSI)
  // SPI FLASH (SPI1) is on SERCOM5, do not use
  // Pin 5 is the magic level-shifted pin on ItsyBitsy, enable if possible!
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   12, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_ITSYBITSY_M4_EXPRESS)
  // SPI is on SERCOM1, but OK to use (as SPI MOSI)
  // I2C is on SERCOM2, do not use
  // Serial1 (TX/RX) is on SERCOM3, do not use
  // ItsyBitsy M4 uses QSPI flash, not on a SERCOM
  // Pin 5 is the magic level-shifted pin on ItsyBitsy, enable if possible!
  // SERCOM0,4,5 are 100% clear to use!
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    2, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,   12, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_METRO_M0_EXPRESS)
  // Serial1 (TX/RX) is on SERCOM0, do not use
  // SERCOM1,2 are 100% in the clear
  // I2C is on SERCOM3, do not use
  // SPI is on SERCOM4, but OK to use (as SPI MOSI)
  // SPI FLASH (SPI1) is on SERCOM5, do not use
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   12, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif


#if defined(ADAFRUIT_METRO_M4_EXPRESS)
  // SERCOM0,1,4 are 100% in the clear
  // SPI is on SERCOM2, but OK to use (as SPI MOSI)
  // Serial1 (TX/RX) is on SERCOM3, do not use
  // I2C is on SERCOM5, do not use
  // Metro M4 uses QSPI flash, not on a SERCOM
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A3, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   11, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    6, SPI_PAD_3_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_METRO_M4_AIRLIFT_LITE)
  // Serial2 (to ESP32) is on SERCOM0, do not use
  // SERCOM1,4 are 100% in the clear
  // SPI is on SERCOM2, but OK to use (as SPI MOSI)
  // Serial1 (TX/RX) is on SERCOM3, do not use
  // I2C is on SERCOM5, do not use
  // Metro M4 uses QSPI flash, not on a SERCOM
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   11, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    6, SPI_PAD_3_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_GRAND_CENTRAL_M4)
  // SERCOM1,4,5 are 100% in the clear
  // Serial1 (TX/RX) is on SERCOM0, do not use
  // SPI1 (SD card) is on SERCOM2, do not use
  // I2C is on SERCOM3, do not use
  // I2C2 is on SERCOM6, do not use
  // SPI is on SERCOM7, but OK to use (as SPI MOSI)
  // Grand Central uses QSPI flash, not on a SERCOM
  // SERCOMs 1, 4 and 5 are mentioned in the board's variant.h file but
  // are not actually instantiated as Serial peripherals...probably a
  // carryover from an earlier board design, which had multiple TX/RX
  // selections. Consider these SERCOMs safe to use for now.
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   11, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,   23, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,   14, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom7, SERCOM7, SERCOM7_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif

#if defined(ARDUINO_SAMD_HALLOWING_M0)
  // SERCOM0,1 are 100% in the clear
  // Serial1 (TX/RX) is on SERCOM2, do not use
  // I2C is on SERCOM3, do not use
  // SPI FLASH is on SERCOM4, do not use
  // SPI (incl. screen) is on SERCOM5, but OK to use (as SPI MOSI)
  // NEOPIX jack is pin 4, SENSE is 3, backlight is 7 (avoid)
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    4, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,    6, SPI_PAD_2_SCK_3, PIO_SERCOM,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_HALLOWING_M4_EXPRESS)
  // SERCOM0,3 are 100% in the clear
  // TFT (SPI1) is on SERCOM1, do not use
  // I2C is on SERCOM2, do not use
  // Serial1 (TX/RX) is on SERCOM4, do not use
  // SPI is on SERCOM5, but OK to use (as SPI MOSI)
  // HalloWing M4 uses QSPI flash, not on a SERCOM
  // NEOPIX jack is pin D3 (PB02) -- this is a SERCOM5 pin and will
  // interfere with SPI (but not the TFT, on its own bus). Since this
  // is the ONLY option for DMA'ing to NEOPIX, it's allowed here, with
  // the understanding that external SPI is then unavailable.
  // Onboard NeoPixels are on D8 (PB16), also a SERCOM5 pin with the
  // same concern. THEREFORE, you get a choice: SPI interface to hardware on
  // the FeatherWing header -or- DMA out on MOSI pin -or- DMA NEOPIX jack
  // -or- DMA onboard pixels. ONLY ONE OF THESE.
  // UPDATE: pin 3 (the NEOPIX connector) does NOT work with DMA.
  // Not sure where the problem lies, have checked against datasheet
  // and that pin (PB02) SHOULD operate as SERCOM5/PAD[0]. I'll leave
  // it in the code for now, but avoid it in examples, README and docs.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A5, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,    6, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    3, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    8, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX, MOSI, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_MONSTER_M4SK_EXPRESS)
  // SERCOM0,4 are 100% in the clear (but no external pins to SERCOM0)
  // I2C is on SERCOM1, do not use
  // Right TFT (SPI) is on SERCOM2, do not use
  // PDM mic (SPI2) is on SERCOM3, do not use
  // Left TFT (SPI1) is on SERCOM5, do not use
  // Monster M4sk uses QSPI flash, not on a SERCOM
  // 3-pin JST is pin D2 (PB08)
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    2, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_PYPORTAL)
  // SERCOM0,1 are 100% in the clear, but few pins are exposed
  // SPI (SD card) is on SERCOM2, no pins exposed, do not use
  // Serial1 (TX/RX) is on SERCOM4, used for WiFi, do not use
  // I2C on SERCOM5, do not use
  // NEOPIX connector is pin 4 (PA05) -- the only SERCOM/PAD combo there
  // is SERCOM0/PAD[1], but PAD[1] can't be used for MOSI, so DMA is not
  // available on this pin.
  // SENSE connector is pin 3 (PA04) -- this DOES allow DMA, and is one of
  // the few exposed pins, so let's enable using that even though it's not
  // the canonical NeoPixel connector.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    3, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_PYPORTAL_M4_TITANO)
  // Same rules and oddness as PYPORTAL_M4 above
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    3, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_PYGAMER_M4_EXPRESS)
  // SERCOM0,3 are 100% in the clear, but few pins exposed
  // SPI (SD card) is on SERCOM1, no pins exposed, do not use
  // I2C on SERCOM2, do not use
  // SPI1 (TFT) is on SERCOM4, no pins exposed, do not use
  // Serial1 (TX/RX) is on SERCOM5
  // PyGamer uses QSPI flash, not on a SERCOM
  // NEOPIX connector is pin 2 (PB03) -- unfortunately that's SERCOM5/PAD[1]
  // with no other options, and PAD[1] can't be a MOSI output.
  // SENSE connector is pin 3 (PB02) -- SERCOM5/PAD[0], which could be a
  // MOSI out, but interferes with Serial1.
  // Onboard NeoPixels on pin 8 (PA15) -- SERCOM2/PAD[3] or SERCOM4/PAD[3],
  // both in use (I2C and SPI1, respectively). It's a short length of pixels
  // and DMA isn't likely to be a huge benefit there anyway.
  // A couple pins on the FeatherWing header are OK though...
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A4, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   12, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_PYGAMER_ADVANCE_M4_EXPRESS)
  // Requirements are identical to PYGAMER_M4_EXPRESS above
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A4, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   12, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_PYBADGE_M4_EXPRESS)
  // SERCOM0 is 100% in the clear, but few pins exposed
  // SPI is on SERCOM1, but OK to use (as SPI MOSI)
  // I2C on SERCOM2, do not use
  // SPI1 (TFT) is on SERCOM4, no pins exposed, do not use
  // PDM mic is on SERCOM3, do not use
  // Serial1 (TX/RX) is on SERCOM5
  // Rules are similar to PyGamer, but without an SD card we at least
  // allow an option of using MOSI pin (but losing SPI in the process).
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A4, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX, MOSI, SPI_PAD_3_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_PYBADGE_AIRLIFT_M4)
  // Requirements are identical to PYBADGE_M4_EXPRESS above
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A4, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX, MOSI, SPI_PAD_3_SCK_1, PIO_SERCOM,
#endif

#if defined(ADAFRUIT_CRICKIT_M0)
  // I2C on SERCOM1, do not use
  // Serial1 (TX/RX) is on SERCOM5, do not use
  // A11 = Captouch 3, A8 = Signal 8, 11 = NeoPixel, D8 = Servo 3
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,  A11, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,   A8, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   11, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    8, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_CIRCUITPLAYGROUND_M0)
  // SERCOM0 is allowed, but SPI-using Gizmos not compatible
  // "Internal" I2C (for LIS3DH) is on SERCOM1, do not use
  // SERCOM2 would be in the clear, but all the MOSI-capable pins are
  // assigned to other tasks: 5 = right, 7 = switch, 26 = IR in
  // SPI FLASH (SPI1) is on SERCOM3, do not use
  // Serial1 (TX/RX) is on SERCOM4, do not use
  // I2C is on SERCOM5, do not use
  // Onboard NeoPixels are pin 8 (SERCOM1/PAD[3] or SERCOM5/PAD[3]),
  // either would interfere with other peripherals, so not supported.
  // That leaves A2 as the only really safe output, and only then
  // if not using SPI-centric Gizmos:
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A2, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_TRINKET_M0)
  // Looks like SPI, Serial1 and I2C all on SERCOM0 (only one can be active),
  // so using DMA NeoPixels means no special peripherals, sorry.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    4, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_GEMMA_M0)
  // Looks like SPI, Serial1 and I2C all on SERCOM0 (only one can be active),
  // so using DMA NeoPixels means no special peripherals, sorry.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    0, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#endif

#if defined(ADAFRUIT_QTPY_M0)
  // We can't use SERCOM0 because Serial1 uses it so that rules out TX/A6/D6.
  // That leaves 3 other possible SERCOM/pin combinations:
  //  * SERCOM1 + SDA/D4 (used for I2C)
  //  * SERCOM2 + MOSI/A10/D10 (used for SPI)
  //  * SERCOM3 + PIN_SPI1_MOSI/D16) (used for the SPI Flash chip that can be soldered 
  //    onto the bottom)
  // Since using those pins means giving up the associated peripheral,
  // and since I2C is featured prominently on the QT Py, it makes the most
  // sense to enable DMA Neopixels on the MOSI pins, though it means sacrificing
  // either SPI peripherals or the (optional) flash chip.  Sorry.
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   16, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif

#if defined(SEEED_XIAO_M0)
  // We can't use SERCOM0 because Serial1 uses it so that rules out TX/A6/D6.
  // That leaves 3 other possible SERCOM/pin combinations:
  //  * SERCOM1 + SDA/D4 (used for I2C)
  //  * SERCOM2 + MOSI/A10/D10 (used for SPI)
  //  * SERCOM3 + PIN_SPI1_MOSI/D16) (used for the SPI Flash chip that can be soldered 
  //    onto the bottom)
  // Since using those pins means giving up the associated peripheral,
  // and since I2C is featured prominently on the QT Py, it makes the most
  // sense to enable DMA Neopixels on the MOSI pins, though it means sacrificing
  // either SPI peripherals or the (optional) flash chip.  Sorry.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,   16, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif  

#if defined(ADAFRUIT_NEOKEYTRINKEY_M0)
  // Onboard NeoPixel
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    0, SPI_PAD_3_SCK_1, PIO_SERCOM,
#endif  

#if defined(USB_PID) && (USB_PID == 0x804d) // ARDUINO ZERO
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   12, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX, MOSI, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#endif // end Arduino Zero

#if defined(USB_PID) && (USB_PID == 0x8057) // ARDUINO NANO 33 IOT
  // SERCOM0 is the only one 100% in the clear; others overlap serial1/2,
  // WiFi, Wire, etc.
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    4, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    6, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    7, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A2, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A3, SPI_PAD_2_SCK_3, PIO_SERCOM,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM,
#endif // end Arduino NANO 33 IoT

}; // end sercomTable[]

#define N_SERCOMS (sizeof sercomTable / sizeof sercomTable[0])

// NOTE TO FUTURE SELF: avoiding pins used by essential SERCOM peripherals
// (I2C, Serial) was a design decision to make documentation easier (not
// having to explain "if you have DMA NeoPixels on this pin, you can't use
// I2C devices") (exception being the SPI MOSI pin, because the older
// library handled that on Metro boards and it's not as widely used as I2C).
// HOWEVER, on many boards where there's only an "external" I2C bus (no
// onboard sensors or such sharing the bus), it mmmmight be sensible to
// allow DMA NeoPixels on either the SDA or SCL pins, since the NeoPixels
// at that point physically block I2C (ditto for the Serial1 TX/RX pins) --
// it's sort of implied that the peripheral can't be used at the same time,
// but as implemented right now, it additionally *enforces* not using DMA
// NeoPixels on those pins (at all, not just when not-using-peripheral).
// Maybe that's too strict and not necessary. Or maybe the selection of
// pins here, as-is, adequately covers most situations. Just saying there
// might be a possibility of having to revisit these tables to add 1-2 more
// pin options that overlap I2C or Serial1 on boards where those are
// physically exposed and not shared with onboard peripherals. It's no fun,
// requires using the pinfinder.py script (in extras directory) and looking
// for the right missing items to add on a per-board basis.
// (There's a couple of exceptions in the lists, if a board just has no
// other SPI-MOSI-DMA-capable pins, one is exposed and will require an
// asterisk in the docs, that it takes out some other peripheral.)

// NOTE FOR FUTURE SAMD BOARD DESIGNS: to ensure one or more outwardly-
// accessible DMA-capable pins, choose a PORT/bit from the datasheet's
// signal mux table that has a SERCOM or SERCOM-ALT setting that's
// 1) not an existing SERCOM that's used for a vital peripheral (I2C,
// Serial1 or SPI, or in the case of certain boards with onboard sensors
// and a second internal peripheral bus, avoid that bus), and...
// 2) on PAD[0] or PAD[3], or if it's a SAMD21 part, then PAD[2] also.

// NOTE FROM PAST SELF: here's the pin table from an earlier version of the
// library. There were very few SAMD boards at the time so the #ifdef checks
// are pretty sloppy, and also it was less strict about allowing use on pins
// tied to other peripherals (like I2C) which would render them useless.
// The new library is more strict in this regard but is less likely to
// result in confusion when trying to use both DMA NeoPixels and other
// devices simultaneously.
#if 0
#if defined(ARDUINO_GEMMA_M0)
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    0, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
#elif defined(ARDUINO_TRINKET_M0)
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,    4, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
#elif defined(ADAFRUIT_CIRCUITPLAYGROUND_M0)
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A2, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,   A7, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    8, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
#elif defined(__SAMD51__) // Metro M4
  &sercom0, SERCOM0, SERCOM0_DMAC_ID_TX,   A3, SPI_PAD_0_SCK_1, PIO_SERCOM_ALT,
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   11, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX, MOSI, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom3, SERCOM3, SERCOM3_DMAC_ID_TX,    8, SPI_PAD_3_SCK_1, PIO_SERCOM_ALT,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,    6, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,    3, SPI_PAD_0_SCK_1, PIO_SERCOM,
#else // Metro M0
  &sercom1, SERCOM1, SERCOM1_DMAC_ID_TX,   11, SPI_PAD_0_SCK_1, PIO_SERCOM,
  &sercom2, SERCOM2, SERCOM2_DMAC_ID_TX,    5, SPI_PAD_3_SCK_1, PIO_SERCOM,
  &sercom4, SERCOM4, SERCOM4_DMAC_ID_TX,   23, SPI_PAD_2_SCK_3, PIO_SERCOM_ALT,
  &sercom5, SERCOM5, SERCOM5_DMAC_ID_TX,   A5, SPI_PAD_0_SCK_3, PIO_SERCOM_ALT,
#endif
#endif // 0

#endif // _DMA_PINS_H_
