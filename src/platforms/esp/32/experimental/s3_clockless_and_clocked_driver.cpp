#include "enabled.h"


#if FASTLED_ESP32_SPI_CLOCKLESS_ENABLED


// File copied from https://github.com/chroma-tech/micropython/blob/fern/ports/esp32/usermodules/modcanopy/driver.h#L225

#ifndef traceISR_EXIT_TO_SCHEDULER
#define traceISR_EXIT_TO_SCHEDULER()
#endif

#include <driver/periph_ctrl.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_private/gdma.h>
#include <esp_rom_gpio.h>
#include <hal/dma_types.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>

#define FASTLED_NO_MCU
#include <FastLED.h>

#include <math.h>

#define COLOR_ORDER_BLUE(color_order) ((color_order) & 0x3)
#define COLOR_ORDER_GREEN(color_order) (((color_order) >> 3) & 0x3)
#define COLOR_ORDER_RED(color_order) (((color_order) >> 6) & 0x3)

struct ColorOrderIndex {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  ColorOrderIndex(EOrder color_order = RGB, bool reverse = false) {
    r = COLOR_ORDER_RED(color_order);
    g = COLOR_ORDER_GREEN(color_order);
    b = COLOR_ORDER_BLUE(color_order);
    if (reverse) {
      r = 2 - r;
      g = 2 - g;
      b = 2 - b;
    }
  }

  ColorOrderIndex reversed() {
    ColorOrderIndex out;
    out.r = 2 - r;
    out.g = 2 - g;
    out.b = 2 - b;
    return out;
  }
};

static const uint16_t srgb_gamma_default[256] = {
    0,    2,    5,    7,    10,   12,   14,   17,   19,   22,   24,   26,
    29,   32,   35,   38,   41,   44,   48,   51,   55,   59,   63,   68,
    72,   77,   82,   87,   92,   97,   103,  108,  114,  120,  126,  133,
    139,  146,  153,  160,  168,  175,  183,  191,  199,  207,  216,  225,
    234,  243,  252,  262,  271,  281,  292,  302,  313,  323,  334,  346,
    357,  369,  381,  393,  405,  418,  431,  444,  457,  470,  484,  498,
    512,  527,  541,  556,  571,  587,  602,  618,  634,  650,  667,  684,
    701,  718,  736,  753,  771,  790,  808,  827,  846,  865,  885,  905,
    925,  945,  966,  986,  1007, 1029, 1050, 1072, 1094, 1117, 1139, 1162,
    1185, 1209, 1233, 1257, 1281, 1305, 1330, 1355, 1381, 1406, 1432, 1458,
    1485, 1511, 1538, 1566, 1593, 1621, 1649, 1678, 1706, 1735, 1765, 1794,
    1824, 1854, 1885, 1915, 1946, 1978, 2009, 2041, 2073, 2106, 2138, 2171,
    2205, 2238, 2272, 2306, 2341, 2376, 2411, 2446, 2482, 2518, 2554, 2591,
    2628, 2665, 2703, 2741, 2779, 2817, 2856, 2895, 2935, 2974, 3014, 3055,
    3095, 3136, 3178, 3219, 3261, 3303, 3346, 3389, 3432, 3475, 3519, 3563,
    3608, 3653, 3698, 3743, 3789, 3835, 3882, 3928, 3975, 4023, 4070, 4118,
    4167, 4216, 4265, 4314, 4364, 4414, 4464, 4515, 4566, 4617, 4669, 4721,
    4773, 4826, 4879, 4932, 4986, 5040, 5095, 5149, 5204, 5260, 5316, 5372,
    5428, 5485, 5542, 5600, 5658, 5716, 5774, 5833, 5892, 5952, 6012, 6072,
    6133, 6194, 6255, 6317, 6379, 6441, 6504, 6567, 6631, 6695, 6759, 6823,
    6888, 6953, 7019, 7085, 7151, 7218, 7285, 7353, 7420, 7488, 7557, 7626,
    7695, 7765, 7835, 7905,
};

struct Gamma {
  float gamma, scale, offset;
  uint16_t lut[256];
  uint8_t lut8[256];

  Gamma(float gamma_ = 1.0, float scale_ = 1.0, float offset_ = 0.0)
      : gamma(gamma_), scale(scale_), offset(offset_) {
    if (scale < 0.0)
      scale = 0.0;
    memcpy(lut, srgb_gamma_default, sizeof(srgb_gamma_default));
    for (auto &val : lut) {
      val = uint16_t(apply(val, 31 * 255));
    }

    for (auto i = 0; i < 256; i++) {
      lut8[i] = uint8_t(apply(i, 255));
    }
  }

  float apply(uint16_t val, const int max_value) {
    auto fval = float(val) / float(max_value);
    fval = powf(fval, gamma);
    fval *= scale;
    fval += offset;
    if (fval > 1.0)
      fval = 1.0;
    if (fval < 0.0)
      fval = 0.0;
    return fval * float(max_value);
  }
};

Gamma default_gamma;

struct CRGBA {
  union {
    struct {
      uint8_t a;
      uint8_t r;
      uint8_t g;
      uint8_t b;
    };
    uint8_t raw[4];
  };

  CRGBA() {}
  CRGBA(CRGB &c, Gamma &gamma_r = default_gamma, Gamma &gamma_g = default_gamma,
        Gamma &gamma_b = default_gamma, uint8_t max_brightness = 255) {
    uint32_t rs = gamma_r.lut[c.r];
    uint32_t gs = gamma_g.lut[c.g];
    uint32_t bs = gamma_b.lut[c.b];
    uint32_t mb = uint32_t(max_brightness) + 1;
    rs = rs * mb / 256;
    gs = gs * mb / 256;
    bs = bs * mb / 256;

    uint32_t as = ((std::max(std::max(rs, gs), bs) + 1) >> 8) + 1;
    rs /= as;
    gs /= as;
    bs /= as;

    a = 0xe0 | uint8_t(as);
    r = uint8_t(rs);
    g = uint8_t(gs);
    b = uint8_t(bs);
  }
};

struct CRGBOut {
  Gamma gamma_r = default_gamma;
  Gamma gamma_g = default_gamma;
  Gamma gamma_b = default_gamma;
  ColorOrderIndex order = GRB;
  uint8_t brightness = 255;

  CRGB ApplyRGB(CRGB &in) {
    CRGB out;
    out.raw[order.r] = gamma_r.lut8[in.r];
    out.raw[order.g] = gamma_g.lut8[in.g];
    out.raw[order.b] = gamma_b.lut8[in.b];
    return out % brightness;
  }

  CRGBA ApplyRGBA(CRGB &in) {
    CRGBA in_rgba(in, gamma_r, gamma_g, gamma_b, brightness);
    CRGBA out;
    out.raw[0] = in_rgba.a;
    out.raw[order.r + 1] = in_rgba.r;
    out.raw[order.g + 1] = in_rgba.g;
    out.raw[order.b + 1] = in_rgba.b;
    return out;
  }
};

/// Simplified form of bits rotating function.  Based on code found here -
/// http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt - rotating data
/// into LSB for a faster write (the code using this data can happily walk the
/// array backwards)
__attribute__((always_inline)) inline void transpose8x1(unsigned char *A,
                                                        unsigned char *B) {
  uint32_t x, y, t;

  // Load the array and pack it into x and y.
  y = *(unsigned int *)(A);
  x = *(unsigned int *)(A + 4);

  // pre-transform x
  t = (x ^ (x >> 7)) & 0x00AA00AA;
  x = x ^ t ^ (t << 7);
  t = (x ^ (x >> 14)) & 0x0000CCCC;
  x = x ^ t ^ (t << 14);

  // pre-transform y
  t = (y ^ (y >> 7)) & 0x00AA00AA;
  y = y ^ t ^ (t << 7);
  t = (y ^ (y >> 14)) & 0x0000CCCC;
  y = y ^ t ^ (t << 14);

  // final transform
  t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
  y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
  x = t;

  B[7] = y;
  y >>= 8;
  B[6] = y;
  y >>= 8;
  B[5] = y;
  y >>= 8;
  B[4] = y;

  B[3] = x;
  x >>= 8;
  B[2] = x;
  x >>= 8;
  B[1] = x;
  x >>= 8;
  B[0] = x;
}

//-----------------------------------------------------------------------------
// Clockless driver
//-----------------------------------------------------------------------------

// hardcode these for now, until we have RGBW support
// const uint16_t bytes_per_pixel = 3;
// const uint16_t max_strips = 8;
const uint32_t minimum_delay_between_frames_us = 350;


// Zach's note: IRAM_ATTR is NOT honored for implicitly instantiated
// templates. This needs to be refactored.
template <uint16_t max_strips, uint16_t bytes_per_pixel>
class S3ClocklessDriver {
  uint16_t num_strips;
  uint16_t leds_per_strip;

  uint8_t *alloc_addr;
  uint8_t *dma_buf;
  gdma_channel_handle_t dma_chan;
  dma_descriptor_t *dma_desc;

  uint32_t show_ended_us;
  SemaphoreHandle_t xRenderSemaphore;

  static IRAM_ATTR bool dma_callback(gdma_channel_handle_t dma_chan,
                                     gdma_event_data_t *event_data,
                                     void *user_data);

public:
  S3ClocklessDriver() {}
  ~S3ClocklessDriver() { end(); }

  bool begin(const int *pins, uint16_t num_strips, uint16_t leds_per_strip) {
    if (num_strips == 0 || num_strips > 8) {
      ESP_LOGE("leds", "Invalid number of strips");
      return false;
    }

    this->num_strips = num_strips;
    this->leds_per_strip = leds_per_strip;

    // we always transfer enough bytes for max strips, even if we're only using
    // fewer than max
    uint32_t xfer_size = max_strips * leds_per_strip * bytes_per_pixel * 3;
    uint32_t buf_size = xfer_size + 3;
    int num_desc = (xfer_size + DMA_DESCRIPTOR_BUFFER_MAX_SIZE - 1) /
                   DMA_DESCRIPTOR_BUFFER_MAX_SIZE;
    uint32_t alloc_size = num_desc * sizeof(dma_descriptor_t) + buf_size;

    alloc_addr = (uint8_t *)heap_caps_malloc(alloc_size,
                                             MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (nullptr == alloc_addr) {
      return false;
    }

    // Find first 32-bit aligned address following descriptor list
    dma_buf =
        (uint8_t *)((uint32_t)(&alloc_addr[num_desc * sizeof(dma_descriptor_t) +
                                           3]) &
                    ~3);

    // LCD_CAM isn't enabled by default -- MUST begin with this:
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD bus
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(100);

    // Configure LCD clock
    LCD_CAM.lcd_clock.clk_en = 1;             // Enable clock
    LCD_CAM.lcd_clock.lcd_clk_sel = 2;        // PLL240M source
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;     // 1/1 fractional divide,
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 1;     // plus '99' below yields...
    LCD_CAM.lcd_clock.lcd_clkm_div_num = 99;  // 1:100 prescale (2.4 MHz CLK)
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;    // PCLK low in 1st half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;   // PCLK low idle
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1; // PCLK = CLK (ignore CLKCNT_N)

    // Configure frame format
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;    // i8080 mode (not RGB)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // Disable RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;  // Do NOT auto-frame
    LCD_CAM.lcd_data_dout_mode.val = 0;      // No data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1;  // Enable 'always out' mode
    LCD_CAM.lcd_user.lcd_8bits_order = 0;    // Do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0;      // Do not reverse bit order
    LCD_CAM.lcd_user.lcd_2byte_en = 0;       // 8-bit data mode
    LCD_CAM.lcd_user.lcd_dummy = 1;          // Dummy phase(s) @ LCD start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0; // 1 dummy phase
    LCD_CAM.lcd_user.lcd_cmd = 0;            // No command at LCD start
    // Dummy phase(s) MUST be enabled for DMA to trigger reliably.

    const uint8_t mux[] = {
        LCD_DATA_OUT0_IDX, LCD_DATA_OUT1_IDX, LCD_DATA_OUT2_IDX,
        LCD_DATA_OUT3_IDX, LCD_DATA_OUT4_IDX, LCD_DATA_OUT5_IDX,
        LCD_DATA_OUT6_IDX, LCD_DATA_OUT7_IDX,
    };

    // Route LCD signals to GPIO pins
    for (int i = 0; i < num_strips; i++) {
      if (pins[i] >= 0) {
        esp_rom_gpio_connect_out_signal(pins[i], mux[i], false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pins[i]], PIN_FUNC_GPIO);
        // gpio_set_drive_capability((gpio_num_t)pins[i], (gpio_drive_cap_t)3);
      }
    }

    // Set up DMA descriptor list (length and data are set before xfer)
    dma_desc = (dma_descriptor_t *)alloc_addr; // At start of alloc'd buffer
    int bytesToGo = xfer_size;
    int offset = 0;
    for (int i = 0; i < num_desc; i++) {
      int bytesThisPass = bytesToGo;
      if (bytesThisPass > DMA_DESCRIPTOR_BUFFER_MAX_SIZE) {
        bytesThisPass = DMA_DESCRIPTOR_BUFFER_MAX_SIZE;
      }
      dma_desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
      dma_desc[i].dw0.suc_eof = 0;
      dma_desc[i].next = &dma_desc[i + 1];
      dma_desc[i].dw0.size = dma_desc[i].dw0.length = bytesThisPass;
      dma_desc[i].buffer = &dma_buf[offset];
      bytesToGo -= bytesThisPass;
      offset += bytesThisPass;
    }
    dma_desc[num_desc - 1].dw0.suc_eof = 1;
    dma_desc[num_desc - 1].next = NULL;

    // Alloc DMA channel & connect it to LCD periph
    gdma_channel_alloc_config_t dma_chan_config = {
        .sibling_chan = NULL,
        .direction = GDMA_CHANNEL_DIRECTION_TX,
        .flags = {.reserve_sibling = 0}};
    gdma_new_channel(&dma_chan_config, &dma_chan);
    gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    gdma_strategy_config_t strategy_config = {.owner_check = false,
                                              .auto_update_desc = false};
    gdma_apply_strategy(dma_chan, &strategy_config);

    // Enable DMA transfer callback
    gdma_tx_event_callbacks_t tx_cbs = {.on_trans_eof = dma_callback};
    gdma_register_tx_event_callbacks(dma_chan, &tx_cbs, this);

    // Max count 1, initial count 0
    xRenderSemaphore = xSemaphoreCreateCounting(1, 0);

    // start it off
    xSemaphoreGive(xRenderSemaphore);

    return true;
  }

  void stage(uint8_t *leds, CRGBOut &out) {
    // we process & transpose one pixel at a time * max_strips
    uint8_t packed[bytes_per_pixel * max_strips] = {0};
    uint8_t transposed[bytes_per_pixel * max_strips] = {0};

    uint8_t *output = dma_buf;

    for (int i = 0; i < leds_per_strip; i++) {
      for (int j = 0; j < num_strips; j++) {
        // color order, gamma, brightness
        CRGB *p = &((CRGB *)leds)[i + j * leds_per_strip];
        CRGB pixel = out.ApplyRGB(*p);
        packed[j + 0] = pixel.raw[0];
        packed[j + 8] = pixel.raw[1];
        packed[j + 16] = pixel.raw[2];
      }

      // transpose
      for (int i = 0; i < bytes_per_pixel; i++) {
        transpose8x1((unsigned char *)(packed + 8 * i),
                     (unsigned char *)(transposed + 8 * i));
      }

      // copy to DMA buffer
      for (int i = 0; i < bytes_per_pixel * max_strips; i++, output += 3) {
        output[0] = 0xFF;
        output[1] = transposed[i];
        output[2] = 0x00;
      }
    }
  }

  void show(uint8_t *leds, CRGBOut &out) {
    // wait for previous call to show to complete
    xSemaphoreTake(xRenderSemaphore, portMAX_DELAY);

    gdma_reset(dma_chan);
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

    stage(leds, out);

    // ensure it's been at least 300 us since the end of the last frame
    uint32_t delta = micros() - show_ended_us;
    if (delta < minimum_delay_between_frames_us) {
      esp_rom_delay_us(minimum_delay_between_frames_us - delta);
    }

    // kick it off
    gdma_start(dma_chan, (intptr_t)&dma_desc[0]);
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_start = 1;
  }

  void end() {
    if (nullptr != dma_chan) {
      gdma_stop(dma_chan);
      gdma_disconnect(dma_chan);
      gdma_del_channel(dma_chan);
      dma_chan = nullptr;
    }
    if (nullptr != alloc_addr) {
      free(alloc_addr);
      alloc_addr = nullptr;
    }
  }
};

template <uint16_t max_strips, uint16_t bytes_per_pixel>
IRAM_ATTR bool S3ClocklessDriver<max_strips, bytes_per_pixel>::dma_callback(
    gdma_channel_handle_t dma_chan, gdma_event_data_t *event_data,
    void *user_data) {
  S3ClocklessDriver *_this = (S3ClocklessDriver *)user_data;

  // DMA callback seems to occur a moment before the last data has issued
  // (perhaps buffering between DMA and the LCD peripheral?), so pause a
  // moment before clearing the lcd_start flag. This figure was determined
  // empirically, not science...may need to increase if last-pixel trouble.
  esp_rom_delay_us(5);
  LCD_CAM.lcd_user.lcd_start = 0;

  _this->show_ended_us = micros();

  portBASE_TYPE HPTaskAwoken = 0;
  xSemaphoreGiveFromISR(_this->xRenderSemaphore, &HPTaskAwoken);
  if (HPTaskAwoken == pdTRUE) {
    portYIELD_FROM_ISR(HPTaskAwoken);
  }

  return true;
}

//-----------------------------------------------------------------------------
// Clocked driver
//-----------------------------------------------------------------------------

enum class ColorDepth : uint8_t { C8Bit = 1, C16Bit = 2 };
enum class SpiWidth : uint8_t { W1Bit = 1, W2Bit = 2, W4Bit = 4, W8Bit = 8 };
enum class ElementsPerPixel : uint8_t { E3 = 3, E4 = 4 };

constexpr size_t startFrameSize(ColorDepth depth, SpiWidth width) {
  return (depth == ColorDepth::C8Bit)    ? 4 * size_t(width)
         : (depth == ColorDepth::C16Bit) ? 16 * size_t(width)
                                         : 0;
}

constexpr size_t endFrameSize(ElementsPerPixel epp, SpiWidth numChannels,
                              uint16_t channelLength) {
  // one bit per pixel, rounded up to
  // the nearest byte. ch len 100 = 104
  return epp == ElementsPerPixel::E3
             ? 0
             : (channelLength + 7) / 8 * size_t(numChannels);
}

// template class with templates for ColorDepth, SpiWidth, ElementsPerPixel
template <ColorDepth color_depth, SpiWidth spi_width, ElementsPerPixel epp>
class NewS3ClockedDriver {
  uint16_t num_strips;
  uint16_t leds_per_strip;

  size_t dma_buffer_size;
  uint8_t *dma_data = NULL;
  spi_host_device_t spi_host_device;
  spi_device_handle_t spi_handle = NULL;
  spi_transaction_t spi_transaction;

  bool inited = false;

  SemaphoreHandle_t xRenderSemaphore;

  static IRAM_ATTR void dma_callback(spi_transaction_t *trans);

  int spi_pin_number(int i, const int *pins) {
    return (i < num_strips && pins[i] >= 0) ? pins[i] : 1;
  }

public:
  NewS3ClockedDriver() {}
  ~NewS3ClockedDriver() { end(); }

  bool begin(const int *pins, uint16_t num_strips, uint16_t leds_per_strip,
             uint8_t sck) {

    if (num_strips == 0 || num_strips > uint16_t(spi_width)) {
      ESP_LOGE("leds", "Invalid number of strips");
      return false;
    }

    if (inited) {
      return inited;
    }

    this->num_strips = num_strips;
    this->leds_per_strip = leds_per_strip;

    // allocate DMA buffer
    size_t pixel_data_size =
        leds_per_strip * size_t(color_depth) * size_t(epp) * size_t(spi_width);
    dma_buffer_size = startFrameSize(color_depth, spi_width) + pixel_data_size +
                      endFrameSize(epp, spi_width, leds_per_strip);

    // must have a 4 byte aligned buffer for SPI DMA
    uint32_t alignment = dma_buffer_size % 4;
    if (alignment) {
      dma_buffer_size += 4 - alignment;
    }

    dma_data = static_cast<uint8_t *>(
        heap_caps_malloc(dma_buffer_size, MALLOC_CAP_DMA));
    memset(dma_data, 0, dma_buffer_size);

    // init spi bus
    esp_err_t ret;
    spi_bus_config_t buscfg;
    memset(&buscfg, 0x00, sizeof(buscfg));

    // spi bus init requires all pins to be valid, so set unused pins to 1
    // and then reclaim pin 1 at the end. this won't work if we want to use pin
    // 1 as an actual LED output
    buscfg.sclk_io_num = sck;
    buscfg.data0_io_num = spi_pin_number(0, pins);
    buscfg.data1_io_num = spi_pin_number(1, pins);
    buscfg.data2_io_num = spi_pin_number(2, pins);
    buscfg.data3_io_num = spi_pin_number(3, pins);
    buscfg.data4_io_num = spi_pin_number(4, pins);
    buscfg.data5_io_num = spi_pin_number(5, pins);
    buscfg.data6_io_num = spi_pin_number(6, pins);
    buscfg.data7_io_num = spi_pin_number(7, pins);

    spi_host_device = SPI2_HOST;

    buscfg.max_transfer_sz = dma_buffer_size;
    if (spi_width == SpiWidth::W8Bit) {
      buscfg.flags = SPICOMMON_BUSFLAG_OCTAL;
    }
    ret = spi_bus_initialize(spi_host_device, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
      ESP_LOGE("leds", "spi_bus_initialize failed");
      return false;
    }

    // reclaim pin 1. does this actually work? hmm
    esp_rom_gpio_connect_out_signal(1, SIG_GPIO_OUT_IDX, false, true);

    // init spi device
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 4 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;
    if (spi_width == SpiWidth::W1Bit) {
      devcfg.flags = 0;
    } else {
      devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    }
    devcfg.post_cb = dma_callback;

    ret = spi_bus_add_device(spi_host_device, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
      ESP_LOGE("leds", "spi_bus_initialize failed");
      return false;
    }

    // Max count 1, initial count 0
    xRenderSemaphore = xSemaphoreCreateCounting(1, 0);
    xSemaphoreGive(xRenderSemaphore);

    inited = true;
    return inited;
  }

  void stage(uint8_t *leds, CRGBOut &out) {
    // we process & transpose one pixel at a time * max_strips
    uint8_t packed[size_t(epp) * size_t(color_depth) * size_t(spi_width)] = {0};

    uint8_t *output = dma_data + startFrameSize(color_depth, spi_width);

    for (int i = 0; i < leds_per_strip; i++) {
      for (int j = 0; j < num_strips; j++) {
        // color order, gamma, brightness
        CRGB *p = &((CRGB *)leds)[i + j * leds_per_strip];
        CRGBA pixel = out.ApplyRGBA(*p);

        // hardcoded for 8 bit for now
        packed[j + 0] = pixel.raw[0];
        packed[j + 8] = pixel.raw[1];
        packed[j + 16] = pixel.raw[2];
        packed[j + 24] = pixel.raw[3];
      }

      // transpose - hardcoded for 8 bit for now
      for (int i = 0; i < size_t(epp); i++) {

        transpose8x1((unsigned char *)(packed + 8 * i),
                     (unsigned char *)(output + 8 * i));
      }

      output += sizeof(packed);
    }
  }

  void show(uint8_t *leds, CRGBOut &out) {
    if (!inited) {
      ESP_LOGW("leds", "Show called before successful init");
      return;
    }

    // wait for previous call to show to complete
    xSemaphoreTake(xRenderSemaphore, portMAX_DELAY);

    // stage
    stage(leds, out);

    // prepare txn
    memset(&spi_transaction, 0, sizeof(spi_transaction));
    spi_transaction.length = dma_buffer_size * 8; // in bits not bytes!
    if (spi_width == SpiWidth::W1Bit) {
      spi_transaction.flags = 0;
    }
    if (spi_width == SpiWidth::W2Bit) {
      spi_transaction.flags = SPI_TRANS_MODE_DIO;
    }
    if (spi_width == SpiWidth::W4Bit) {
      spi_transaction.flags = SPI_TRANS_MODE_QIO;
    }
    if (spi_width == SpiWidth::W8Bit) {
      spi_transaction.flags = SPI_TRANS_MODE_OCT;
    }
    spi_transaction.tx_buffer = dma_data;
    spi_transaction.user = this;

    // kick it off
    esp_err_t ret = spi_device_queue_trans(spi_handle, &spi_transaction, 0);
    if (ret != ESP_OK) {
      ESP_LOGE("leds", "spi_device_queue_trans failed: %d", ret);
      xSemaphoreGive(xRenderSemaphore);
    }
  }

  void end() {
    if (nullptr != spi_handle) {
      spi_bus_remove_device(spi_handle);
      spi_handle = nullptr;
      spi_bus_free(spi_host_device);
    }

    if (nullptr != dma_data) {
      heap_caps_free(dma_data);
      dma_data = nullptr;
    }

    inited = false;
  }
};

template <ColorDepth color_depth, SpiWidth spi_width, ElementsPerPixel epp>
IRAM_ATTR void NewS3ClockedDriver<color_depth, spi_width, epp>::dma_callback(
    spi_transaction_t *spi_tran) {
  NewS3ClockedDriver *_this = (NewS3ClockedDriver *)spi_tran->user;

  portBASE_TYPE HPTaskAwoken = 0;
  xSemaphoreGiveFromISR(_this->xRenderSemaphore, &HPTaskAwoken);
  if (HPTaskAwoken == pdTRUE) {
    portYIELD_FROM_ISR(HPTaskAwoken);
  }
}

#endif  // FASTLED_ESP32_SPI_CLOCKLESS_ENABLED
