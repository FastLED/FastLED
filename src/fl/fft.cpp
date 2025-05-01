/// #include <Arduino.h>
// #include <iostream>
// #include "audio_types.h"
// // #include "defs.h"
// #include "thirdparty/cq_kernel/cq_kernel.h"
// #include "thirdparty/cq_kernel/kiss_fftr.h"
// #include "util.h"

#define FASTLED_INTERNAL 1

#include "FastLED.h"

#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

#include "fl/warn.h"
#include "fl/fft.h"
#include "fl/vector.h"
#include "fl/unused.h"

// #define SAMPLES IS2_AUDIO_BUFFER_LEN
#define AUDIO_SAMPLE_RATE 44100
#define SAMPLES 512
#define BANDS 16
#define SAMPLING_FREQUENCY AUDIO_SAMPLE_RATE
#define MAX_FREQUENCY 4698.3
#define MIN_FREQUENCY 174.6
#define MIN_VAL 5000 // Equivalent to 0.15 in Q15

#define PRINT_HEADER 0

namespace fl {
namespace {
kiss_fftr_cfg g_fftr_cfg = nullptr;
cq_kernels_t g_kernels;

cq_kernel_cfg cq_kernel_cfgcq_kernel_cfg_make() {
  cq_kernel_cfg out;
  memset(&out, 0, sizeof(cq_kernel_cfg));
  out.samples = SAMPLES;
  out.bands = BANDS;
  out.fmin = MIN_FREQUENCY;
  out.fmax = MAX_FREQUENCY;
  out.fs = AUDIO_SAMPLE_RATE;
  out.min_val = MIN_VAL;
  return out;
}

cq_kernel_cfg g_cq_cfg = cq_kernel_cfgcq_kernel_cfg_make();

bool g_is_fft_initialized = false;

void init_once() {
    if (g_is_fft_initialized) {
        return;
    }
    FASTLED_WARN("Initializing FFT");
    uint32_t start = millis();
    g_is_fft_initialized = true;
    g_fftr_cfg = kiss_fftr_alloc(SAMPLES, 0, NULL, NULL);
    cq_kernels_t kernels = generate_kernels(g_cq_cfg);
    g_kernels = generate_kernels(g_cq_cfg);
    g_kernels = reallocate_kernels(g_kernels, g_cq_cfg); // optional
    uint32_t diff = millis() - start;

    FASTLED_WARN("FFT initialized in " << diff << "ms");
    FASTLED_WARN("kernel: " << kernels);
}

char pixelBrightnessToChar(float value, float min_value, float max_value) {
    // Ensure the input is in the expected range
    if (value < min_value)
        value = min_value;
    if (value > max_value)
        value = max_value;

    const char *gradation = " .:-=+*%@#";
    size_t levels = strlen(gradation);

    // Determine the index into the gradation string
    size_t index =
        (size_t)((value - min_value) / (max_value - min_value) * (levels - 1));

    return gradation[index];
}

} // namespace

void fft_init() { init_once(); }

bool fft_is_initialized() { return g_is_fft_initialized; }


void fft_unit_test(const fft_audio_buffer_t &buffer) {
    uint32_t start = millis();
    // std::cout << "Performing FFT" << std::endl;

    init_once();
    kiss_fft_cpx fft[SAMPLES] = {};
    kiss_fftr(g_fftr_cfg, buffer, fft);
    kiss_fft_cpx cq[BANDS] = {};
    apply_kernels(fft, cq, g_kernels, g_cq_cfg);
    uint32_t diff = millis() - start;
    FASTLED_UNUSED(diff);

    float delta_f = (MAX_FREQUENCY - MIN_FREQUENCY) / BANDS;

    char output_str[2048]; // Assuming this size is sufficient. Adjust as
                           // necessary.
    int offset = 0;
    // offset += snprintf(output_str + offset, sizeof(output_str) - offset, "FFT
    // took %u ms. FFT output: ", diff);

#if PRINT_HEADER
    static int64_t s_frame = 0;
    int64_t frame = s_frame;
    s_frame++;
    if (frame % 100 == 0) {
        // print header
        for (int i = 0; i < BANDS; ++i) {
            float f_start = MIN_FREQUENCY + i * delta_f;
            float f_end = f_start + delta_f;
            offset += snprintf(output_str + offset, sizeof(output_str) - offset,
                               "%.2fHz-%.2fHz, ", f_start, f_end);
        }
        offset +=
            snprintf(output_str + offset, sizeof(output_str) - offset, "\n");
    }
#endif

    // process output here
    for (int i = 0; i < BANDS; ++i) {
        int32_t real = cq[i].r;
        int32_t imag = cq[i].i;
        float r2 = float(real * real);
        float i2 = float(imag * imag);
        float magnitude = sqrt(r2 + i2);
        float magnitude_db = 20 * log10(magnitude);
        float f_start = MIN_FREQUENCY + i * delta_f;
        float f_end = f_start + delta_f;
        FASTLED_UNUSED(f_start);
        FASTLED_UNUSED(f_end);

        if (magnitude <= 0.0f) {
            magnitude_db = 0.0f;
        }
        char pixel = pixelBrightnessToChar(magnitude_db, 0.0f, 50.0f);
        // offset += snprintf(output_str + offset, sizeof(output_str) - offset,
        //                     "%05.2f, ", magnitude_db);
        offset += snprintf(output_str + offset, sizeof(output_str) - offset,
                           "%c%c", pixel, pixel);
    }
    // std::cout << output_str << std::endl
    FASTLED_WARN(output_str);
}

#if 0
// Ensure you free resources properly
void cleanup_fft_resources() {
  free(g_fftr_cfg);
  free_kernels(g_kernels, g_cq_cfg);
}
#endif

} // namespace fl
