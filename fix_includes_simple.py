#!/usr/bin/env python3
"""Simple script to fix relative includes by direct string replacement"""

import re
from pathlib import Path
from typing import Dict, List


def fix_file(file_path: Path, replacements: Dict[str, str]) -> int:
    """Fix includes in a file using a dict of replacements"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return 0

    original = content
    fixes = 0

    for old, new in replacements.items():
        if old in content:
            content = content.replace(old, new)
            fixes += content.count(new) - original.count(new)

    if content != original:
        try:
            with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(content)
            print(f"Fixed {file_path}: {fixes} replacements")
            return fixes
        except Exception as e:
            print(f"Error writing {file_path}: {e}")
            return 0

    return 0


# Files in src/fl/ - go up one level to src/
fl_files = [
    "src/fl/crgb_hsv8.cpp",
    "src/fl/fastled.h",
    "src/fl/fastled_internal.cpp",
    "src/fl/fastpin.h",
    "src/fl/pixel_iterator.h",
    "src/fl/rgb8.h",
]

fl_replacements = {
    '#include "../hsv2rgb.h"': '#include "hsv2rgb.h"',
    '#include "../fastled_config.h"': '#include "fastled_config.h"',
    '#include "../led_sysdefs.h"': '#include "led_sysdefs.h"',
    '#include "../cpp_compat.h"': '#include "cpp_compat.h"',
    '#include "../color.h"': '#include "color.h"',
    '#include "../crgb.h"': '#include "crgb.h"',
    '#include "../chsv.h"': '#include "chsv.h"',
    '#include "../pixeltypes.h"': '#include "pixeltypes.h"',
    '#include "../eorder.h"': '#include "eorder.h"',
    '#include "../dither_mode.h"': '#include "dither_mode.h"',
    '#include "../pixel_controller.h"': '#include "pixel_controller.h"',
    '#include "../pixel_iterator.h"': '#include "pixel_iterator.h"',
    '#include "../cled_controller.h"': '#include "cled_controller.h"',
    '#include "../cpixel_ledcontroller.h"': '#include "cpixel_ledcontroller.h"',
    '#include "../controller.h"': '#include "controller.h"',
    '#include "../platforms.h"': '#include "platforms.h"',
    '#include "../fastled_delay.h"': '#include "fastled_delay.h"',
    '#include "../lib8tion.h"': '#include "lib8tion.h"',
    '#include "../bitswap.h"': '#include "bitswap.h"',
    '#include "../fastled_progmem.h"': '#include "fastled_progmem.h"',
    '#include "../fastpin.h"': '#include "fastpin.h"',
    '#include "../platforms/fastpin.h"': '#include "platforms/fastpin.h"',
    '#include "../rgbw.h"': '#include "rgbw.h"',
}

# Platform files - various depths
platform_replacements: Dict[str, Dict[str, str]] = {
    "src/platforms/apollo3/clockless_apollo3.h": {
        '#include "../../fastled_delay.h"': '#include "fastled_delay.h"',
    },
    "src/platforms/arm/d21/clockless_arm_d21.h": {
        '#include "../common/m0clockless.h"': '#include "platforms/arm/common/m0clockless.h"',
    },
    "src/platforms/arm/d51/fastled_arm_d51.h": {
        '#include "../../fastspi_arduino_core.h"': '#include "fastspi_arduino_core.h"',
    },
    "src/platforms/arm/giga/fastled_arm_giga.h": {
        '#include "../../fastspi_arduino_core.h"': '#include "fastspi_arduino_core.h"',
    },
    "src/platforms/arm/k20/octows2811_controller.h": {
        '#include "../../../bitswap.h"': '#include "bitswap.h"',
    },
    "src/platforms/arm/kl26/clockless_arm_kl26.h": {
        '#include "../common/m0clockless.h"': '#include "platforms/arm/common/m0clockless.h"',
    },
    "src/platforms/arm/nrf51/clockless_arm_nrf51.h": {
        '#include "../common/m0clockless.h"': '#include "platforms/arm/common/m0clockless.h"',
    },
    "src/platforms/arm/renesas/fastled_arm_renesas.h": {
        '#include "../../fastspi_arduino_core.h"': '#include "fastspi_arduino_core.h"',
    },
    "src/platforms/arm/rp/rp2040/clockless_arm_rp2040.h": {
        '#include "../rpcommon/clockless_rp_pio.h"': '#include "platforms/arm/rp/rpcommon/clockless_rp_pio.h"',
        '#include "../rpcommon/clockless_rp_pio_parallel.h"': '#include "platforms/arm/rp/rpcommon/clockless_rp_pio_parallel.h"',
    },
    "src/platforms/arm/rp/rp2040/led_sysdefs_arm_rp2040.h": {
        '#include "../rpcommon/led_sysdefs_rp_common.h"': '#include "platforms/arm/rp/rpcommon/led_sysdefs_rp_common.h"',
    },
    "src/platforms/arm/rp/rp2350/clockless_arm_rp2350.h": {
        '#include "../rpcommon/clockless_rp_pio.h"': '#include "platforms/arm/rp/rpcommon/clockless_rp_pio.h"',
        '#include "../rpcommon/clockless_rp_pio_parallel.h"': '#include "platforms/arm/rp/rpcommon/clockless_rp_pio_parallel.h"',
    },
    "src/platforms/arm/rp/rp2350/led_sysdefs_arm_rp2350.h": {
        '#include "../rpcommon/led_sysdefs_rp_common.h"': '#include "platforms/arm/rp/rpcommon/led_sysdefs_rp_common.h"',
    },
    "src/platforms/arm/rp/rpcommon/clockless_rp_pio.h": {
        '#include "../../common/m0clockless.h"': '#include "platforms/arm/common/m0clockless.h"',
    },
    "src/platforms/arm/sam/fastspi_arm_sam.h": {
        '#include "../../../fastspi_types.h"': '#include "fastspi_types.h"',
    },
    "src/platforms/arm/teensy/teensy31_32/octows2811_controller.h": {
        '#include "../../../../bitswap.h"': '#include "bitswap.h"',
    },
    "src/platforms/arm/teensy/teensy36/fastled_arm_k66.h": {
        '#include "../teensy31_32/octows2811_controller.h"': '#include "platforms/arm/teensy/teensy31_32/octows2811_controller.h"',
        '#include "../teensy31_32/ws2812serial_controller.h"': '#include "platforms/arm/teensy/teensy31_32/ws2812serial_controller.h"',
        '#include "../teensy31_32/smartmatrix_t3.h"': '#include "platforms/arm/teensy/teensy31_32/smartmatrix_t3.h"',
    },
    "src/platforms/arm/teensy/teensy3_common/spi_output_template.h": {
        '#include "../teensy31_32/fastspi_arm_k20.h"': '#include "platforms/arm/teensy/teensy31_32/fastspi_arm_k20.h"',
    },
    "src/platforms/arm/teensy/teensy4_common/fastled_arm_mxrt1062.h": {
        '#include "../teensy31_32/ws2812serial_controller.h"': '#include "platforms/arm/teensy/teensy31_32/ws2812serial_controller.h"',
        '#include "../teensy31_32/smartmatrix_t3.h"': '#include "platforms/arm/teensy/teensy31_32/smartmatrix_t3.h"',
    },
    "src/platforms/arm/teensy/teensy_lc/clockless_arm_kl26.h": {
        '#include "../../common/m0clockless.h"': '#include "platforms/arm/common/m0clockless.h"',
        '#include "../../../../fastled_delay.h"': '#include "fastled_delay.h"',
    },
    "src/platforms/arm/teensy/teensy_lc/fastled_arm_kl26.h": {
        '#include "../teensy31_32/ws2812serial_controller.h"': '#include "platforms/arm/teensy/teensy31_32/ws2812serial_controller.h"',
    },
    "src/platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h": {
        '#include "../../../../fastspi_types.h"': '#include "fastspi_types.h"',
        '#include "../../../../fastled_delay.h"': '#include "fastled_delay.h"',
    },
    "src/platforms/avr/clockless_trinket.h": {
        '#include "../../controller.h"': '#include "controller.h"',
        '#include "../../lib8tion.h"': '#include "lib8tion.h"',
        '#include "../../fl/force_inline.h"': '#include "fl/force_inline.h"',
        '#include "../../fl/chipsets/led_timing.h"': '#include "fl/chipsets/led_timing.h"',
        '#include "../../fl/chipsets/timing_traits.h"': '#include "fl/chipsets/timing_traits.h"',
        '#include "../../fastled_delay.h"': '#include "fastled_delay.h"',
    },
    "src/platforms/esp/int.h": {
        '#include "../esp/esp_version.h"': '#include "platforms/esp/esp_version.h"',
    },
    "src/platforms/intmap.h": {
        '#include "../lib8tion/lib8static.h"': '#include "lib8tion/lib8static.h"',
    },
    "src/third_party/libhelix_mp3/real/coder.h": {
        '#include "../pub/mp3common.h"': '#include "third_party/libhelix_mp3/pub/mp3common.h"',
    },
    "src/third_party/libnsgif/src/gif.cpp": {
        '#include "../include/nsgif.hpp"': '#include "third_party/libnsgif/include/nsgif.hpp"',
    },
}

total_fixes = 0

# Fix fl files
for file in fl_files:
    total_fixes += fix_file(Path(file), fl_replacements)

# Fix platform files
for file, replacements in platform_replacements.items():
    total_fixes += fix_file(Path(file), replacements)

print(f"\nTotal fixes: {total_fixes}")
