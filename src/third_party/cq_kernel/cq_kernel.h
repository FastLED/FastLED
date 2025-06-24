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

#ifndef CQ_KERNEL_H
#define CQ_KERNEL_H

#include <math.h>
#include "_kiss_fft_guts.h"
#include "kiss_fftr.h"

#ifdef __cplusplus
extern "C" {
#endif

enum window_type{
    HAMMING,
    GAUSSIAN
};

/*
 *  cq_kernel_cfg - holds parameters used by cq_kernel
 *  
 *  Initialize with desired parameters and pass into cq_kernel functions
 * */
struct cq_kernel_cfg{
    int samples;                // kiss_fftr requires this must be even
    int bands;
    float fmin;
    float fmax;
    float fs;
    enum window_type window_type;
    kiss_fft_scalar min_val;    // sparse matrix threshold, see CQT paper
};

/*
 *  sparse_arr_elem - holds one position and complex value pair in sparse array
 *  sparse_arr - holds length and pointer of sparse array
 * */
struct sparse_arr_elem{
    int n;
    kiss_fft_cpx val;
};
struct sparse_arr{
    int n_elems;
    struct sparse_arr_elem *elems;
};

typedef struct sparse_arr *cq_kernels_t;

// private functions
void _generate_center_freqs(float freq[], int bands, float fmin, float fmax);
void _generate_hamming(kiss_fft_scalar window[], int N);
void _generate_gaussian(kiss_fft_scalar window[], int N);
void _generate_kernel(kiss_fft_cpx K[], kiss_fftr_cfg cfg, enum window_type window_type, float f, float fmin, float fs, int N);
kiss_fft_scalar _mag(kiss_fft_cpx x);

// public functions
/*
 *  generate_kernels - generates constant q kernels from cfg
 *  
 *  typical usage: cq_kernels_t kernels = generate_kernels(cfg);
 * 
 *  Beware massive memory usage, uses more than 3*cfg.samples*sizeof(kiss_fft_scalar) bytes at a time
 *  to calculate all of the kernels. However, it will return sparse arrays that use much less memory.
 * */
struct sparse_arr* generate_kernels(struct cq_kernel_cfg cfg);

/*
 *  reallocate_kernels - (optional) reallocates the kernels
 * 
 *  typical usage: kernels = reallocate_kernels(kernels, cfg);
 * 
 *  This can be used to eliminate the hole in heap left by generate_kernels
 * */
struct sparse_arr* reallocate_kernels(struct sparse_arr* kernels, struct cq_kernel_cfg cfg);

/*
 *  apply_kernels - calculates constant q from output of kiss_fft
 * 
 *  typical usage:
 *  kiss_fft_cpx fft[SAMPLES] = {0};
 *  kiss_fftr(fft_cfg, in, fft);
 *  kiss_fft_cpx cq[BANDS] = {0};
 *  apply_kernels(fft, cq, kernels, cfg);
 * */
void apply_kernels(kiss_fft_cpx fft[], kiss_fft_cpx cq[], struct sparse_arr kernels[], struct cq_kernel_cfg cfg);

/*
 *  free_kernels - frees memory used by the sparse arrays of kernels
 * 
 *  typical_usage: free_kernels(kernels, cfg);
 * */
void free_kernels(struct sparse_arr *kernels, struct cq_kernel_cfg cfg);

#ifdef __cplusplus
} 
#endif

#endif