Driving WS2812 LEDs via the ESP32-P4 RGB LCD Controller

The WS2812 (NeoPixel) protocol uses a ~1.25 µs bit period with precise high/low timing (e.g. “0” = ~0.35 µs high, ~0.85 µs low; “1” = ~0.8 µs high, ~0.45 µs low ). In principle the ESP32-P4’s LCD RGB (parallel) peripheral can be configured to generate such timed pulses. The LCD controller outputs data on up to 24 parallel GPIO lines, gated by a pixel clock (PCLK) and sync signals  . By choosing a suitable PCLK frequency and encoding each WS2812 bit as one or more “pixels,” the LCD engine can produce the required waveforms on multiple lines simultaneously.

RGB-LCD Peripheral: Capabilities & Configuration

The ESP-IDF esp_lcd driver exposes the RGB LCD (I80) controller on ESP32-P4. A panel is created via esp_lcd_new_rgb_panel(&config, &handle), where config (type esp_lcd_rgb_panel_config_t) specifies the data bus width, pixel clock, GPIO pins, DMA, and timing parameters  . Key fields include:
 • data_width (8/16/24) – number of parallel data lines.  For example, data_width=16 lets 16 GPIOs carry 16 WS2812 strips in parallel .
 • bits_per_pixel – color depth in the frame buffer. By default this equals data_width. We can set bits_per_pixel=16 so that each “pixel” is 16 bits wide, one bit per strip.
 • pclk_gpio_num, hsync_gpio_num, vsync_gpio_num, de_gpio_num, disp_gpio_num – select the GPIO pins used for PCLK, horizontal sync, vertical sync, data enable, and display-enable signals, respectively . Unused signals should be set to -1.
 • data_gpio_nums[24] – array of up to 24 GPIOs used as the data bus . Only the lowest data_width entries are used; e.g. with data_width=16, the first 16 data_gpio_nums are driven.

The esp_lcd_rgb_panel_config_t also includes timing parameters (rgb_timing_t) for HSYNC/VSYNC porches and pulse widths. In a WS2812 application we typically configure a very simple “frame” – for example, a horizontal active width equal to the number of encoded bits (pixels) per strip, and minimal blanking. By carefully choosing these parameters we can insert a long idle period (via VSYNC front porch or disabling DE) to act as the ~50 µs reset gap required after each LED frame.

Example configuration (pseudocode):

esp_lcd_rgb_panel_config_t cfg = {
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .data_width = 16,           // 16 parallel WS2812 strips
    .bits_per_pixel = 16,       // use all 16 bits as outputs (1 bit per line)
    .pclk_gpio_num = PIN_PCLK,
    .hsync_gpio_num = -1,       // not used for simple streaming
    .vsync_gpio_num = PIN_VSYNC,
    .de_gpio_num   = PIN_DE,    // use Data Enable to indicate active data
    .disp_gpio_num = PIN_DISP,  // optional display enable
    .data_gpio_nums = { PIN_D0, PIN_D1, /*...*/ PIN_D15 },
    .dma_burst_size = 64,
    .timings = {
        .pclk_hz = 3200000,     // e.g. 3.2 MHz pixel clock
        .h_res = N_pixels,      // number of PCLK pulses = bits per strip
        .v_res = 1,             // one line per “frame”
        .hsync_back_porch = 0,
        .hsync_front_porch = 0,
        .vsync_back_porch = 1,
        .vsync_front_porch = RESET_PERIOD_IN_LINES, 
        .vsync_pulse_width = 1,
    }
};
esp_lcd_panel_handle_t panel;
esp_lcd_new_rgb_panel(&cfg, &panel);
esp_lcd_rgb_panel_enable_output(panel); 

Here the pixel clock pclk_hz is set so that each clock tick is ~250–400 ns. For example, 3.2 MHz yields ~312.5 ns per PCLK. Then one WS2812 bit can be encoded in 4 pixels (4×312.5 = 1250 ns total): e.g. [HI, HI, LO, LO] for “1” and [HI, LO, LO, LO] for “0,” where each entry represents one PCLK with all data lines high or low. Other clock rates (e.g. 2.666 MHz, 6.4 MHz, etc.) can also be chosen and tested to get the T1/T2/T3 timings as close as needed.

Encoding WS2812 Bits in the Pixel Stream

Each WS2812 bit is mapped to a series of PCLK-driven “pixels.” For example, at 3.2 MHz PCLK (312.5 ns):
 • “0” bit: 1 pixel high (312 ns), 3 pixels low (938 ns) ⇒ high0.312 µs, low0.938 µs.
 • “1” bit: 2 pixels high (625 ns), 2 pixels low (625 ns) ⇒ high0.625 µs, low0.625 µs.
 
 
 These are close to the WS2812 requirements (0.4/0.85 and 0.8/0.45 µs). At 6.4 MHz PCLK (156.25 ns) one could use 2/6 pixels for “0” and 4/4 for “1” (high=312/625 ns, low=937/625 ns) . In general, choose an integer number of PCLK pulses per bit so that total ≈1.25 µs, and adjust front/back porches so the idle reset gap (50+ µs) appears as extra lines between frames.
 
 Within the software driver, you fill the LCD frame buffer (or bounce buffer) with 16-bit values where each bit corresponds to one strip’s output level. For instance:
 
 uint16_t *fb;
 esp_lcd_rgb_panel_get_frame_buffer(panel, 0, (void**)&fb);
 int idx = 0;
 for (int led = 0; led < NUM_LEDS; ++led) {
     for (int bit = 23; bit >= 0; --bit) { // GRB bits MSB first
         bool val = (led_colors[led] >> bit) & 1;
         if (val) {
             fb[idx++] = 0xFFFF; // all 16 lines HIGH
             fb[idx++] = 0xFFFF; 
             fb[idx++] = 0x0000; // then all LOW
             fb[idx++] = 0x0000;
         } else {
             fb[idx++] = 0xFFFF;
             fb[idx++] = 0x0000;
             fb[idx++] = 0x0000;
             fb[idx++] = 0x0000;
         }
     }
 }
 // Send frame via LCD DMA
 esp_lcd_rgb_panel_draw_bitmap(panel, fb);
 esp_lcd_rgb_panel_refresh(panel);
 
 This code uses 4 pixels per bit. After all bits of all LEDs are sent, one can let the DE signal go inactive and pause (or rely on the vsync front porch) for ≥50 µs to latch the LEDs.
 
 Parallel Data Bus (Strips per GPIO)
 
 By setting data_width = 16, the LCD peripheral drives 16 GPIOs in parallel. Each GPIO can be connected (via a level-shifter) to a separate WS2812 strip. Thus the engine can update 16 strips simultaneously with one frame’s data. (In theory up to 24 strips are possible if data_width=24, but board pin availability and DMA limits usually favor 16.) Each strip’s data is independent, but they share the same timing (same PCLK and bit boundaries). Practically, 16 strips is the limit per RGB engine.
 
 Since the ESP32-P4 also supports a Parallel IO (PARLIO) TX peripheral, one can drive additional strips by using that engine as well. The PARLIO TX unit (with its own clock line) also supports up to 16 parallel data lines . On ESP32-P4 there are typically two PARLIO TX units available (each up to 16 bits), so in principle you could drive up to 32 more strips. However, using both engines concurrently requires careful pin assignment (no overlap) and sufficient DMA channels. In summary, the RGB-LCD engine handles 16 strips; PARLIO can add another 16 per unit if needed.
 
 Code Example: Initialization and LED Update
 
 // 1. Configure and create RGB LCD panel
 esp_lcd_rgb_panel_config_t cfg = {
     .clk_src = LCD_CLK_SRC_DEFAULT,
     .data_width = 16,
     .bits_per_pixel = 16,
     .pclk_gpio_num = PIN_PCLK,
     .hsync_gpio_num = -1,
     .vsync_gpio_num = PIN_VSYNC,
     .de_gpio_num   = PIN_DE,
     .disp_gpio_num = PIN_DISP, 
     .data_gpio_nums = { PIN_D0, PIN_D1, /*... up to PIN_D15 ...*/ },
     .dma_burst_size = 64,
     .timings = {
         .pclk_hz = 3200000,
         .h_res = LED_BITS_PER_LINE, 
         .v_res = 1,
         .hsync_back_porch = 0,
         .hsync_front_porch = 0,
         .vsync_back_porch = 1,
         .vsync_front_porch = RESET_GAP_LINES,
         .vsync_pulse_width = 1
     }
 };
 esp_lcd_panel_handle_t panel;
 ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
 
 // 2. Allocate a frame buffer (in PSRAM or SRAM)
 uint16_t* framebuf;
 ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 0, (void**)&framebuf));
 
 // 3. Prepare the LED data pattern in framebuf (as shown above).
 //    [Fill framebuf with 16-bit words encoding each WS2812 bit]
 
 // 4. Start a DMA refresh (one “frame”)
 ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, framebuf));
 ESP_ERROR_CHECK(esp_lcd_rgb_panel_refresh(panel));
 
 // 5. Wait for at least 50 µs to latch LEDs (e.g. via a delay or vsync gap).
 
In practice, you may run this in a loop, double-buffering frame data for continuous updates. Using refresh-on-demand mode (setting cfg.


refresh_on_demand = true), one can manually trigger each LED update by calling esp_lcd_rgb_panel_refresh(panel) once the frame buffer is ready.

To generate the reset latch gap, the VSYNC front porch (vsync_front_porch) can be set to a value that yields ≥50 µs at the chosen PCLK. Alternatively, toggling the DISP (display-enable) GPIO low for the reset duration will hold all data lines inactive. Either way, the WS2812 latch requires the line low for ~50 µs after the last bit.

PARLIO TX Peripheral Comparison

The Parallel IO (PARLIO) TX unit is another DMA-driven engine (separate from esp_lcd). Each PARLIO TX unit supports its own clock line and up to 16 data outputs . Configuring PARLIO is similar: you specify .data_width = 16, .clk_out_gpio_num, and the data GPIOs in parlio_tx_unit_config_t. For example:

parlio_tx_unit_config_t pconfig = {
    .clk_src = PARLIO_CLK_SRC_DEFAULT,
    .data_width = 16,
    .output_clk_freq_hz = 800000,    // 800 kHz WS2812
    .clk_out_gpio_num = PIN_CLK,
    .data_gpio_nums = {PIN_P0, PIN_P1, /*...*/ PIN_P15},
    .sample_edge = PARLIO_SAMPLE_EDGE_NEG,
};
parlio_tx_unit_handle_t tx_unit;
ESP_ERROR_CHECK(parlio_new_tx_unit(&pconfig, &tx_unit));
ESP_ERROR_CHECK(parlio_tx_unit_enable(tx_unit));
// Fill a buffer with WS2812 bit patterns (most significant bit first per word)
// and use parlio_tx_write() or equivalent API to send it.

Capacity: The RGB LCD engine handles 16 strips; PARLIO TX can drive 16 per unit. On P4, two TX units could drive 32, but in practice WLED examples show stable 16 strips via PARLIO (with room to scale) and 16 via LCD . Concurrency: Both engines use separate DMA channels, so they can run simultaneously provided they use different GPIOs and DMA resources. Overall, one could conceivably drive ~32 strips (16+16) at once with careful setup.

Performance, Bandwidth, and Limitations
 • Data rate: Each LED uses 24 bits. At 800 kHz and 16 strips, updating 1024 LEDs/strip (24,576 bits) takes ~30.72 ms (≈32 kB). The ESP32-P4’s DMA and memory bandwidth easily support this (tens of MB/s capability). With 16×1024 LEDs, you can achieve ~32 fps; with fewer LEDs or higher clock (e.g. 1.2 MHz), over 60–120 fps is possible. In WLED tests, 16×256 LEDs ran at ~130 fps . CPU usage is minimal since DMA handles the transfer; only buffer preparation and triggering uses CPU.
 • DMA/GDMA: The LCD driver uses GDMA; PARLIO TX also uses GDMA. They each allocate DMA descriptors for the frame. Configure dma_burst_size (power-of-2) for efficiency  . Neither engine will disturb Wi-Fi or other buses significantly at these data rates, but ensure sufficient free DMA channels (typically ample on P4).
 • GPIO Conflicts: The LCD controller “steals” its signals (PCLK, VSYNC, etc.) while active. Choose non-conflicting pins or set unused signals to -1. PARLIO similarly claims its pins. You cannot use a GPIO for two peripherals simultaneously. Also note that if you enable the “valid” signal in PARLIO, it may reduce the effective data bus width by 1 (as documented) .
 • Timing Diagram: Use a logic analyzer to verify one strip’s waveform. The analyzer should show 4 PCLK pulses per WS bit with the expected high/low durations. Label the bit times (T0H, T1H, etc.) and confirm they meet the ~1.25 µs period (refer to the timing diagram above). Ensure the reset gap (no PCLK for ≥50 µs) is present between frames.

Integration with FastLED

FastLED’s ESP32-S2/S3 LCD driver already uses the esp_lcd (I80) interface for WS2812. To use this backend on ESP32-P4, one could adapt the same approach. In FastLED you’d define FASTLED_ESP32_LCD_DRIVER and then call FastLED.addLeds<WS2812, DATA_PIN>(leds, numLeds), where DATA_PIN is used by the driver (it will reassign to the LCD pins). FastLED allows customizing the bit timing (T1/T2/T3) per chipset, which maps to choosing the right PCLK and pixel encoding. Essentially you would write a FastLED driver that fills an esp_lcd frame buffer with the patterns described above, and override the timing for WS2812 (normally T1=350ns, T2=250ns, T3=400ns, etc.


) to match the generated pulses. This lets application code remain “FastLED-style” while offloading actual bit-banging to the LCD hardware.

References
 • Espressif ESP-IDF RGB LCD (I80) driver documentation: config fields and APIs  .
 • Espressif Parallel IO (PARLIO) TX driver docs: supports up to 16 data lines per unit .
 • WS2812 timing from manufacturer/app notes .

---

## Implementation Status

### ✅ Completed (Date: 2025-01-XX)

#### Phase 1: ESP32 I80 LCD Driver Refactoring
- ✅ Created unified `lcd_cam/` subdirectory structure in `src/platforms/esp/32/drivers/lcd_cam/`
- ✅ Extracted I80 LCD driver to modular architecture:
  - `lcd_driver_common.h` - Shared structures and pin validation utilities
  - `lcd_driver_i80.h` - I80/LCD_CAM driver class definition
  - `lcd_driver_i80_impl.h` - Template implementation for I80
- ✅ Refactored existing wrapper files to use new structure:
  - Updated `clockless_lcd_i80_esp32.h` to include from same directory
  - Updated `clockless_lcd_i80_esp32.cpp` to use `LcdI80Driver` class
- ✅ Preserved backward compatibility - all existing LCD code still works
- ✅ Verified with unit tests - all C++ tests pass

#### Phase 2: ESP32 RGB LCD Driver Implementation
- ✅ Implemented RGB LCD driver following TASK.md specification:
  - `lcd_driver_rgb.h` - RGB LCD driver class definition
  - `lcd_driver_rgb_impl.h` - Template implementation for RGB
- ✅ Key RGB implementation features:
  - 4-pixel encoding for WS2812 timing (vs I80's 3-word encoding)
  - Uses `esp_lcd_new_rgb_panel()` API (vs I80's I80 API)
  - VSYNC front porch for reset gap generation
  - Optimized PCLK calculation via `ClocklessTiming` module
- ✅ Created FastLED controller wrappers:
  - `clockless_lcd_rgb_esp32.h` - Controller class definitions
  - `clockless_lcd_rgb_esp32.cpp` - RectangularDrawBuffer integration
- ✅ Follows same patterns as I80/I2S/PARLIO drivers for consistency

### Architecture Summary

**File Structure:**
```
src/platforms/esp/32/drivers/lcd_cam/  (LCD_CAM driver code - peripheral-based naming)
├── lcd_driver_common.h                - Shared config, pin validation
├── lcd_driver_i80.h                   - I80/LCD_CAM driver (ESP32-S3, potentially others)
├── lcd_driver_i80_impl.h              - I80 implementation
├── lcd_driver_rgb.h                   - RGB LCD driver (ESP32-P4, potentially others)
├── lcd_driver_rgb_impl.h              - RGB implementation
├── clockless_lcd_i80_esp32.h          - I80 controller wrapper
├── clockless_lcd_i80_esp32.cpp        - I80 RectangularDrawBuffer integration
├── clockless_lcd_rgb_esp32.h          - RGB controller wrapper
├── clockless_lcd_rgb_esp32.cpp        - RGB RectangularDrawBuffer integration
└── clockless_lcd_esp32s3_impl.hpp     - ESP32-S3 specific implementation
```

**Key Differences: I80 vs RGB**

| Aspect | I80/LCD_CAM | RGB LCD |
|--------|-------------|---------|
| Platforms | ESP32-S3 | ESP32-P4 |
| Peripheral | LCD_CAM (I80 mode) | RGB LCD controller |
| API | `esp_lcd_panel_io_i80` | `esp_lcd_rgb_panel` |
| Headers | `hal/lcd_ll.h`, `hal/lcd_hal.h` | `esp_lcd_panel_rgb.h` |
| Encoding | 3-word (6 bytes/bit) | 4-pixel (8 bytes/bit) |
| PCLK Range | 1-80 MHz | 1-40 MHz |
| Sync Signals | CS, DC | HSYNC, VSYNC, DE, DISP |
| Reset Gap | DC idle | VSYNC front porch |
| Max Strips | 16 parallel | 16 parallel |

### Testing Notes

- ✅ I80 refactor verified with existing unit tests
- ⚠️ RGB implementation requires actual ESP32-P4 hardware for testing
- ⚠️ RGB GPIO pin assignments currently hardcoded in clockless_lcd_rgb_esp32.cpp
  - TODO: Make PCLK/VSYNC/HSYNC/DE/DISP pins user-configurable

### Next Steps / TODOs

1. **Hardware Testing**:
   - Test RGB driver on actual ESP32-P4 hardware with WS2812 strips
   - Verify timing with logic analyzer
   - Measure actual frame rates and memory usage

2. **GPIO Configuration**:
   - Add user API to configure PCLK and sync signal pins
   - Document platform-specific pin constraints from datasheets
   - Update `validate_esp32p4_lcd_pin()` with actual reserved pins

3. **Performance Optimization**:
   - Fine-tune PCLK frequency for different chipsets (SK6812, APA102, etc.)
   - Test PSRAM vs SRAM buffer performance
   - Benchmark I80 vs RGB vs PARLIO drivers

4. **Documentation**:
   - Add RGB LCD driver examples
   - Document memory requirements and performance characteristics
   - Create migration guide for users upgrading from PARLIO

5. **Feature Parity**:
   - Ensure RGB driver supports all features available in I80 driver
   - Consider dual-engine mode (RGB LCD + PARLIO for 32 strips on P4)