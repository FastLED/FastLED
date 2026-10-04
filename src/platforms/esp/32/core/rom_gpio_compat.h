#pragma once
// IWYU pragma: private
// ok no namespace fl

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION_4_OR_HIGHER
#include "esp_rom_gpio.h"
#else
// IWYU pragma: begin_keep
#include "rom/gpio.h"
// IWYU pragma: end_keep
#define esp_rom_gpio_connect_out_signal gpio_matrix_out
#define esp_rom_gpio_connect_in_signal gpio_matrix_in
#define esp_rom_gpio_pad_select_gpio gpio_pad_select_gpio
#endif
