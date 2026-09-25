// FastLED#4540: bare `int` as a template argument in production code.
//
// `int` is 16-bit on AVR and 32-bit elsewhere, so `vector<int>`,
// `flat_map<int, T>`, `numeric_limits<int>`, `static_cast<int>(x)` -- any
// template argument list or cast that names bare `int` -- silently changes
// width and RAM per target. Use a width-aware FastLED type instead (fl::u16 /
// fl::i16 for bounded indices, fl::size for counts, fl::i32 when the range
// needs it).
//
// Not flagged: `template <int N>` parameter declarations (these are the
// public sketch API, e.g. `addLeds<WS2812, int DATA_PIN>`), function
// parameters and locals, comments, and third-party / fl::stl code. Existing
// uses are ratcheted per file by INT_TEMPLATE_ARG_BASELINE: a file may not
// exceed its recorded count, and files not listed may have none. A genuine
// ABI need can add `// fl-lint: int-template-ok <reason>` on the line.

const INT_TEMPLATE_SUPPRESSION: &str = "fl-lint: int-template-ok";

fn int_template_arg_regex() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"[<,]\s*(?:(?:un)?signed\s+)?int\s*[>,]").unwrap()
    })
}

/// Returns the template (or cast) name for every bare-`int` template
/// argument in `code`, which must already have comments stripped.
fn find_int_template_args(code: &str) -> Vec<String> {
    let bytes = code.as_bytes();
    let mut hits = Vec::new();
    let mut search = 0;
    while let Some(m) = int_template_arg_regex().find_at(code, search) {
        // Overlapping matches share the separator (`<int, int>`).
        search = m.end() - 1;
        // Walk left to the unmatched opener; it must be `<`, not `(`.
        let mut depth_angle = 0i32;
        let mut depth_paren = 0i32;
        let mut opener = None;
        let mut i = m.start() + 1; // include the `<`/`,` itself
        while i > 0 {
            i -= 1;
            match bytes[i] {
                b'>' => depth_angle += 1,
                b')' => depth_paren += 1,
                b'(' => {
                    if depth_paren == 0 {
                        break;
                    }
                    depth_paren -= 1;
                }
                b'<' => {
                    if depth_angle == 0 && depth_paren == 0 {
                        opener = Some(i);
                        break;
                    }
                    depth_angle -= 1;
                }
                b';' | b'{' | b'}' => break,
                _ => {}
            }
        }
        let Some(open) = opener else { continue };
        let name_end = code[..open].trim_end().len();
        let name_start = code[..name_end]
            .rfind(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
            .map_or(0, |p| p + 1);
        let name = &code[name_start..name_end];
        // `template <int N>` / `template <typename T, int N>` declare
        // non-type parameters; they are not instantiations.
        if name.is_empty() || name == "template" {
            continue;
        }
        let args = &code[open..m.end()];
        if args.contains("typename") || args.contains("class ") {
            continue;
        }
        hits.push(name.to_string());
    }
    hits
}

struct IntTemplateArgChecker;

impl IntTemplateArgChecker {
    fn violations_for(&self, path: &str, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut found = Vec::new();
        let mut in_multiline_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let mut code = split_line_comment(line).to_string();
            if in_multiline_comment {
                match code.find("*/") {
                    Some(end) => {
                        code.replace_range(..end + 2, " ");
                        in_multiline_comment = false;
                    }
                    None => continue,
                }
            }
            while let Some(open) = code.find("/*") {
                match code[open + 2..].find("*/") {
                    Some(rel) => code.replace_range(open..open + 2 + rel + 2, " "),
                    None => {
                        code.truncate(open);
                        in_multiline_comment = true;
                        break;
                    }
                }
            }
            if line.contains(INT_TEMPLATE_SUPPRESSION) || !code.contains("int") {
                continue;
            }
            for name in find_int_template_args(&code) {
                found.push((index + 1, name, line.trim().to_string()));
            }
        }
        let allowed = INT_TEMPLATE_ARG_BASELINE
            .iter()
            .find(|(file, _)| path.ends_with(file))
            .map_or(0, |(_, n)| *n);
        if found.len() <= allowed {
            return Vec::new();
        }
        let count = found.len();
        found
            .into_iter()
            .map(|(line_no, name, text)| {
                (
                    line_no,
                    format!(
                        "bare 'int' template argument in '{name}<...>' ({count} in file, baseline {allowed}): \
                         int is 16-bit on AVR; use fl::u16/fl::i16 for bounded indices, fl::size \
                         for counts, or fl::i32, or add `// {INT_TEMPLATE_SUPPRESSION} <reason>` \
                         (FastLED#4540): {text}"
                    ),
                )
            })
            .collect()
    }
}

impl FileContentChecker for IntTemplateArgChecker {
    fn name(&self) -> &'static str {
        "IntTemplateArgChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "src")
            && ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp"])
            && !normalized.contains("/third_party/")
            && !normalized.contains("/fl/stl/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("int") {
            return Vec::new();
        }
        self.violations_for(&file_content.path, file_content)
    }
}

/// Per-file counts of bare-`int` template arguments that predate
/// FastLED#4540. Lower a count when a file is cleaned up; never raise it.
const INT_TEMPLATE_ARG_BASELINE: &[(&str, usize)] = &[
    ("src/FastLED.h", 17),
    ("src/fl/audio/audio_context.cpp.hpp", 9),
    ("src/fl/audio/audio_context.h", 1),
    ("src/fl/audio/audio_reactive.cpp.hpp", 3),
    ("src/fl/audio/detector/buildup.cpp.hpp", 1),
    ("src/fl/audio/detector/chord.cpp.hpp", 3),
    ("src/fl/audio/detector/chord.h", 1),
    ("src/fl/audio/detector/drop.cpp.hpp", 4),
    ("src/fl/audio/detector/equalizer.cpp.hpp", 3),
    ("src/fl/audio/detector/frequency_bands.cpp.hpp", 1),
    ("src/fl/audio/detector/key.cpp.hpp", 3),
    ("src/fl/audio/detector/mood_analyzer.cpp.hpp", 2),
    ("src/fl/audio/detector/note.cpp.hpp", 4),
    ("src/fl/audio/detector/percussion.cpp.hpp", 1),
    ("src/fl/audio/detector/pitch.cpp.hpp", 5),
    ("src/fl/audio/detector/vocal.cpp.hpp", 8),
    ("src/fl/audio/fft/fft.cpp.hpp", 4),
    ("src/fl/audio/fft/fft_backend.h", 1),
    ("src/fl/audio/fft/fft_impl.cpp.hpp", 18),
    ("src/fl/audio/input.h", 1),
    ("src/fl/channels/adapters/spi_channel_adapter.cpp.hpp", 4),
    ("src/fl/channels/adapters/spi_channel_adapter.h", 2),
    ("src/fl/channels/bus_info.json.h", 1),
    ("src/fl/channels/channel.cpp.hpp", 3),
    ("src/fl/channels/detail/validation/rx_test.cpp.hpp", 1),
    ("src/fl/channels/id_tracker.h", 1),
    ("src/fl/channels/spi/config.h", 1),
    ("src/fl/channels/uart_wave_encoder.cpp.hpp", 3),
    ("src/fl/channels/validation.cpp.hpp", 1),
    ("src/fl/channels/validation.h", 2),
    ("src/fl/chipsets/apa102.h", 3),
    ("src/fl/chipsets/encoders/pixel_iterator.h", 1),
    ("src/fl/chipsets/ucs7604.h", 3),
    ("src/fl/codec/mp3.cpp.hpp", 3),
    ("src/fl/control/wled.cpp.hpp", 6),
    ("src/fl/control/wled/client.cpp.hpp", 7),
    ("src/fl/fx/2d/animartrix.hpp", 12),
    ("src/fl/fx/2d/flowfield.h", 3),
    ("src/fl/fx/2d/luminova.cpp.hpp", 6),
    ("src/fl/fx/fx_engine.h", 1),
    ("src/fl/gfx/blur.cpp.hpp", 1),
    ("src/fl/gfx/colorutils.h", 3),
    ("src/fl/gfx/corkscrew.h", 1),
    ("src/fl/gfx/crgb_json.cpp.hpp", 3),
    ("src/fl/gfx/fill.h", 3),
    ("src/fl/gfx/primitives.h", 7),
    ("src/fl/gfx/rgbw_colorimetric.cpp.hpp", 2),
    ("src/fl/gfx/rgbww.cpp.hpp", 4),
    ("src/fl/gfx/sample.cpp.hpp", 10),
    ("src/fl/gfx/xypath.cpp.hpp", 1),
    ("src/fl/gfx/xypath_impls.cpp.hpp", 3),
    ("src/fl/math/alpha.h", 2),
    ("src/fl/math/filter/savitzky_golay_filter_impl.h", 2),
    ("src/fl/math/filter/spectral_variance_impl.h", 1),
    ("src/fl/math/fixed_point.h", 1),
    ("src/fl/math/fixed_point/traits.h", 1),
    ("src/fl/math/line_simplification.h", 2),
    ("src/fl/math/math.cpp.hpp", 7),
    ("src/fl/math/math.h", 4),
    ("src/fl/math/screenmap.cpp.hpp", 1),
    ("src/fl/math/soft_float.h", 2),
    ("src/fl/math/traverse_grid.h", 4),
    ("src/fl/math/wave/wave_simulation.cpp.hpp", 3),
    ("src/fl/math/wave/wave_simulation_real.cpp.hpp", 4),
    ("src/fl/math/xymap.h", 2),
    ("src/fl/net/http/fetch_request.cpp.hpp", 1),
    ("src/fl/net/http/stream_client.cpp.hpp", 1),
    ("src/fl/net/http/stream_server.cpp.hpp", 2),
    ("src/fl/remote/rpc/json_arg_converter.h", 1),
    ("src/fl/system/pin.cpp.hpp", 2),
    ("src/fl/system/pins.cpp.hpp", 3),
    ("src/fl/system/static_constexpr_defs.cpp.hpp", 6),
    ("src/fl/system/trace.h", 1),
    ("src/fl/task/executor.cpp.hpp", 1),
    ("src/fl/task/promise.h", 3),
    ("src/fl/task/scheduler.h", 1),
    ("src/fl/task/task.cpp.hpp", 1),
    ("src/fl/task/task.h", 1),
    ("src/fl/test/fltest.cpp.hpp", 1),
    ("src/fl/ui/checkbox.h", 1),
    ("src/fl/ui/dropdown.h", 3),
    ("src/fl/ui/number_field.h", 2),
    ("src/fl/ui/slider.h", 4),
    ("src/pixel_controller.h", 4),
    ("src/platforms/arduino/audio_input.hpp", 1),
    ("src/platforms/arm/d21/init_channel_driver_samd21.cpp.hpp", 1),
    ("src/platforms/arm/d51/init_channel_driver_samd51.cpp.hpp", 1),
    ("src/platforms/arm/lpc/drivers/uart_dma/channel_engine_lpc_uart_dma.cpp.hpp", 2),
    ("src/platforms/arm/mxrt1062/spi_device_proxy.h", 2),
    ("src/platforms/arm/nrf52/init_channel_driver_nrf52.cpp.hpp", 1),
    ("src/platforms/arm/nrf52/isr_nrf52.hpp", 3),
    ("src/platforms/arm/nrf52/spi_device_proxy.h", 2),
    ("src/platforms/arm/rp/ble_rp.cpp.hpp", 1),
    ("src/platforms/arm/rp/isr_rp2040.hpp", 4),
    ("src/platforms/arm/rp/isr_rp2350.hpp", 4),
    ("src/platforms/arm/rp/pin_rp.hpp", 4),
    ("src/platforms/arm/rp/rpcommon/init_channel_driver_rp.cpp.hpp", 1),
    ("src/platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h", 2),
    ("src/platforms/arm/sam/spi_device_proxy.h", 2),
    ("src/platforms/arm/samd/isr_samd.hpp", 1),
    ("src/platforms/arm/samd/watchdog_samd.impl.hpp", 1),
    ("src/platforms/arm/stm32/drivers/spi_hw_2_stm32.cpp.hpp", 6),
    ("src/platforms/arm/stm32/drivers/spi_hw_4_stm32.cpp.hpp", 10),
    ("src/platforms/arm/stm32/drivers/spi_hw_8_stm32.cpp.hpp", 11),
    ("src/platforms/arm/stm32/init_channel_driver_stm32.cpp.hpp", 1),
    ("src/platforms/arm/stm32/pin_stm32_native.hpp", 1),
    ("src/platforms/arm/stm32/spi_device_proxy.h", 2),
    ("src/platforms/arm/teensy/pin_teensy.hpp", 1),
    ("src/platforms/arm/teensy/pin_teensy_native.hpp", 1),
    ("src/platforms/arm/teensy/sdfat/FatLib/FatFile.h", 2),
    ("src/platforms/arm/teensy/teensy4_common/clockless_objectfled.cpp.hpp", 1),
    ("src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp", 1),
    ("src/platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_diagnostics.cpp.hpp", 8),
    ("src/platforms/arm/teensy/teensy4_common/init_channel_driver_mxrt1062.cpp.hpp", 1),
    ("src/platforms/arm/teensy/teensy4_common/spi_hw_2_mxrt1062.cpp.hpp", 2),
    ("src/platforms/arm/teensy/teensy4_common/spi_hw_4_mxrt1062.cpp.hpp", 4),
    ("src/platforms/avr/pin_avr_native.hpp", 2),
    ("src/platforms/ci13xx/pin_ci13xx.hpp", 2),
    ("src/platforms/esp/32/audio/devices/i2s.hpp", 1),
    ("src/platforms/esp/32/audio/devices/pdm.hpp", 1),
    ("src/platforms/esp/32/core/clock_divider.h", 1),
    ("src/platforms/esp/32/detail/io_esp_jtag_or_uart.hpp", 1),
    ("src/platforms/esp/32/drivers/ble/ble_esp32.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/channel_manager_esp32.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/cled.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx_mcpwm.cpp.hpp", 14),
    ("src/platforms/esp/32/drivers/i2s/channel_driver_i2s.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.cpp.hpp", 12),
    ("src/platforms/esp/32/drivers/i2s/ii2s_lcd_cam_peripheral.h", 1),
    ("src/platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/i2s_spi/ii2s_spi_peripheral.h", 1),
    ("src/platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/lcd_cam/ilcd_rgb_peripheral.h", 1),
    ("src/platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_esp.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/lcd_spi/ilcd_spi_peripheral.h", 1),
    ("src/platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.cpp.hpp", 3),
    ("src/platforms/esp/32/drivers/parlio/channel_driver_parlio.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/parlio/iparlio_peripheral.h", 1),
    ("src/platforms/esp/32/drivers/parlio/parlio_engine.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/parlio/parlio_engine.h", 3),
    ("src/platforms/esp/32/drivers/parlio/parlio_peripheral_mock.cpp.hpp", 5),
    ("src/platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h", 1),
    ("src/platforms/esp/32/drivers/parlio_rx/parlio_rx_sampler.cpp.hpp", 3),
    ("src/platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.cpp.hpp", 26),
    ("src/platforms/esp/32/drivers/rmt/rmt_5/buffer_pool.cpp.hpp", 3),
    ("src/platforms/esp/32/drivers/rmt/rmt_5/channel_driver_rmt.cpp.hpp", 26),
    ("src/platforms/esp/32/drivers/rmt/rmt_5/rmt5_peripheral_esp.cpp.hpp", 1),
    ("src/platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.cpp.hpp", 21),
    ("src/platforms/esp/32/drivers/rmt_rx/rmt_rx_4/rmt_rx_channel_4.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/rmt_rx/rmt_rx_5/rmt_rx_channel_5.cpp.hpp", 23),
    ("src/platforms/esp/32/drivers/spi/channel_driver_spi.cpp.hpp", 20),
    ("src/platforms/esp/32/drivers/uart/uart_peripheral_esp.cpp.hpp", 2),
    ("src/platforms/esp/32/drivers/uart_esp32_idf.hpp", 11),
    ("src/platforms/esp/32/drivers/usb_serial_jtag_esp32_idf.hpp", 4),
    ("src/platforms/esp/8266/pin_esp8266_native.hpp", 2),
    ("src/platforms/posix/run_unit_test.hpp", 1),
    ("src/platforms/shared/active_strip_data/active_strip_data.cpp.hpp", 1),
    ("src/platforms/shared/active_strip_data/active_strip_data.h", 2),
    ("src/platforms/shared/active_strip_tracker/active_strip_tracker.h", 2),
    ("src/platforms/shared/mock/esp/32/drivers/rmt4_peripheral_mock.cpp.hpp", 1),
    ("src/platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.cpp.hpp", 3),
    ("src/platforms/shared/spi_manager.cpp.hpp", 16),
    ("src/platforms/shared/spi_types.cpp.hpp", 1),
    ("src/platforms/shared/ui/json/dropdown.cpp.hpp", 2),
    ("src/platforms/shared/ui/json/json_console.cpp.hpp", 2),
    ("src/platforms/shared/ui/json/json_console.h", 1),
    ("src/platforms/shared/ui/json/slider.cpp.hpp", 1),
    ("src/platforms/stub/Arduino.cpp.hpp", 2),
    ("src/platforms/stub/Arduino.h", 2),
    ("src/platforms/stub/stub_gpio.cpp.hpp", 12),
    ("src/platforms/wasm/fs_wasm.cpp.hpp", 1),
    ("src/platforms/wasm/js_bindings.cpp.hpp", 3),
    ("src/platforms/win/io_win.cpp.hpp", 2),
    ("src/platforms/win/socket_win.cpp.hpp", 11),
    ("src/power_mgt.cpp.hpp", 2),
];
