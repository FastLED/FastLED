// Copyright 2020 Kenny Peng
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <math.h>
#include <string.h>
#include "cq_kernel.h"

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif


void _generate_guassian(kiss_fft_scalar window[], int N);

void _generate_center_freqs(float freq[], int bands, float fmin, float fmax){
    float m = log(fmax/fmin);
    for(int i = 0; i < bands; i++) freq[i] = fmin*exp(m*i/(bands-1));
}

void _generate_hamming(kiss_fft_scalar window[], int N){
    float a0 = 0.54;
    for(int i = 0; i < N; i++){
        #ifdef FIXED_POINT  // If fixed_point, represent hamming window with integers
        window[i] = SAMP_MAX*(a0-(1-a0)*cos(2*M_PI*i/(N-1)));
        #else               // Else if floating point, represent hamming window as-is
        window[i] = a0-(1-a0)*cos(2*M_PI*i/(N-1));
        #endif
    }
}

void _generate_guassian(kiss_fft_scalar window[], int N){
    float sigma = 0.5; // makes a window accurate to -30dB from peak, but smaller sigma is more accurate
    for(int i = 0; i < N; i++){
        #ifdef FIXED_POINT  // If fixed_point, represent window with integers
        window[i] = SAMP_MAX*exp(-0.5*pow((i-N/2.0)/(sigma*N/2.0), 2));
        #else               // Else if floating point, represent window as-is
        window[i] = exp(-0.5*pow((i-N/2.0)/(sigma*N/2.0), 2));
        #endif
    }
}

void _generate_kernel(kiss_fft_cpx kernel[], kiss_fftr_cfg cfg, enum window_type window_type, float f, float fmin, float fs, int N){
    // Generates window in the center and zero everywhere else
    float factor = f/fmin;
    int N_window = N/factor; // Scales inversely with frequency (see CQT paper)
    kiss_fft_scalar *time_K = (kiss_fft_scalar*)calloc(N, sizeof(kiss_fft_scalar));

    switch(window_type){
        case HAMMING:
            _generate_hamming(&time_K[(N-N_window)/2], N_window);
            break;
        case GAUSSIAN:
            _generate_guassian(&time_K[(N-N_window)/2], N_window);
            break;
    }

    // Fills window with f Hz wave sampled at fs Hz
    for(int i = 0; i < N; i++) time_K[i] *= cos(2*M_PI*(f/fs)*(i-N/2));

    #ifdef FIXED_POINT // If using fixed point, just scale inversely to N after FFT (don't normalize)
    kiss_fftr(cfg, time_K, kernel); // Outputs garbage for Q31
    for(int i = 0; i < N; i++){
        kernel[i].r *= factor;
        kernel[i].i *= factor;
    }
    #else // Else if floating point, follow CQT paper more exactly (normalize with N before FFT)
    for(int i = 0; i < N; i++) time_K[i] /= N_window;
    kiss_fftr(cfg, time_K, kernel);
    #endif

    free(time_K);
}

kiss_fft_scalar _mag(kiss_fft_cpx x){
    return sqrt(x.r*x.r+x.i*x.i);
}

struct sparse_arr* generate_kernels(struct cq_kernel_cfg cfg){
    float *freq = (float*)malloc(cfg.bands * sizeof(float));
    _generate_center_freqs(freq, cfg.bands, cfg.fmin, cfg.fmax);

    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(cfg.samples, 0, NULL, NULL);
    struct sparse_arr* kernels = (struct sparse_arr*)malloc(cfg.bands*sizeof(struct sparse_arr));
    kiss_fft_cpx *temp_kernel = (kiss_fft_cpx*)malloc(cfg.samples*sizeof(kiss_fft_cpx));

    for(int i = 0; i < cfg.bands; i++){
        // Clears temp_kernel before calling _generate_kernel on it
        for(int i = 0; i < cfg.samples; i++) temp_kernel[i].r = temp_kernel[i].i = 0;

        _generate_kernel(temp_kernel, fft_cfg, cfg.window_type, freq[i], cfg.fmin, cfg.fs, cfg.samples);

        // Counts number of elements with a complex magnitude above cfg.min_val in temp_kernel
        int n_elems = 0;
        for(int j = 0; j < cfg.samples; j++) if(_mag(temp_kernel[j]) > cfg.min_val) n_elems++;

        // Generates sparse_arr holding n_elems sparse_arr_elem's
        kernels[i].n_elems = n_elems;
        kernels[i].elems = (struct sparse_arr_elem*)malloc(n_elems*sizeof(struct sparse_arr_elem));

        // Generates sparse_arr_elem's from complex values counted before
        int k = 0;
        for(int j = 0; j < cfg.samples; j++){
            if(_mag(temp_kernel[j]) > cfg.min_val){
                kernels[i].elems[k].val = temp_kernel[j];
                kernels[i].elems[k].n = j;
                k++;
            }
        }
    }

    free(fft_cfg);
    free(temp_kernel);
    free(freq);

    return kernels;
}

struct sparse_arr* reallocate_kernels(struct sparse_arr *old_ptr, struct cq_kernel_cfg cfg){
    struct sparse_arr *new_ptr = (struct sparse_arr*)malloc(cfg.bands*sizeof(struct sparse_arr));
    for(int i = 0; i < cfg.bands; i++){
        new_ptr[i].n_elems = old_ptr[i].n_elems;
        new_ptr[i].elems = (struct sparse_arr_elem*)malloc(old_ptr[i].n_elems*sizeof(struct sparse_arr_elem));
        memcpy(new_ptr[i].elems, old_ptr[i].elems, old_ptr[i].n_elems*sizeof(struct sparse_arr_elem));
        free(old_ptr[i].elems);
    }
    free(old_ptr);
    return new_ptr;
}

void apply_kernels(kiss_fft_cpx fft[], kiss_fft_cpx cq[], struct sparse_arr kernels[], struct cq_kernel_cfg cfg){
    for(int i = 0; i < cfg.bands; i++){
        for(int j = 0; j < kernels[i].n_elems; j++){
            kiss_fft_cpx weighted_val;
            C_MUL(weighted_val, fft[kernels[i].elems[j].n], kernels[i].elems[j].val);
            C_ADDTO(cq[i], weighted_val);
        }
    }
}

void free_kernels(struct sparse_arr *kernels, struct cq_kernel_cfg cfg){
    for(int i = 0; i < cfg.bands; i++) free(kernels[i].elems);
    free(kernels);
} 
