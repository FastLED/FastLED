/**
 * @file simplewebp.h
 * @author Google Inc., Miku AuahDark
 * @brief A simple WebP decoder.
 * @version 20231226
 * See license at the bottom of the file.
 */

#ifndef _SIMPLE_WEBP_H_
#define _SIMPLE_WEBP_H_

#include <stdlib.h>
#include "fl/stdint.h"

#define SIMPLEWEBP_VERSION 20231226

/**
 * @brief SimpleWebP "input stream".
 * 
 * For user-defined "input stream", populate the struct accordingly.
 * This struct can be allocated on stack, and is assumed so.
 */
typedef struct simplewebp_input
{
	/**
	 * @brief Function-specific userdata.
	 */
	void *userdata;

	/**
	 * @brief Function to close **and** deallocate `userdata`.
	 * @param userdata Function-specific userdata.
	 */
	void (*close)(void *userdata);

	/**
	 * @brief Function to read from "input stream"
	 * @param size Amount of bytes to read.
	 * @param dest Output buffer.
	 * @param userdata Function-specific userdata.
	 * @return Amount of bytes read.
	 */
	size_t (*read)(size_t size, void *dest, void *userdata);

	/**
	 * @brief Function to seek "input stream". Stream must support seeking!
	 * @param pos New position of the stream, based on the beginning of the file.
	 * @param userdata Function-specific userdata.
	 * @return Non-zero on success, zero on failure.
	 */
	int32_t (*seek)(size_t pos, void *userdata);

	/**
	 * @brief Function to get current "input stream" position. Stream must support this!
	 * @return Stream position relative to the beginning.
	 */
	size_t (*tell)(void *userdata);
} simplewebp_input;

/**
 * @brief SimpleWebP allocator structure.
 * 
 * SimpleWebP functions that needs an allocation need to pass this structure.
 * This struct can be allocated on stack.
 */
typedef struct simplewebp_allocator
{
	/**
	 * @brief Allocate block of memory.
	 * @param size Amount of bytes to allocate.
	 * @return Pointer to tbe memory block, or `NULL` on failure.
	 */
	void *(*alloc)(size_t size);

	/**
	 * @brief Free allocated memory.
	 * @param mem Valid pointer to the memory allocated by `alloc` function.
	 * @note Passing `NULL` is undefined behavior, although most implementation treat it as no-op.
	 */
	void (*free)(void *mem);
} simplewebp_allocator;

/**
 * @brief SimpleWebP opaque handle.
 */
typedef struct simplewebp simplewebp;

typedef enum simplewebp_error
{
	/** No error */
	SIMPLEWEBP_NO_ERROR = 0,
	/** Failed to allocate memory */
	SIMPLEWEBP_ALLOC_ERROR,
	/** Input read error */
	SIMPLEWEBP_IO_ERROR,
	/** Not a WebP image */
	SIMPLEWEBP_NOT_WEBP_ERROR,
	/** WebP image corrupt */
	SIMPLEWEBP_CORRUPT_ERROR,
	/** WebP image unsupported */
	SIMPLEWEBP_UNSUPPORTED_ERROR,
	/** WebP image is lossless */
	SIMPLEWEBP_IS_LOSSLESS_ERROR
} simplewebp_error;

/**
 * @brief Get runtime SimpleWebP version.
 * In most cases, this should be equivalent to `SIMPLEWEBP_VERSION`
 * 
 * @return SimpleWebP numeric version.
 */
size_t simplewebp_version(void);
/**
 * @brief Get error message associated by SimpleWebP error code.
 * 
 * @param error Error code.
 * @return Error message as null-terminated string.
 */
const char *simplewebp_get_error_text(simplewebp_error error);

/**
 * @brief Initialize `simplewebp_input` structure to load from memory.
 * 
 * @param data Pointer to the WebP-encoded image in memory.
 * @param size Size of `data` in bytes.
 * @param out Destination structure.
 * @param allocator Allocator structure, or `NULL` to use C default.
 * @return Error codes.
 */
simplewebp_error simplewebp_input_from_memory(void *data, size_t size, simplewebp_input *out, const simplewebp_allocator *allocator);
/**
 * @brief Close `simplewebp_input`, freeing the `userdata` and setting its `userdata` to `NULL`.
 * @param input Pointer to the `simplewebp_input` structure.
 * @note Application should not use the same `simplewebp_input` unless re-initialized again.
 */
void simplewebp_close_input(simplewebp_input *input);

/**
 * @brief Load a WebP image.
 * 
 * @param input Pointer to the `simplewebp_input` structure. This function will take the ownership of the `simplewebp_input`.
 * @param allocator Allocator structure, or `NULL` to use C default.
 * @param out Pointer to the `simplewebp` opaque handle.
 * @return Error codes. 
 */
simplewebp_error simplewebp_load(simplewebp_input *input, const simplewebp_allocator *allocator, simplewebp **out);

/**
 * @brief Load a WebP image from memory.
 * 
 * @param data Pointer to the WebP-encoded image in memory.
 * @param size Size of `data` in bytes.
 * @param allocator Allocator structure, or `NULL` to use C default.
 * @param out Pointer to the `simplewebp` opaque handle.
 * @return Error codes. 
 */
simplewebp_error simplewebp_load_from_memory(void *data, size_t size, const simplewebp_allocator *allocator, simplewebp **out);

/**
 * @brief Frees and unload associated memory and closes the "input stream".
 * @param simplewebp `simplewebp` opaque handle.
 */
void simplewebp_unload(simplewebp *simplewebp);

/**
 * @brief Get WebP image dimensions.
 * @param simplewebp `simplewebp` opaque handle.
 * @param width Where to store the image width.
 * @param height Where to store the image height.
 */
void simplewebp_get_dimensions(simplewebp *simplewebp, size_t *width, size_t *height);

/**
 * @brief Check if the WebP image is lossless or lossy.
 * @param simplewebp `simplewebp` opaque handle.
 * @return 1 if lossless, 0 if lossy.
 */
int32_t simplewebp_is_lossless(simplewebp *simplewebp);

/**
 * @brief Decode WebP image to raw RGBA8 pixels data.
 * @param simplewebp `simplewebp` opaque handle.
 * @param buffer Block of memory with size of `width * height * 4` bytes. This is where the RGBA is stored.
 * @param settings Ignored. Set this to `NULL`.
 * @return Error codes. 
 */
simplewebp_error simplewebp_decode(simplewebp *simplewebp, void *buffer, void *settings);

/**
 * @brief Decode WebP image to raw planar YUVA420 pixels data. Can only be used on lossy WebP image.
 * @param simplewebp `simplewebp` opaque handle.
 * @param y_buffer Block of memory with size of `width * height` bytes.
 * @param u_buffer Block of memory with size of `(width + 1) / 2 * (height + 1) / 2` bytes.
 * @param v_buffer Block of memory with size of `(width + 1) / 2 * (height + 1) / 2` bytes.
 * @param a_buffer Block of memory with size of `width * height` bytes.
 * @param settings Ignored. Set this to `NULL`.
 * @return simplewebp_error Error codes.
 */
simplewebp_error simplewebp_decode_yuva(simplewebp *simplewebp, void *y_buffer, void *u_buffer, void *v_buffer, void *a_buffer, void *settings);

#ifndef SIMPLEWEBP_DISABLE_STDIO
#include <stdio.h>

/**
 * @brief Initialize `simplewebp_input` structure to load from `FILE*`.
 * 
 * @param file C stdio `FILE*`.
 * @param out Destination structure.
 * @return Error codes.
 */
simplewebp_error simplewebp_input_from_file(FILE *file, simplewebp_input *out);

/**
 * @brief Initialize `simplewebp_input` structure to load from file path using `fopen`.
 * 
 * @param filename Path to file.
 * @param out Destination structure.
 * @return Error codes.
 */
simplewebp_error simplewebp_input_from_filename(const char *filename, simplewebp_input *out);

/**
 * @brief Load a WebP image from `FILE*`.
 * 
 * @param file C stdio `FILE*`.
 * @param allocator Allocator structure, or `NULL` to use C default.
 * @param out Pointer to the `simplewebp` opaque handle.
 * @return Error codes. 
 */
simplewebp_error simplewebp_load_from_file(FILE *file, const simplewebp_allocator *allocator, simplewebp **out);

/**
 * @brief Load a WebP image from file path using `fopen`.
 * 
 * @param filename Path to file.
 * @param allocator Allocator structure, or `NULL` to use C default.
 * @param out Pointer to the `simplewebp` opaque handle.
 * @return Error codes. 
 */
simplewebp_error simplewebp_load_from_filename(const char *filename, const simplewebp_allocator *allocator, simplewebp **out);
#endif /* SIMPLEWEBP_DISABLE_STDIO */


#endif /* _SIMPLE_WEBP_H_ */

#ifdef SIMPLEWEBP_IMPLEMENTATION

#include <assert.h>
#include <string.h>

struct simplewebp__picture_header
{
	uint16_t width, height;
	uint8_t xscale, yscale, colorspace, clamp_type;
};

struct simplewebp__finfo
{
	uint8_t limit, ilevel, inner, hev_thresh;
};

struct simplewebp__topsmp
{
	uint8_t y[16], u[8], v[8];
};

struct simplewebp__mblock
{
	uint8_t nz, nz_dc;
};

struct simplewebp__mblockdata
{
	int16_t coeffs[384];
	uint32_t nonzero_y, nonzero_uv;
	uint8_t imodes[16], is_i4x4, uvmode, dither, skip, segment;
};

typedef uint8_t simplewebp__probarray[11];

struct simplewebp__bandprobas
{
	simplewebp__probarray probas[3];
};

struct simplewebp__proba
{
	uint8_t segments[3];
	struct simplewebp__bandprobas bands[4][8];
	const struct simplewebp__bandprobas *bands_ptr[4][17];
};

struct simplewebp__frame_header
{
	uint8_t key_frame, profile, show;
	uint32_t partition_length;
};

struct simplewebp__filter_header
{
	uint8_t simple, level, sharpness, use_lf_delta;
	int32_t ref_lf_delta[4], mode_lf_delta[4];
};

struct simplewebp__segment_header
{
	uint8_t use_segment, update_map, absolute_delta;
	char quantizer[4], filter_strength[4];
};

struct simplewebp__random
{
	int32_t index1, index2, amp;
	uint32_t tab[55];
};

/* Bit reader and boolean decoder */
struct simplewebp__bitread
{
	const uint8_t *buf, *buf_end, *buf_max;
	uint32_t value;
	uint8_t range, eof;
	int8_t bits;
};

typedef int32_t simplewebp__quant_t[2];
struct simplewebp__quantmat
{
	simplewebp__quant_t y1_mat, y2_mat, uv_mat;
	int32_t uv_quant, dither;
};

struct simplewebp__alpha_decoder
{
	int32_t method;
	uint8_t filter_type, use_8b_decode;
};

struct simplewebp__vp8_decoder
{
	uint8_t ready;

	struct simplewebp__bitread br;
	struct simplewebp__frame_header frame_header;
	struct simplewebp__picture_header picture_header;
	struct simplewebp__filter_header filter_header;
	struct simplewebp__segment_header segment_header;

	int32_t mb_w, mb_h;
	int32_t tl_mb_x, tl_mb_y;
	int32_t br_mb_x, br_mb_y;

	uint32_t nparts_minus_1;
	struct simplewebp__bitread parts[8];

	int32_t dither;
	struct simplewebp__random dither_rng;

	struct simplewebp__quantmat dqm[4];

	struct simplewebp__proba proba;
	uint8_t use_skip_proba, skip_proba;

	uint8_t *intra_t, intra_l[4];

	struct simplewebp__topsmp *yuv_t;

	struct simplewebp__mblock *mb_info;
	struct simplewebp__finfo *f_info;
	uint8_t *yuv_b;

	
	uint8_t *cache_y, *cache_u, *cache_v;
	int32_t cache_y_stride, cache_uv_stride;

	
	uint8_t* mem;
	size_t mem_size;

	int32_t mb_x, mb_y;
	struct simplewebp__mblockdata *mb_data;

	char filter_type;
	struct simplewebp__finfo fstrengths[4][2];

	struct simplewebp__alpha_decoder *alpha_decoder;
	const uint8_t *alpha_data;
	size_t alpha_data_size;
	int32_t is_alpha_decoded;
	uint8_t *alpha_plane_mem, *alpha_plane;
	const uint8_t *alpha_prev_line;
	int32_t alpha_dithering;
};

static char simplewebp__longlong_must_64bit[sizeof(uint64_t) == 8 ? 1 : -1];

struct simplewebp__vp8l_bitread
{
	uint64_t val;
	uint8_t *buf;
	size_t len, pos;
	int32_t bit_pos;
	uint8_t eos;
};

struct simplewebp__vp8l_color_cache
{
	uint32_t *colors;
	int32_t hash_shift, hash_bits;
};

struct simplewebp__huffman_code
{
	uint8_t bits;
	uint16_t value;
};

struct simplewebp__huffman_code_32
{
	int32_t bits;
	uint32_t value;
};

struct simplewebp__htree_group
{
	struct simplewebp__huffman_code *htrees[5];
	char is_trivial_literal, is_trivial_code, use_packed_table;
	uint32_t literal_arb;
	struct simplewebp__huffman_code_32 packed_table[64];
};

struct simplewebp__huffman_tables_segment
{
	struct simplewebp__huffman_code *start, *current;
	struct simplewebp__huffman_tables_segment *next;
	int32_t size;
};

struct simplewebp__huffman_tables
{
	struct simplewebp__huffman_tables_segment root, *current;
};

struct simplewebp__vp8l_metadata
{
	int32_t color_cache_size;
	struct simplewebp__vp8l_color_cache color_cache, saved_color_cache;

	int32_t huffman_mask, huffman_subsample_bits, huffman_xsize;
	uint32_t *huffman_image;
	int32_t hum_htree_groups;
	struct simplewebp__htree_group *htree_groups;
	struct simplewebp__huffman_tables huffman_tables;
};

struct simplewebp__vp8l_transform
{
	/* Type is: predictor (0), x-color (1), subgreen (2), and colorindex (3) */
	int32_t type;
	int32_t bits, xsize, ysize;
	uint32_t *data;
};

struct simplewebp__rescaler
{
	uint8_t x_expand, y_expand;
	int32_t num_channels;
	uint32_t fx_scale;
	uint32_t fy_scale;
	uint32_t fxy_scale;
	int32_t y_accum;
	int32_t y_add, y_sub;
	int32_t x_add, x_sub;
	int32_t src_width, src_height;
	int32_t dst_width, dst_height;
	int32_t src_y, dst_y;
	uint8_t* dst;
	int32_t dst_stride;
	uint32_t *irow, *frow;
};

struct simplewebp__vp8l_decoder
{
	uint32_t *pixels, *argb_cache;
	struct simplewebp__vp8l_bitread br, saved_br;
	int32_t saved_last_pixel;

	uint32_t width, height;
	int32_t last_row, last_pixel, last_out_row;

	struct simplewebp__vp8l_metadata header;

	int32_t next_transform;
	struct simplewebp__vp8l_transform transforms[4];
	uint32_t transforms_seen;

	uint8_t *rescaler_mem;
	struct simplewebp__rescaler rescaler;
};

union simplewebp__decoder_list
{
	struct simplewebp__vp8_decoder vp8;
	struct simplewebp__vp8l_decoder vp8l;
};

struct simplewebp__yuvdst
{
	uint8_t *y, *u, *v, *a;
};

struct simplewebp
{
	simplewebp_input input, riff_input, vp8_input, vp8x_input, alph_input;
	simplewebp_allocator allocator;
	struct simplewebp__alpha_decoder alpha_decoder;

	uint8_t webp_type; /* Simple lossy (0), Lossless (1) */
	union simplewebp__decoder_list decoder;
};

struct simplewebp_memoryinput_data
{
	simplewebp_allocator allocator;
	void *data;
	size_t size, pos;
};

static simplewebp_allocator simplewebp_default_allocator = {malloc, free};

size_t simplewebp_version(void)
{
	return SIMPLEWEBP_VERSION;
}

const char *simplewebp_get_error_text(simplewebp_error error)
{
	switch (error)
	{
		case SIMPLEWEBP_NO_ERROR:
			return "No error";
		case SIMPLEWEBP_ALLOC_ERROR:
			return "Failed to allocate memory";
		case SIMPLEWEBP_IO_ERROR:
			return "Input read error";
		case SIMPLEWEBP_NOT_WEBP_ERROR:
			return "Not a WebP image";
		case SIMPLEWEBP_CORRUPT_ERROR:
			return "WebP image corrupt";
		case SIMPLEWEBP_UNSUPPORTED_ERROR:
			return "WebP image unsupported";
		case SIMPLEWEBP_IS_LOSSLESS_ERROR:
			return "WebP image is lossless";
		default:
			return "Unknown error";
	}
}

static size_t simplewebp__memoryinput_read(size_t size, void *dest, void *userdata)
{
	struct simplewebp_memoryinput_data *input_data;
	size_t nextpos, readed;

	input_data = (struct simplewebp_memoryinput_data *) userdata;
	nextpos = input_data->pos + size;

	if (nextpos >= input_data->size)
	{
		readed = input_data->size - input_data->pos;
		nextpos = input_data->size;
	}
	else
		readed = size;

	if (readed > 0)
	{
		memcpy(dest, ((char *) input_data->data) + input_data->pos, readed);
		input_data->pos = nextpos;
	}

	return readed;
}

static int32_t simplewebp__memoryinput_seek(size_t pos, void *userdata)
{
	struct simplewebp_memoryinput_data *input_data = (struct simplewebp_memoryinput_data *) userdata;

	if (pos >= input_data->size)
		input_data->pos = input_data->size;
	else
		input_data->pos = pos;

	return 1;
}

static void simplewebp__memoryinput_close(void *userdata)
{
	struct simplewebp_memoryinput_data *input_data = (struct simplewebp_memoryinput_data *) userdata;
	input_data->allocator.free(input_data);
}

static size_t simplewebp__memoryinput_tell(void *userdata)
{
	struct simplewebp_memoryinput_data *input_data = (struct simplewebp_memoryinput_data *) userdata;
	return input_data->pos;
}

simplewebp_error simplewebp_input_from_memory(void *data, size_t size, simplewebp_input *out, const simplewebp_allocator *allocator)
{
	struct simplewebp_memoryinput_data *input_data;

	if (allocator == NULL)
		allocator = &simplewebp_default_allocator;

	input_data = (struct simplewebp_memoryinput_data *) allocator->alloc(sizeof(struct simplewebp_memoryinput_data));
	if (input_data == NULL)
		return SIMPLEWEBP_ALLOC_ERROR;

	input_data->allocator = *allocator;
	input_data->data = data;
	input_data->size = size;
	input_data->pos = 0;
	out->userdata = input_data;
	out->read = simplewebp__memoryinput_read;
	out->seek = simplewebp__memoryinput_seek;
	out->tell = simplewebp__memoryinput_tell;
	out->close = simplewebp__memoryinput_close;
	return SIMPLEWEBP_NO_ERROR;
}

struct simplewebp_input_proxy
{
	simplewebp_input input;
	simplewebp_allocator allocator;

	size_t start, length;
};

static int32_t simplewebp__seek(size_t pos, simplewebp_input *input)
{
	return input->seek(pos, input->userdata);
}

static size_t simplewebp__read(size_t size, void *dest, simplewebp_input *input)
{
	return input->read(size, dest, input->userdata);
}

static int32_t simplewebp__read2(size_t size, void *dest, simplewebp_input *input)
{
	return simplewebp__read(size, dest, input) == size;
}

static size_t simplewebp__tell(simplewebp_input *input)
{
	return input->tell(input->userdata);
}

static size_t simplewebp__proxy_tell(void *userdata)
{
	struct simplewebp_input_proxy *proxy;
	size_t pos;

	proxy = (struct simplewebp_input_proxy *) userdata;
	pos = simplewebp__tell(&proxy->input);

	if (pos < proxy->start)
	{
		simplewebp__seek(proxy->start, &proxy->input);
		pos = proxy->start;
	}

	return pos - proxy->start;
}

static size_t simplewebp__proxy_read(size_t size, void *dest, void *userdata)
{
	struct simplewebp_input_proxy *proxy;
	size_t pos, nextpos, readed;

	proxy = (struct simplewebp_input_proxy *) userdata;
	pos = simplewebp__proxy_tell(userdata);
	
	nextpos = pos + size;

	if (nextpos >= proxy->length)
	{
		readed = size - (proxy->length - nextpos);
		nextpos = proxy->length;
	}
	else
		readed = size;

	if (readed > 0)
		readed = simplewebp__read(readed, dest, &proxy->input);

	return readed;
}

static int32_t simplewebp__proxy_seek(size_t pos, void *userdata)
{
	struct simplewebp_input_proxy *proxy = (struct simplewebp_input_proxy *) userdata;

	if (pos > proxy->length)
		pos = proxy->length;

	return simplewebp__seek(pos + proxy->start, &proxy->input);
}

static void simplewebp__proxy_close(void *userdata)
{
	struct simplewebp_input_proxy *proxy = (struct simplewebp_input_proxy *) userdata;
	proxy->allocator.free(proxy);
}

static size_t simplewebp__proxy_size(void *userdata)
{
	struct simplewebp_input_proxy *proxy = (struct simplewebp_input_proxy *) userdata;
	return proxy->length;
}

static simplewebp_error simplewebp__proxy_create(const simplewebp_allocator *allocator, simplewebp_input *input, simplewebp_input *out, size_t start, size_t length)
{
	struct simplewebp_input_proxy *proxy = (struct simplewebp_input_proxy *) allocator->alloc(sizeof(struct simplewebp_input_proxy));
	if (proxy == NULL)
		return SIMPLEWEBP_ALLOC_ERROR;

	proxy->input = *input;
	proxy->allocator = *allocator;
	proxy->start = start;
	proxy->length = length;
	out->userdata = proxy;
	out->read = simplewebp__proxy_read;
	out->seek = simplewebp__proxy_seek;
	out->tell = simplewebp__proxy_tell;
	out->close = simplewebp__proxy_close;
	return SIMPLEWEBP_NO_ERROR;
}

void simplewebp_close_input(simplewebp_input *input)
{
	input->close(input->userdata);
	input->userdata = NULL;
}

static uint32_t simplewebp__to_uint32(const uint8_t *buf)
{
	return buf[0] | (((uint32_t) buf[1]) << 8) | (((uint32_t) buf[2]) << 16) | (((uint32_t) buf[3]) << 24);
}

static simplewebp_error simplewebp__get_input_chunk_4cc(const simplewebp_allocator *allocator, simplewebp_input *input, simplewebp_input *outproxy, void *fourcc)
{
	uint8_t size[4];
	size_t chunk_size;

	if (!simplewebp__read2(4, fourcc, input))
		return SIMPLEWEBP_IO_ERROR;
	if (!simplewebp__read2(4, size, input))
		return SIMPLEWEBP_IO_ERROR;

	chunk_size = simplewebp__to_uint32(size);
	return simplewebp__proxy_create(allocator, input, outproxy, simplewebp__tell(input), chunk_size);
}

static uint16_t simplewebp__to_uint16(const uint8_t *buf)
{
	return buf[0] | (((uint16_t) buf[1]) << 8);
}

static simplewebp_error simplewebp__load_lossy(const simplewebp_allocator *allocator, simplewebp_input *input, simplewebp_input *riff_input, simplewebp_input *vp8_input, simplewebp **out)
{
	uint8_t temp[8], profile;
	uint32_t frametag, partition_size;
	uint16_t width, height;
	simplewebp *result;

	if (!simplewebp__seek(0, vp8_input))
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	if (!simplewebp__read2(3, temp, vp8_input))
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	frametag = temp[0] | (((uint32_t) temp[1]) << 8) | (((uint32_t) temp[2]) << 16);
	if (frametag & 1)
	{
		/* Intraframe in SimpleWebP? Nope */
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_UNSUPPORTED_ERROR;
	}

	profile = (frametag >> 1) & 7;
	if (profile > 3)
	{
		/* Unsupported profile */
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_UNSUPPORTED_ERROR;
	}

	partition_size = frametag >> 5;
	if (partition_size >= simplewebp__proxy_size(vp8_input->userdata))
	{
		/* Inconsistent data */
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	if (!simplewebp__read2(7, temp, vp8_input))
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	if (memcmp(temp, "\x9D\x01\x2A", 3) != 0)
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	width = simplewebp__to_uint16(temp + 3);
	height = simplewebp__to_uint16(temp + 5);

	result = (simplewebp *) allocator->alloc(sizeof(simplewebp));
	if (result == NULL)
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_ALLOC_ERROR;
	}

	memset(result, 0, sizeof(simplewebp));
	result->input = *input;
	result->riff_input = *riff_input;
	result->vp8_input = *vp8_input;
	result->allocator = *allocator;
	result->webp_type = 0;
	memset(&result->decoder.vp8, 0, sizeof(struct simplewebp__vp8_decoder));
	result->decoder.vp8.picture_header.width = width & 0x3FFF;
	result->decoder.vp8.picture_header.height = height & 0x3FFF;
	result->decoder.vp8.picture_header.xscale = (uint8_t) (width >> 14);
	result->decoder.vp8.picture_header.yscale = (uint8_t) (height >> 14);
	result->decoder.vp8.frame_header.partition_length = partition_size;
	*out = result;

	return SIMPLEWEBP_NO_ERROR;
}

static simplewebp_error simplewebp__load_lossless(const simplewebp_allocator *allocator, simplewebp_input *input, simplewebp_input *riff_input, simplewebp_input *vp8_input, simplewebp **out)
{
	uint8_t temp[5];
	uint32_t header;
	simplewebp *result;

	if (!simplewebp__seek(0, vp8_input))
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	if (!simplewebp__read2(5, temp, vp8_input))
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	if (temp[0] != 0x2F)
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	header = simplewebp__to_uint32(temp + 1);
	if ((header >> 29) != 0)
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_UNSUPPORTED_ERROR;
	}

	result = (simplewebp *) allocator->alloc(sizeof(simplewebp));
	if (result == NULL)
	{
		simplewebp_close_input(vp8_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_ALLOC_ERROR;
	}

	memset(result, 0, sizeof(simplewebp));
	result->input = *input;
	result->riff_input = *riff_input;
	result->vp8_input = *vp8_input;
	result->allocator = *allocator;
	memset(&result->decoder.vp8l, 0, sizeof(struct simplewebp__vp8l_decoder));
	result->decoder.vp8l.width = (header & 0x3FF) + 1;
	result->decoder.vp8l.height = ((header >> 14) & 0x3FF) + 1;
	result->webp_type = 1;
	*out = result;

	return SIMPLEWEBP_NO_ERROR;
}

static uint32_t simplewebp__to_uint24(const uint8_t *buf)
{
	return buf[0] | (((uint32_t) buf[1]) << 8) | (((uint32_t) buf[2]) << 16);
}

static simplewebp_error simplewebp__alpha_init(simplewebp *simplewebp)
{
	simplewebp_input *alph_input = &simplewebp->alph_input;
	if (!simplewebp__seek(0, alph_input))
		return SIMPLEWEBP_IO_ERROR;

	/* TODO */
	return SIMPLEWEBP_NO_ERROR;
}

static simplewebp_error simplewebp__load_extended(const simplewebp_allocator *allocator, simplewebp_input *input, simplewebp_input *riff_input, simplewebp_input *vp8x_input, simplewebp **out)
{
	simplewebp *result;
	simplewebp_input alpha_input, chunk;
	simplewebp_error err;
	uint8_t temp[8], handled;
	uint32_t width, height, pwidth, pheight;

	memset(&chunk, 0, sizeof(simplewebp_input));
	memset(&alpha_input, 0, sizeof(simplewebp_input));

	if (!simplewebp__seek(0, vp8x_input))
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	if (!simplewebp__read(7, temp, vp8x_input))
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	width = simplewebp__to_uint24(temp + 4) + 1;
	if (width > 16384)
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}
	
	if (!simplewebp__read2(3, temp, vp8x_input))
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	height = simplewebp__to_uint24(temp) + 1;
	if (height > 16384)
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	/* Skip the rest of VP8X chunk */
	if (!simplewebp__seek(simplewebp__proxy_size(vp8x_input->userdata), vp8x_input))
	{
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}

	while (1)
	{
		size_t riff_pos, chunk_size;

		handled = 0;
		riff_pos = simplewebp__tell(riff_input); /* before chunk name and size */
		err = simplewebp__get_input_chunk_4cc(allocator, riff_input, &chunk, temp);
		if (err != SIMPLEWEBP_NO_ERROR)
		{
			simplewebp_close_input(vp8x_input);
			simplewebp_close_input(riff_input);
			return err;
		}

		if (memcmp(temp, "VP8 ", 4) == 0)
		{
			err = simplewebp__load_lossy(allocator, input, riff_input, &chunk, &result);
			break;
		}
		else if (memcmp(temp, "VP8L", 4) == 0)
		{
			err = simplewebp__load_lossless(allocator, input, riff_input, &chunk, &result);
			break;
		}
		else if (memcmp(temp, "ALPH", 4) == 0)
		{
			alpha_input = chunk;
			handled = 1;
		}

		/* Skip this chunk over */
		/* We can't just seek the proxy. If the chunk is odd-sized then it
		 * doesn't work (RIFF requires chunk to be aligned to word boundary) */
		chunk_size = (simplewebp__proxy_size(chunk.userdata) + 1) & (~((size_t) 1));
		if (!simplewebp__seek(riff_pos + 8 + chunk_size, riff_input))
		{
			if (alpha_input.userdata)
				simplewebp_close_input(&alpha_input);
			simplewebp_close_input(vp8x_input);
			simplewebp_close_input(riff_input);
			return SIMPLEWEBP_IO_ERROR;
		}

		if (!handled)
			/* Close this chunk if not copied over */
			simplewebp_close_input(&chunk);
	}

	/* Either VP8 or VP8L chunk has been handled */
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		if (alpha_input.userdata)
			simplewebp_close_input(&alpha_input);
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return err;
	}

	/* At this point, chunk has been handled without error */
	/* This means the "result" variable has been populated. */

	switch (result->webp_type)
	{
		case 0:
			pwidth = result->decoder.vp8.picture_header.width;
			pheight = result->decoder.vp8.picture_header.height;
			break;
		case 1:
			pwidth = result->decoder.vp8l.width;
			pheight = result->decoder.vp8l.height;
			break;
		default:
			pwidth = pheight = 0;
			break;
	}

	/* Sanity check width and height */
	if (pwidth != width || pheight != height)
	{
		if (alpha_input.userdata)
			simplewebp_close_input(&alpha_input);
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	result->vp8x_input = *vp8x_input;
	if (alpha_input.userdata)
	{
		result->alph_input = alpha_input;
		if (result->webp_type == 0)
		{
			memset(&result->alpha_decoder, 0, sizeof(struct simplewebp__alpha_decoder));
			result->decoder.vp8.alpha_decoder = &result->alpha_decoder;
			err = simplewebp__alpha_init(result);
		}
	}

	if (err != SIMPLEWEBP_NO_ERROR)
	{
		if (alpha_input.userdata)
			simplewebp_close_input(&alpha_input);
		simplewebp_close_input(vp8x_input);
		simplewebp_close_input(riff_input);
		return err;
	}

	*out = result;
	return SIMPLEWEBP_NO_ERROR;
}

simplewebp_error simplewebp_load(simplewebp_input *input, const simplewebp_allocator *allocator, simplewebp **out)
{
	uint8_t temp[4];
	simplewebp_error err;
	simplewebp_input riff_input, vp8_input;

	*out = NULL;
	if (allocator == NULL)
		allocator = &simplewebp_default_allocator;

	/* Read "RIFF" */
	err = simplewebp__get_input_chunk_4cc(allocator, input, &riff_input, temp);
	if (err != SIMPLEWEBP_NO_ERROR)
		return err;
	if (memcmp(temp, "RIFF", 4) != 0)
	{
		simplewebp_close_input(&riff_input);
		return SIMPLEWEBP_NOT_WEBP_ERROR;
	}

	/* Read "WEBP" */
	if (!simplewebp__read2(4, temp, &riff_input))
	{
		simplewebp_close_input(&riff_input);
		return SIMPLEWEBP_IO_ERROR;
	}
	if (memcmp(temp, "WEBP", 4) != 0)
	{
		simplewebp_close_input(&riff_input);
		return SIMPLEWEBP_NOT_WEBP_ERROR;
	}

	err = simplewebp__get_input_chunk_4cc(allocator, &riff_input, &vp8_input, temp);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		simplewebp_close_input(&riff_input);
		return err;
	}

	if (memcmp(temp, "VP8 ", 4) == 0)
		return simplewebp__load_lossy(allocator, input, &riff_input, &vp8_input, out);
	else if (memcmp(temp, "VP8L", 4) == 0)
		return simplewebp__load_lossless(allocator, input, &riff_input, &vp8_input, out);
	else if (memcmp(temp, "VP8X", 4) == 0)
		return simplewebp__load_extended(allocator, input, &riff_input, &vp8_input, out);
	else
	{
		simplewebp_close_input(&vp8_input);
		simplewebp_close_input(&riff_input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}
}

simplewebp_error simplewebp_load_from_memory(void *data, size_t size, const simplewebp_allocator *allocator, simplewebp **out)
{
	simplewebp_input input;
	simplewebp_error err;

	err = simplewebp_input_from_memory(data, size, &input, allocator);
	if (err != SIMPLEWEBP_NO_ERROR)
		return err;

	err = simplewebp_load(&input, allocator, out);
	if (err != SIMPLEWEBP_NO_ERROR)
		simplewebp_close_input(&input);

	return err;
}

void simplewebp_unload(simplewebp *simplewebp)
{
	if (simplewebp->alph_input.userdata != NULL)
		simplewebp_close_input(&simplewebp->alph_input);
	if (simplewebp->vp8x_input.userdata != NULL)
		simplewebp_close_input(&simplewebp->vp8x_input);
	simplewebp_close_input(&simplewebp->vp8_input);
	simplewebp_close_input(&simplewebp->riff_input);
	simplewebp_close_input(&simplewebp->input);
	simplewebp->allocator.free(simplewebp);
}

void simplewebp_get_dimensions(simplewebp *simplewebp, size_t *width, size_t *height)
{
	switch (simplewebp->webp_type)
	{
		case 0:
			*width = simplewebp->decoder.vp8.picture_header.width;
			*height = simplewebp->decoder.vp8.picture_header.height;
			break;
		case 1:
			*width = simplewebp->decoder.vp8l.width;
			*height = simplewebp->decoder.vp8l.height;
			break;
	}
}

static void simplewebp__bitread_setbuf(struct simplewebp__bitread *br, uint8_t *buf, size_t size)
{
	br->buf = buf;
	br->buf_end = buf + size;
	br->buf_max = (size >= sizeof(uint32_t))
		? (buf + size - sizeof(uint32_t) + 1)
		: buf;
}

static void simplewebp__bitread_load(struct simplewebp__bitread *br)
{
	uint32_t bits;

	if (br->buf < br->buf_max)
	{
		/* Read 24 bits at a time in big endian order */
		bits = br->buf[2] | (br->buf[1] << 8) | (br->buf[0] << 16);
		br->buf += 3; /* 24 / 8 */
		br->value = bits | (br->value << 24);
		br->bits += 24;
	}
	else
	{
		/* Only read 8 bits at a time */
		if (br->buf < br->buf_end)
		{
			br->bits += 8;
			br->value = (*br->buf++) | (br->value << 8);
		}
		else if (!br->eof)
		{
			br->value <<= 8;
			br->bits += 8;
			br->eof = 1;
		}
		else
			br->bits = 0;
	}
}

static void simplewebp__bitread_init(struct simplewebp__bitread *br, uint8_t *buf, size_t size)
{
	br->range = 254;
	br->value = 0;
	br->bits = -8;
	br->eof = 0;
	simplewebp__bitread_setbuf(br, buf, size);
	simplewebp__bitread_load(br);
}

/* https://stackoverflow.com/a/11398748 */
const uint32_t simplewebp__blog2_tab32[32] = {
	 0,  9,  1, 10, 13, 21,  2, 29,
	11, 14, 16, 18, 22, 25,  3, 30,
	 8, 12, 20, 28, 15, 17, 24,  7,
	19, 27, 23,  6, 26,  5,  4, 31
};

static uint32_t simplewebp__bitslog2floor(uint32_t value)
{
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	return simplewebp__blog2_tab32[(value * 0x07C4ACDDU) >> 27];
}

static uint32_t simplewebp__bitread_getbit(struct simplewebp__bitread *br, uint32_t prob)
{
	uint32_t bit;
	uint8_t range, split, value, shift;
	int8_t pos;

	range = br->range;

	if (br->bits < 0)
		simplewebp__bitread_load(br);

	pos = br->bits;
	split = (uint8_t) ((((uint32_t) range) * prob) >> 8);
	value = (uint8_t) (br->value >> pos);
	bit = value > split;

	if (bit)
	{
		range -= split;
		br->value -= (((uint32_t) split) + 1) << pos;
	}
	else
		range = split + 1;

	shift = 7 ^ simplewebp__bitslog2floor(range);
	range <<= shift;
	br->bits -= shift;
	br->range = range - 1;
	return bit;
}

static uint32_t simplewebp__bitread_getval(struct simplewebp__bitread *br, uint32_t bits)
{
	uint32_t value = 0;

	while (bits--)
		value |= simplewebp__bitread_getbit(br, 0x80) << bits;

	return value;
}

static int32_t simplewebp__bitread_getvalsigned(struct simplewebp__bitread *br, uint32_t bits)
{
	int32_t value = simplewebp__bitread_getval(br, bits);
	return simplewebp__bitread_getval(br, 1) ? -value : value;
}

static int32_t simplewebp__bitread_getsigned(struct simplewebp__bitread *br, int32_t v)
{
	int8_t pos;
	uint32_t split, value;
	int32_t mask;

	if (br->bits < 0)
		simplewebp__bitread_load(br);

	pos = br->bits;
	split = br->range >> 1;
	value = br->value >> pos;
	mask = ((int) (split - value)) >> 31;
	br->bits -= 1;
	br->range += (uint8_t) mask;
	br->range |= 1;
	br->value -= (uint32_t) ((split + 1) & (uint32_t) mask) << pos;
	return (v ^ mask) - mask;
}

/* RFC 6386 section 14.1 */
static const uint8_t simplewebp__dctab[128] = {
  4,     5,   6,   7,   8,   9,  10,  10,
  11,   12,  13,  14,  15,  16,  17,  17,
  18,   19,  20,  20,  21,  21,  22,  22,
  23,   23,  24,  25,  25,  26,  27,  28,
  29,   30,  31,  32,  33,  34,  35,  36,
  37,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  46,  47,  48,  49,  50,
  51,   52,  53,  54,  55,  56,  57,  58,
  59,   60,  61,  62,  63,  64,  65,  66,
  67,   68,  69,  70,  71,  72,  73,  74,
  75,   76,  76,  77,  78,  79,  80,  81,
  82,   83,  84,  85,  86,  87,  88,  89,
  91,   93,  95,  96,  98, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136,
  138, 140, 143, 145, 148, 151, 154, 157
};

static const uint16_t simplewebp__actab[128] = {
	4,     5,   6,   7,   8,   9,  10,  11,
	12,   13,  14,  15,  16,  17,  18,  19,
	20,   21,  22,  23,  24,  25,  26,  27,
	28,   29,  30,  31,  32,  33,  34,  35,
	36,   37,  38,  39,  40,  41,  42,  43,
	44,   45,  46,  47,  48,  49,  50,  51,
	52,   53,  54,  55,  56,  57,  58,  60,
	62,   64,  66,  68,  70,  72,  74,  76,
	78,   80,  82,  84,  86,  88,  90,  92,
	94,   96,  98, 100, 102, 104, 106, 108,
	110, 112, 114, 116, 119, 122, 125, 128,
	131, 134, 137, 140, 143, 146, 149, 152,
	155, 158, 161, 164, 167, 170, 173, 177,
	181, 185, 189, 193, 197, 201, 205, 209,
	213, 217, 221, 225, 229, 234, 239, 245,
	249, 254, 259, 264, 269, 274, 279, 284
};

static int32_t simplewebp__clip(int32_t v, int32_t m)
{
	return (v < 0) ? 0 : ((v > m) ? m : v);
}

/* RFC 6386 section 13 */
static const uint8_t simplewebp__coeff_update_proba[4][8][3][11] = {
	{ { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255 },
			{ 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		}
	},
	{ { { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255 },
			{ 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255 }
		},
		{ { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		}
	},
	{ { { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255 },
			{ 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255 }
		},
		{ { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		}
	},
	{ { { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255 },
			{ 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		},
		{ { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
			{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
		}
	}
};

/* RFC 6386 section 13.5 */
static const uint8_t simplewebp__coeff_proba0[4][8][3][11] = {
	{ { { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ { 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128 },
			{ 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128 },
			{ 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128 }
		},
		{ { 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128 },
			{ 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128 },
			{ 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128 },
		},
		{ { 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128 },
			{ 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128 },
			{ 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128 },
		},
		{ { 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128 },
			{ 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128 },
			{ 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128 }
		},
		{ { 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128 },
			{ 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128 },
			{ 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128 }
		},
		{ { 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128 },
			{ 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128 },
			{ 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128 }
		},
		{ { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	},
	{ { { 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62 },
			{ 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1 },
			{ 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128 }
		},
		{ { 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128 },
			{ 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128 },
			{ 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128 }
		},
		{ { 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128 },
			{ 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128 },
			{ 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128 }
		},
		{ { 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128 },
			{ 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128 },
			{ 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128 }
		},
		{ { 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128 },
			{ 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128 },
			{ 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128 }
		},
		{ { 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128 },
			{ 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128 },
			{ 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128 }
		},
		{ { 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128 },
			{ 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128 },
			{ 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128 }
		},
		{ { 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128 },
			{ 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128 }
		}
	},
	{ { { 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128 },
			{ 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128 },
			{ 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128 }
		},
		{ { 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128 },
			{ 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128 },
			{ 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128 }
		},
		{ { 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128 },
			{ 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128 },
			{ 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128 }
		},
		{ { 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128 },
			{ 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128 }
		},
		{ { 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128 },
			{ 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ { 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ { 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	},
	{ { { 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255 },
			{ 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128 },
			{ 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128 }
		},
		{ { 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128 },
			{ 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128 },
			{ 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128 }
		},
		{ { 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128 },
			{ 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128 },
			{ 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128 }
		},
		{ { 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128 },
			{ 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128 },
			{ 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128 }
		},
		{ { 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128 },
			{ 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128 },
			{ 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128 }
		},
		{ { 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128 },
			{ 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128 },
			{ 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128 }
		},
		{ { 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128 },
			{ 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128 },
			{ 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128 }
		},
		{ { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	}
};

static const uint8_t simplewebp__kbands[16 + 1] = {
	0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0
};

static const uint8_t simplewebp__fextrarows[3] = {0, 2, 8};

static uint8_t *simplewebp__align32(uint8_t *ptr)
{
	size_t uptr = (size_t) ptr;
	return (uint8_t *) ((uptr + 31) & (~(size_t)31));
}

/* Clip tables */

static const uint8_t simplewebp__abs0[255 + 255 + 1] = {
	0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4,
	0xf3, 0xf2, 0xf1, 0xf0, 0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
	0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0, 0xdf, 0xde, 0xdd, 0xdc,
	0xdb, 0xda, 0xd9, 0xd8, 0xd7, 0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xd0,
	0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4,
	0xc3, 0xc2, 0xc1, 0xc0, 0xbf, 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8,
	0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1, 0xb0, 0xaf, 0xae, 0xad, 0xac,
	0xab, 0xaa, 0xa9, 0xa8, 0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0,
	0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94,
	0x93, 0x92, 0x91, 0x90, 0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88,
	0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80, 0x7f, 0x7e, 0x7d, 0x7c,
	0x7b, 0x7a, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
	0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
	0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a, 0x59, 0x58,
	0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x4e, 0x4d, 0x4c,
	0x4b, 0x4a, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40,
	0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34,
	0x33, 0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28,
	0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c,
	0x1b, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
	0x03, 0x02, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
	0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
	0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
	0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
	0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
static const uint8_t *const simplewebp__kabs0 = &simplewebp__abs0[255];

static const uint8_t simplewebp__sclip1[1020 + 1020 + 1] = {
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
	0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
	0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
	0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
	0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
	0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
	0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
	0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f
};
static const char *const simplewebp__ksclip1 = (const char *) &simplewebp__sclip1[1020];

static const uint8_t simplewebp__sclip2[112 + 112 + 1] = {
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
	0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
};
static const char *const simplewebp__ksclip2 = (const char *) &simplewebp__sclip2[112];

static const uint8_t simplewebp__clip1[255 + 511 + 1] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
	0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
	0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
	0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
	0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
static const uint8_t *const simplewebp__kclip1 = &simplewebp__clip1[255];

/* RFC 6386 section 14.4 */

static void simplewebp__transform_wht(const int16_t *in, int16_t *out)
{
	int32_t temp[16], i;

	for (i = 0; i < 4; i++)
	{
		int32_t a0, a1, a2, a3;

		a0 = in[i] + in[i + 12];
		a1 = in[i + 4] + in[i + 8];
		a2 = in[i + 4] - in[i + 8];
		a3 = in[i] - in[i + 12];
		temp[i] = a0 + a1;
		temp[i + 4] = a3 + a2;
		temp[i + 8] = a0 - a1;
		temp[i + 12] = a3 - a2;
	}
	for (i = 0; i < 4; i++)
	{
		int32_t dc, a0, a1, a2, a3;

		dc = temp[i * 4] + 3;
		a0 = dc + temp[i * 4 + 3];
		a1 = temp[i * 4 + 1] + temp[i * 4 + 2];
		a2 = temp[i * 4 + 1] - temp[i * 4 + 2];
		a3 = dc - temp[i * 4 + 3];
		out[i * 64] = (a0 + a1) >> 3;
		out[i * 64 + 16] = (a3 + a2) >> 3;
		out[i * 64 + 32] = (a0 - a1) >> 3;
		out[i * 64 + 48] = (a3 - a2) >> 3;
	}
}

static int32_t simplewebp__mul1(int16_t a)
{
	return (((int) a * 20091) >> 16) + a;
}

static int32_t simplewebp__mul2(int16_t a)
{
	return ((int) a * 35468) >> 16;
}


static uint8_t simplewebp__clip8b(int32_t v) {
	return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

static void simplewebp__store(uint8_t *out, int32_t x, int32_t y, int32_t v)
{
	out[y * 32 + x] = simplewebp__clip8b(out[y * 32 + x] + (v >> 3));
}

static void simplewebp__transform_one(const int16_t *in, uint8_t *out)
{
	int32_t tmp[16], i;

	/* Vertical pass */
	for (i = 0; i < 4; i++)
	{
		int32_t a, b, c, d;

		a = in[i] + in[i + 8];
		b = in[i] - in[i + 8];
		c = simplewebp__mul2(in[i + 4]) - simplewebp__mul1(in[i + 12]);
		d = simplewebp__mul1(in[i + 4]) + simplewebp__mul2(in[i + 12]);
		tmp[i * 4] = a + d;
		tmp[i * 4 + 1] = b + c;
		tmp[i * 4 + 2] = b - c;
		tmp[i * 4 + 3] = a - d;
	}
	/* Horizontal pass */
	for (i = 0; i < 4; i++)
	{
		int32_t dc, a, b, c, d;

		dc = tmp[i] + 4;
		a = dc + tmp[i + 8];
		b = dc - tmp[i + 8];
		c = simplewebp__mul2(tmp[i + 4]) - simplewebp__mul1(tmp[i + 12]);
		d = simplewebp__mul1(tmp[i + 4]) + simplewebp__mul2(tmp[i + 12]);
		simplewebp__store(out, 0, i, a + d);
		simplewebp__store(out, 1, i, b + c);
		simplewebp__store(out, 2, i, b - c);
		simplewebp__store(out, 3, i, a - d);
	}
}

static void simplewebp__transform(const int16_t *in, uint8_t *out, uint8_t do_2)
{
	simplewebp__transform_one(in, out);
	if (do_2)
		simplewebp__transform_one(in + 16, out + 4);
}

static void simplewebp__transform_dc(const int16_t *in, uint8_t *out)
{
	int32_t dc, x, y;
	dc = in[0] + 4;

	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
			simplewebp__store(out, x, y, dc);
	}
}

static void simplewebp__store2(uint8_t *out, int32_t y, int32_t dc, int32_t d, int32_t c)
{
	simplewebp__store(out, 0, y, dc + d);
	simplewebp__store(out, 1, y, dc + c);
	simplewebp__store(out, 2, y, dc - c);
	simplewebp__store(out, 3, y, dc - d);
}

static void simplewebp__transform_ac3(const int16_t *in, uint8_t *out)
{
	int32_t a, c4, d4, c1, d1;

	a = in[0] + 4;
	c4 = simplewebp__mul2(in[4]);
	d4 = simplewebp__mul1(in[4]);
	c1 = simplewebp__mul2(in[1]);
	d1 = simplewebp__mul1(in[1]);
	simplewebp__store2(out, 0, a + d4, d1, c1);
	simplewebp__store2(out, 1, a + c4, d1, c1);
	simplewebp__store2(out, 2, a - c4, d1, c1);
	simplewebp__store2(out, 3, a - d4, d1, c1);
}

static void simplewebp__transform_uv(const int16_t *in, uint8_t *out)
{
	simplewebp__transform(in, out, 1);
	simplewebp__transform(in + 32 /* 2*16 */, out + 128 /* 4*BPS */, 1);
}

static void simplewebp__transform_dcuv(const int16_t *in, uint8_t *out)
{
	if (in[0])
		simplewebp__transform_dc(in, out);
	if (in[16])
		simplewebp__transform_dc(in + 16, out + 4);
	if (in[32])
		simplewebp__transform_dc(in + 32, out + 128);
	if (in[48])
		simplewebp__transform_dc(in + 48, out + 132);
}

static int32_t simplewebp__needsfilter2(const uint8_t *p, int32_t step, int32_t t, int32_t it)
{
	int32_t p3, p2, p1, p0, q0, q1, q2, q3;
	p3 = p[-4 * step];
	p2 = p[-3 * step];
	p1 = p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];
	q2 = p[2 * step];
	q3 = p[3 * step];

	if ((4 * simplewebp__kabs0[p0 - q0] + simplewebp__kabs0[p1 - q1]) > t)
		return 0;

	return
		simplewebp__kabs0[p3 - p2] <= it &&
		simplewebp__kabs0[p2 - p1] <= it &&
		simplewebp__kabs0[p1 - p0] <= it &&
		simplewebp__kabs0[q3 - q2] <= it &&
		simplewebp__kabs0[q2 - q1] <= it &&
		simplewebp__kabs0[q1 - q0] <= it;
}

static int32_t simplewebp__hev(const uint8_t *p, int32_t step, int32_t thresh)
{
	int32_t p1, p0, q0, q1;

	p1 = p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];

	return (simplewebp__kabs0[p1 - p0] > thresh) || (simplewebp__kabs0[q1 - q0] > thresh);
}

static void simplewebp__do_filter2(uint8_t *p, int32_t step)
{
	int32_t p1, p0, q0, q1, a, a1, a2;

	p1 =  p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];
	a = 3 * (q0 - p0) + simplewebp__ksclip1[p1 - q1];
	a1 =  simplewebp__ksclip2[(a + 4) >> 3];
	a2 =  simplewebp__ksclip2[(a + 3) >> 3];
	p[-step] = simplewebp__kclip1[p0 + a2];
	p[0] = simplewebp__kclip1[q0 - a1];
}

static void simplewebp__do_filter6(uint8_t *p, int32_t step)
{
	int32_t p2, p1, p0, q0, q1, q2, a, a1, a2, a3;

	p2 =  p[-3 * step];
	p1 =  p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];
	q2 = p[2 * step];
	a = simplewebp__ksclip1[3 * (q0 - p0) + simplewebp__ksclip1[p1 - q1]];
	a1 = (27 * a + 63) >> 7;
	a2 = (18 * a + 63) >> 7;
	a3 = (9 * a + 63) >> 7;

	p[-3 * step] = simplewebp__kclip1[p2 + a3];
	p[-2 * step] = simplewebp__kclip1[p1 + a2];
	p[-step] = simplewebp__kclip1[p0 + a1];
	p[0] = simplewebp__kclip1[q0 - a1];
	p[step] = simplewebp__kclip1[q1 - a2];
	p[2 * step] = simplewebp__kclip1[q2 - a3];
}

static void simplewebp__filterloop26(uint8_t *p, int32_t hstride, int32_t vstride, int32_t size, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	int32_t thresh2 = 2 * thresh + 1;

	while (size-- > 0)
	{
		if (simplewebp__needsfilter2(p, hstride, thresh2, ithresh))
		{
			if (simplewebp__hev(p, hstride, hev_thresh))
				simplewebp__do_filter2(p, hstride);
			else
				simplewebp__do_filter6(p, hstride);
		}

		p += vstride;
	}
}

static void simplewebp__vfilter16(uint8_t *p, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop26(p, stride, 1, 16, thresh, ithresh, hev_thresh);
}

static void simplewebp__do_filter4(uint8_t *p, int32_t step)
{
	int32_t p1, p0, q0, q1, a, a1, a2, a3;

	p1 =  p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];
	a = 3 * (q0 - p0);
	a1 = simplewebp__ksclip2[(a + 4) >> 3];
	a2 = simplewebp__ksclip2[(a + 3) >> 3];
	a3 = (a1 + 1) >> 1;

	p[-2 * step] = simplewebp__kclip1[p1 + a3];
	p[-step] = simplewebp__kclip1[p0 + a2];
	p[0] = simplewebp__kclip1[q0 - a1];
	p[step] = simplewebp__kclip1[q1 - a3];
}

static void simplewebp__filterloop24(uint8_t *p, int32_t hstride, int32_t vstride, int32_t size, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	int32_t thresh2 = 2 * thresh + 1;

	while (size-- > 0)
	{
		if (simplewebp__needsfilter2(p, hstride, thresh2, ithresh))
		{
			if (simplewebp__hev(p, hstride, hev_thresh))
				simplewebp__do_filter2(p, hstride);
			else
				simplewebp__do_filter4(p, hstride);
		}

		p += vstride;
	}
}

static void simplewebp__vfilter16_i(uint8_t *p, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	int32_t i;

	for (i = 3; i > 0; i--)
	{
		p += 4 * stride;
		simplewebp__filterloop24(p, stride, 1, 16, thresh, ithresh, hev_thresh);
	}
}

static void simplewebp__hfilter16(uint8_t *p, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop26(p, 1, stride, 16, thresh, ithresh, hev_thresh);
}

static void simplewebp__vfilter8(uint8_t *u, uint8_t *v, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop26(u, stride, 1, 8, thresh, ithresh, hev_thresh);
	simplewebp__filterloop26(v, stride, 1, 8, thresh, ithresh, hev_thresh);
}

static void simplewebp__vfilter8_i(uint8_t *u, uint8_t *v, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop24(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
	simplewebp__filterloop24(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}

static int32_t simplewebp__needsfilter(const uint8_t *p, int32_t step, int32_t t)
{
	int32_t p1, p0, q0, q1;

	p1 =  p[-2 * step];
	p0 = p[-step];
	q0 = p[0];
	q1 = p[step];
	return (4 * simplewebp__kabs0[p0 - q0] + simplewebp__kabs0[p1 - q1]) <= t;
}

static void simplewebp__simple_vfilter16(uint8_t *p, int32_t stride, int32_t thresh)
{
	int32_t i, thresh2;

	thresh2 = 2 * thresh + 1;
	for (i = 0; i < 16; i++)
	{
		if (simplewebp__needsfilter(p + i, stride, thresh2))
			simplewebp__do_filter2(p + i, stride);
	}
}

static void simplewebp__simple_hfilter16(uint8_t *p, int32_t stride, int32_t thresh)
{
	int32_t i, thresh2;
	uint8_t *target;

	thresh2 = 2 * thresh + 1;
	for (i = 0; i < 16; i++)
	{
		target = p + i * stride;

		if (simplewebp__needsfilter(target, 1, thresh2))
			simplewebp__do_filter2(target, 1);
	}
}

static void simplewebp__simple_vfilter16_i(uint8_t *p, int32_t stride, int32_t thresh)
{
	int32_t k;

	for (k = 3; k > 0; k--)
	{
		p += 4 * stride;
		simplewebp__simple_vfilter16(p, stride, thresh);
	}
}

static void simplewebp__simple_hfilter16_i(uint8_t *p, int32_t stride, int32_t thresh)
{
	int32_t k;

	for (k = 3; k > 0; k--)
	{
		p += 4;
		simplewebp__simple_hfilter16(p, stride, thresh);
	}
}

static void simplewebp__hfilter16_i(uint8_t *p, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	int32_t k;

	for (k = 3; k > 0; k--)
	{
		p += 4;
		simplewebp__filterloop24(p, 1, stride, 16, thresh, ithresh, hev_thresh);
	}
}

static void simplewebp__hfilter8(uint8_t *u, uint8_t *v, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop26(u, 1, stride, 8, thresh, ithresh, hev_thresh);
	simplewebp__filterloop26(v, 1, stride, 8, thresh, ithresh, hev_thresh);
}

static void simplewebp__hfilter8_i(uint8_t *u, uint8_t *v, int32_t stride, int32_t thresh, int32_t ithresh, int32_t hev_thresh)
{
	simplewebp__filterloop24(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
	simplewebp__filterloop24(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}

/* DC */
static void simplewebp__predluma4_0(uint8_t *out)
{
	uint32_t dc;
	int32_t i;

	dc = 4;
	for (i = 0; i < 4; i++)
		dc += out[i - 32] + out[-1 + i * 32];
	dc >>= 3;
	for (i = 0; i < 4; i++)
		memset(out + i * 32, dc, 4);
}

static void simplewebp__truemotion(uint8_t *out, int32_t size)
{
	const uint8_t *top, *clip0, *clip;
	int32_t x, y;

	top = out - 32;
	clip0 = simplewebp__kclip1 - top[-1];

	for (y = 0; y < size; y++)
	{
		clip = clip0 + out[-1];

		for (x = 0; x < size; x++)
			out[x] = clip[top[x]];

		out += 32;
	}
}

/* TM4 */
static void simplewebp__predluma4_1(uint8_t *out)
{
	simplewebp__truemotion(out, 4);
}

static uint8_t simplewebp__avg3(uint32_t a, uint32_t b, uint32_t c)
{
	return (uint8_t) ((a + 2 * b + c + 2) >> 2);
}

/* Vertical */
static void simplewebp__predluma4_2(uint8_t *out)
{
	const uint8_t *top;
	uint8_t vals[4], i;

	top = out - 32;

	for (i = 0; i < 4; i++)
		vals[i] = simplewebp__avg3(top[i - 1], top[i], top[i + 1]);
	for (i = 0; i < 4; i++)
		memcpy(out + i * 32, vals, sizeof(vals));
}

static void simplewebp__from_uint32(uint8_t *out, uint32_t value)
{
	/* Little endian */
	out[0] = (uint8_t) value;
	out[1] = (uint8_t) (value >> 8);
	out[2] = (uint8_t) (value >> 16);
	out[3] = (uint8_t) (value >> 24);
}

/* Horizontal*/
static void simplewebp__predluma4_3(uint8_t *out)
{
	int32_t vals[5], i;
	for (i = -1; i < 4; i++)
		vals[i + 1] = out[-1 + i * 32];

	simplewebp__from_uint32(out, 0x01010101U * simplewebp__avg3(vals[0], vals[1], vals[2]));
	simplewebp__from_uint32(out + 32, 0x01010101U * simplewebp__avg3(vals[1], vals[2], vals[3]));
	simplewebp__from_uint32(out + 64, 0x01010101U * simplewebp__avg3(vals[2], vals[3], vals[4]));
	simplewebp__from_uint32(out + 96, 0x01010101U * simplewebp__avg3(vals[3], vals[4], vals[4]));
}

/* Right Down */
static void simplewebp__predluma4_4(uint8_t *out)
{
	uint32_t i, j, k, l, x, a, b, c, d;

	i = out[-1];
	j = out[-1 + 1 * 32];
	k = out[-1 + 2 * 32];
	l = out[-1 + 3 * 32];
	x = out[-1 - 32];
	a = out[0 - 32];
	b = out[1 - 32];
	c = out[2 - 32];
	d = out[3 - 32];

	out[3 * 32 + 0] = simplewebp__avg3(j, k, l);
	out[3 * 32 + 1] = out[2 * 32 + 0] = simplewebp__avg3(i, j, k);
	out[3 * 32 + 2] = out[2 * 32 + 1] = out[1 * 32 + 0] = simplewebp__avg3(x, i, j);
	out[3 * 32 + 3] = out[2 * 32 + 2] = out[1 * 32 + 1] = out[0 * 32 + 0] = simplewebp__avg3(a, x, i);
	out[2 * 32 + 3] = out[1 * 32 + 2] = out[0 * 32 + 1] = simplewebp__avg3(b, a, x);
	out[1 * 32 + 3] = out[0 * 32 + 2] = simplewebp__avg3(c, b, a);
	out[0 * 32 + 3] = simplewebp__avg3(d, c, b);
}

static uint8_t simplewebp__avg2(uint32_t a, uint32_t b)
{
	return (uint8_t) ((a + b + 1) >> 1);
}

/* Vertical-Right */
static void simplewebp__predluma4_5(uint8_t *out)
{
	uint32_t i, j, k, x, a, b, c, d;

	i = out[-1];
	j = out[-1 + 1 * 32];
	k = out[-1 + 2 * 32];
	x = out[-1 - 32];
	a = out[0 - 32];
	b = out[1 - 32];
	c = out[2 - 32];
	d = out[3 - 32];

	out[0 * 32 + 0] = out[2 * 32 + 1] = simplewebp__avg2(x, a);
	out[0 * 32 + 1] = out[2 * 32 + 2] = simplewebp__avg2(a, b);
	out[0 * 32 + 2] = out[2 * 32 + 3] = simplewebp__avg2(b, c);
	out[0 * 32 + 3] = simplewebp__avg2(c, d);
	out[3 * 32 + 0] = simplewebp__avg3(k, j, i);
	out[2 * 32 + 0] = simplewebp__avg3(j, i, x);
	out[1 * 32 + 0] = out[3 * 32 + 1] = simplewebp__avg3(i, x, a);
	out[1 * 32 + 1] = out[3 * 32 + 2] = simplewebp__avg3(x, a, b);
	out[1 * 32 + 2] = out[3 * 32 + 3] = simplewebp__avg3(a, b, c);
	out[1 * 32 + 3] = simplewebp__avg3(b, c, d);
}

/* Left Down */
static void simplewebp__predluma4_6(uint8_t *out)
{
	int32_t a, b, c, d, e, f, g, h;

	a = out[-32];
	b = out[-31];
	c = out[-30];
	d = out[-29];
	e = out[-28];
	f = out[-27];
	g = out[-26];
	h = out[-25];

	out[0 * 32 + 0] = simplewebp__avg3(a, b, c);
	out[0 * 32 + 1] = out[1 * 32 + 0] = simplewebp__avg3(b, c, d);
	out[0 * 32 + 2] = out[1 * 32 + 1] = out[2 * 32 + 0] = simplewebp__avg3(c, d, e);
	out[0 * 32 + 3] = out[1 * 32 + 2] = out[2 * 32 + 1] = out[3 * 32 + 0] = simplewebp__avg3(d, e, f);
	out[1 * 32 + 3] = out[2 * 32 + 2] = out[3 * 32 + 1] = simplewebp__avg3(e, f, g);
	out[2 * 32 + 3] = out[3 * 32 + 2] = simplewebp__avg3(f, g, h);
	out[3 * 32 + 3] = simplewebp__avg3(g, h, h);
}

/* Vertical-Left */
static void simplewebp__predluma4_7(uint8_t *out)
{
	int32_t a, b, c, d, e, f, g, h;

	a = out[-32];
	b = out[-31];
	c = out[-30];
	d = out[-29];
	e = out[-28];
	f = out[-27];
	g = out[-26];
	h = out[-25];

	out[0 * 32 + 0] = simplewebp__avg2(a, b);
	out[0 * 32 + 1] = out[2 * 32 + 0] = simplewebp__avg2(b, c);
	out[0 * 32 + 2] = out[2 * 32 + 1] = simplewebp__avg2(c, d);
	out[0 * 32 + 3] = out[2 * 32 + 2] = simplewebp__avg2(d, e);
	out[1 * 32 + 0] = simplewebp__avg3(a, b, c);
	out[1 * 32 + 1] = out[3 * 32 + 0] = simplewebp__avg3(b, c, d);
	out[1 * 32 + 2] = out[3 * 32 + 1] = simplewebp__avg3(c, d, e);
	out[1 * 32 + 3] = out[3 * 32 + 2] = simplewebp__avg3(d, e, f);
	out[2 * 32 + 3] = simplewebp__avg3(e, f, g);
	out[3 * 32 + 3] = simplewebp__avg3(f, g, h);
}

/* Horizontal-Down */
static void simplewebp__predluma4_8(uint8_t *out)
{
	uint32_t i, j, k, l, x, a, b, c;

	i = out[-1];
	j = out[-1 + 1 * 32];
	k = out[-1 + 2 * 32];
	l = out[-1 + 3 * 32];
	x = out[-1 - 32];
	a = out[0 - 32];
	b = out[1 - 32];
	c = out[2 - 32];
	
	out[0 * 32 + 0] = out[1 * 32 + 2] = simplewebp__avg2(i, x);
	out[1 * 32 + 0] = out[2 * 32 + 2] = simplewebp__avg2(j, i);
	out[2 * 32 + 0] = out[3 * 32 + 2] = simplewebp__avg2(k, j);
	out[3 * 32 + 0] = simplewebp__avg2(l, k);
	out[0 * 32 + 3] = simplewebp__avg3(a, b, c);
	out[0 * 32 + 2] = simplewebp__avg3(x, a, b);
	out[0 * 32 + 1] = out[1 * 32 + 3] = simplewebp__avg3(i, x, a);
	out[1 * 32 + 1] = out[2 * 32 + 3] = simplewebp__avg3(j, i, x);
	out[2 * 32 + 1] = out[3 * 32 + 3] = simplewebp__avg3(k, j, i);
	out[3 * 32 + 1] = simplewebp__avg3(l, k, j);
}

/* Horizontal-Up */
static void simplewebp__predluma4_9(uint8_t *out)
{
	uint32_t i, j, k, l;

	i = out[-1];
	j = out[-1 + 1 * 32];
	k = out[-1 + 2 * 32];
	l = out[-1 + 3 * 32];

	out[0 * 32 + 0] = simplewebp__avg2(i, j);
	out[0 * 32 + 2] = out[1 * 32 + 0] = simplewebp__avg2(j, k);
	out[1 * 32 + 2] = out[2 * 32 + 0] = simplewebp__avg2(k, l);
	out[0 * 32 + 1] = simplewebp__avg3(i, j, k);
	out[0 * 32 + 3] = out[1 * 32 + 1] = simplewebp__avg3(j, k, l);
	out[1 * 32 + 3] = out[2 * 32 + 1] = simplewebp__avg3(k, l, l);
	out[2 * 32 + 3] = out[2 * 32 + 2] =
	out[3 * 32 + 0] = out[3 * 32 + 1] =
	out[3 * 32 + 2] = out[3 * 32 + 3] = l;
}

static void simplewebp__predluma4(uint8_t num, uint8_t *out)
{
	switch (num)
	{
		case 0:
			simplewebp__predluma4_0(out);
			break;
		case 1:
			simplewebp__predluma4_1(out);
			break;
		case 2:
			simplewebp__predluma4_2(out);
			break;
		case 3:
			simplewebp__predluma4_3(out);
			break;
		case 4:
			simplewebp__predluma4_4(out);
			break;
		case 5:
			simplewebp__predluma4_5(out);
			break;
		case 6:
			simplewebp__predluma4_6(out);
			break;
		case 7:
			simplewebp__predluma4_7(out);
			break;
		case 8:
			simplewebp__predluma4_8(out);
			break;
		case 9:
			simplewebp__predluma4_9(out);
			break;
		default:
			break;
	}
}

static void simplewebp__put16(int32_t v, uint8_t *out)
{
	int32_t j;
	for (j = 0; j < 16; j++)
		memset(out + j * 32, v, 16);
}

/* DC */
static void simplewebp__predluma16_0(uint8_t *out)
{
	int32_t dc, j;

	dc = 16;
	for (j = 0; j < 16; j++)
		dc += out[-1 + j * 32] + out[j - 32];

	simplewebp__put16(dc >> 5, out);
}

/* TM */
static void simplewebp__predluma16_1(uint8_t *out)
{
	simplewebp__truemotion(out, 16);
}

/* Vertical */
static void simplewebp__predluma16_2(uint8_t *out)
{
	int32_t j;
	for (j = 0; j < 16; j++)
		memcpy(out + j * 32, out - 32, 16);
}

/* Horizontal */
static void simplewebp__predluma16_3(uint8_t *out)
{
	int32_t j;
	for (j = 16; j > 0; j--)
	{
		memset(out, out[-1], 16);
		out += 32;
	}
}

/* DC w/o Top */
static void simplewebp__predluma16_4(uint8_t *out)
{
	int32_t dc, j;

	dc = 8;
	for (j = 0; j < 16; j++)
		dc += out[-1 + j * 32];

	simplewebp__put16(dc >> 4, out);
}

/* DC w/o Left */
static void simplewebp__predluma16_5(uint8_t *out)
{
	int32_t dc, j;

	dc = 8;
	for (j = 0; j < 16; j++)
		dc += out[j - 32];

	simplewebp__put16(dc >> 4, out);
}

/* DC w/o Top Left */
static void simplewebp__predluma16_6(uint8_t *out)
{
	simplewebp__put16(128, out);
}

static void simplewebp__predluma16(uint8_t num, uint8_t *out)
{
	switch (num)
	{
		case 0:
			simplewebp__predluma16_0(out);
			break;
		case 1:
			simplewebp__predluma16_1(out);
			break;
		case 2:
			simplewebp__predluma16_2(out);
			break;
		case 3:
			simplewebp__predluma16_3(out);
			break;
		case 4:
			simplewebp__predluma16_4(out);
			break;
		case 5:
			simplewebp__predluma16_5(out);
			break;
		case 6:
			simplewebp__predluma16_6(out);
			break;
		default:
			break;
	}
}

static void simplewebp__put8x8uv(int32_t value, uint8_t *out)
{
	int32_t j;
	for (j = 0; j < 8; j++)
		memset(out + j * 32, value, 8);
}

/* DC */
static void simplewebp__predchroma8_0(uint8_t *out)
{
	int32_t dc0, i;

	dc0 = 8;
	for (i = 0; i < 8; i++)
		dc0 += out[i - 32] + out[-1 + i * 32];

	simplewebp__put8x8uv(dc0 >> 4, out);
}

/* TM */
static void simplewebp__predchroma8_1(uint8_t *out)
{
	simplewebp__truemotion(out, 8);
}

/* Vertical */
static void simplewebp__predchroma8_2(uint8_t *out)
{
	int32_t j;
	for (j = 0; j < 8; j++)
		memcpy(out + j * 32, out - 32, 8);
}

/* Horizontal */
static void simplewebp__predchroma8_3(uint8_t *out)
{
	int32_t j;
	for (j = 0; j < 8; j++)
		memset(out + j * 32, out[j * 32 - 1], 8);
}

/* DC w/o Top */
static void simplewebp__predchroma8_4(uint8_t *out)
{
	int32_t dc0, i;

	dc0 = 4;
	for (i = 0; i < 8; i++)
		dc0 += out[-1 + i * 32];

	simplewebp__put8x8uv(dc0 >> 3, out);
}

/* DC w/o Left */
static void simplewebp__predchroma8_5(uint8_t *out)
{
	int32_t dc0, i;

	dc0 = 4;
	for (i = 0; i < 8; i++)
		dc0 += out[i - 32];

	simplewebp__put8x8uv(dc0 >> 3, out);
}

/* DC w/o Top Left */
static void simplewebp__predchroma8_6(uint8_t *out)
{
	simplewebp__put8x8uv(128, out);
}

static void simplewebp__predchroma8(uint8_t num, uint8_t *out)
{
	switch (num)
	{
		case 0:
			simplewebp__predchroma8_0(out);
			break;
		case 1:
			simplewebp__predchroma8_1(out);
			break;
		case 2:
			simplewebp__predchroma8_2(out);
			break;
		case 3:
			simplewebp__predchroma8_3(out);
			break;
		case 4:
			simplewebp__predchroma8_4(out);
			break;
		case 5:
			simplewebp__predchroma8_5(out);
			break;
		case 6:
			simplewebp__predchroma8_6(out);
			break;
		default:
			break;
	}
}

static void simplewebp__dither_combine_8x8(const uint8_t *dither, uint8_t *out, int32_t stride)
{
	int32_t i, j, delta0, delta1;

	for (j = 0; j < 8; j++)
	{
		for (i = 0; i < 8; i++)
		{
			delta0 = dither[i] - 128;
			delta1 = (delta0 + 8) >> 4;
			out[i] = simplewebp__clip8b(out[i] + delta1);
		}

		out += stride;
		dither += 8;
	}
}

/* RFC 6386 section 11.5 */
static const uint8_t simplewebp__modes_proba[10][10][9] = {
	{ { 231, 120, 48, 89, 115, 113, 120, 152, 112 },
		{ 152, 179, 64, 126, 170, 118, 46, 70, 95 },
		{ 175, 69, 143, 80, 85, 82, 72, 155, 103 },
		{ 56, 58, 10, 171, 218, 189, 17, 13, 152 },
		{ 114, 26, 17, 163, 44, 195, 21, 10, 173 },
		{ 121, 24, 80, 195, 26, 62, 44, 64, 85 },
		{ 144, 71, 10, 38, 171, 213, 144, 34, 26 },
		{ 170, 46, 55, 19, 136, 160, 33, 206, 71 },
		{ 63, 20, 8, 114, 114, 208, 12, 9, 226 },
		{ 81, 40, 11, 96, 182, 84, 29, 16, 36 } },
	{ { 134, 183, 89, 137, 98, 101, 106, 165, 148 },
		{ 72, 187, 100, 130, 157, 111, 32, 75, 80 },
		{ 66, 102, 167, 99, 74, 62, 40, 234, 128 },
		{ 41, 53, 9, 178, 241, 141, 26, 8, 107 },
		{ 74, 43, 26, 146, 73, 166, 49, 23, 157 },
		{ 65, 38, 105, 160, 51, 52, 31, 115, 128 },
		{ 104, 79, 12, 27, 217, 255, 87, 17, 7 },
		{ 87, 68, 71, 44, 114, 51, 15, 186, 23 },
		{ 47, 41, 14, 110, 182, 183, 21, 17, 194 },
		{ 66, 45, 25, 102, 197, 189, 23, 18, 22 } },
	{ { 88, 88, 147, 150, 42, 46, 45, 196, 205 },
		{ 43, 97, 183, 117, 85, 38, 35, 179, 61 },
		{ 39, 53, 200, 87, 26, 21, 43, 232, 171 },
		{ 56, 34, 51, 104, 114, 102, 29, 93, 77 },
		{ 39, 28, 85, 171, 58, 165, 90, 98, 64 },
		{ 34, 22, 116, 206, 23, 34, 43, 166, 73 },
		{ 107, 54, 32, 26, 51, 1, 81, 43, 31 },
		{ 68, 25, 106, 22, 64, 171, 36, 225, 114 },
		{ 34, 19, 21, 102, 132, 188, 16, 76, 124 },
		{ 62, 18, 78, 95, 85, 57, 50, 48, 51 } },
	{ { 193, 101, 35, 159, 215, 111, 89, 46, 111 },
		{ 60, 148, 31, 172, 219, 228, 21, 18, 111 },
		{ 112, 113, 77, 85, 179, 255, 38, 120, 114 },
		{ 40, 42, 1, 196, 245, 209, 10, 25, 109 },
		{ 88, 43, 29, 140, 166, 213, 37, 43, 154 },
		{ 61, 63, 30, 155, 67, 45, 68, 1, 209 },
		{ 100, 80, 8, 43, 154, 1, 51, 26, 71 },
		{ 142, 78, 78, 16, 255, 128, 34, 197, 171 },
		{ 41, 40, 5, 102, 211, 183, 4, 1, 221 },
		{ 51, 50, 17, 168, 209, 192, 23, 25, 82 } },
	{ { 138, 31, 36, 171, 27, 166, 38, 44, 229 },
		{ 67, 87, 58, 169, 82, 115, 26, 59, 179 },
		{ 63, 59, 90, 180, 59, 166, 93, 73, 154 },
		{ 40, 40, 21, 116, 143, 209, 34, 39, 175 },
		{ 47, 15, 16, 183, 34, 223, 49, 45, 183 },
		{ 46, 17, 33, 183, 6, 98, 15, 32, 183 },
		{ 57, 46, 22, 24, 128, 1, 54, 17, 37 },
		{ 65, 32, 73, 115, 28, 128, 23, 128, 205 },
		{ 40, 3, 9, 115, 51, 192, 18, 6, 223 },
		{ 87, 37, 9, 115, 59, 77, 64, 21, 47 } },
	{ { 104, 55, 44, 218, 9, 54, 53, 130, 226 },
		{ 64, 90, 70, 205, 40, 41, 23, 26, 57 },
		{ 54, 57, 112, 184, 5, 41, 38, 166, 213 },
		{ 30, 34, 26, 133, 152, 116, 10, 32, 134 },
		{ 39, 19, 53, 221, 26, 114, 32, 73, 255 },
		{ 31, 9, 65, 234, 2, 15, 1, 118, 73 },
		{ 75, 32, 12, 51, 192, 255, 160, 43, 51 },
		{ 88, 31, 35, 67, 102, 85, 55, 186, 85 },
		{ 56, 21, 23, 111, 59, 205, 45, 37, 192 },
		{ 55, 38, 70, 124, 73, 102, 1, 34, 98 } },
	{ { 125, 98, 42, 88, 104, 85, 117, 175, 82 },
		{ 95, 84, 53, 89, 128, 100, 113, 101, 45 },
		{ 75, 79, 123, 47, 51, 128, 81, 171, 1 },
		{ 57, 17, 5, 71, 102, 57, 53, 41, 49 },
		{ 38, 33, 13, 121, 57, 73, 26, 1, 85 },
		{ 41, 10, 67, 138, 77, 110, 90, 47, 114 },
		{ 115, 21, 2, 10, 102, 255, 166, 23, 6 },
		{ 101, 29, 16, 10, 85, 128, 101, 196, 26 },
		{ 57, 18, 10, 102, 102, 213, 34, 20, 43 },
		{ 117, 20, 15, 36, 163, 128, 68, 1, 26 } },
	{ { 102, 61, 71, 37, 34, 53, 31, 243, 192 },
		{ 69, 60, 71, 38, 73, 119, 28, 222, 37 },
		{ 68, 45, 128, 34, 1, 47, 11, 245, 171 },
		{ 62, 17, 19, 70, 146, 85, 55, 62, 70 },
		{ 37, 43, 37, 154, 100, 163, 85, 160, 1 },
		{ 63, 9, 92, 136, 28, 64, 32, 201, 85 },
		{ 75, 15, 9, 9, 64, 255, 184, 119, 16 },
		{ 86, 6, 28, 5, 64, 255, 25, 248, 1 },
		{ 56, 8, 17, 132, 137, 255, 55, 116, 128 },
		{ 58, 15, 20, 82, 135, 57, 26, 121, 40 } },
	{ { 164, 50, 31, 137, 154, 133, 25, 35, 218 },
		{ 51, 103, 44, 131, 131, 123, 31, 6, 158 },
		{ 86, 40, 64, 135, 148, 224, 45, 183, 128 },
		{ 22, 26, 17, 131, 240, 154, 14, 1, 209 },
		{ 45, 16, 21, 91, 64, 222, 7, 1, 197 },
		{ 56, 21, 39, 155, 60, 138, 23, 102, 213 },
		{ 83, 12, 13, 54, 192, 255, 68, 47, 28 },
		{ 85, 26, 85, 85, 128, 128, 32, 146, 171 },
		{ 18, 11, 7, 63, 144, 171, 4, 4, 246 },
		{ 35, 27, 10, 146, 174, 171, 12, 26, 128 } },
	{ { 190, 80, 35, 99, 180, 80, 126, 54, 45 },
		{ 85, 126, 47, 87, 176, 51, 41, 20, 32 },
		{ 101, 75, 128, 139, 118, 146, 116, 128, 85 },
		{ 56, 41, 15, 176, 236, 85, 37, 9, 62 },
		{ 71, 30, 17, 119, 118, 255, 17, 18, 138 },
		{ 101, 38, 60, 138, 55, 70, 43, 26, 142 },
		{ 146, 36, 19, 30, 171, 255, 97, 27, 20 },
		{ 138, 45, 61, 62, 219, 1, 81, 188, 64 },
		{ 32, 41, 20, 117, 151, 142, 20, 21, 163 },
		{ 112, 19, 12, 61, 195, 128, 48, 4, 24 } }
};

static const uint32_t simplewebp__random_table[55] = {
	0x0de15230, 0x03b31886, 0x775faccb, 0x1c88626a, 0x68385c55, 0x14b3b828,
	0x4a85fef8, 0x49ddb84b, 0x64fcf397, 0x5c550289, 0x4a290000, 0x0d7ec1da,
	0x5940b7ab, 0x5492577d, 0x4e19ca72, 0x38d38c69, 0x0c01ee65, 0x32a1755f,
	0x5437f652, 0x5abb2c32, 0x0faa57b1, 0x73f533e7, 0x685feeda, 0x7563cce2,
	0x6e990e83, 0x4730a7ed, 0x4fc0d9c6, 0x496b153c, 0x4f1403fa, 0x541afb0c,
	0x73990b32, 0x26d7cb1c, 0x6fcc3706, 0x2cbb77d8, 0x75762f2a, 0x6425ccdd,
	0x24b35461, 0x0a7d8715, 0x220414a8, 0x141ebf67, 0x56b41583, 0x73e502e3,
	0x44cab16f, 0x28264d42, 0x73baaefb, 0x0a50ebed, 0x1d6ab6fb, 0x0d3ad40b,
	0x35db3b68, 0x2b081e83, 0x77ce6b95, 0x5181e5f0, 0x78853bbc, 0x009f9494,
	0x27e5ed3c
};

static void simplewebp__random_init(struct simplewebp__random *rng, float dithering)
{
	memcpy(rng->tab, simplewebp__random_table, sizeof(rng->tab));
	rng->index1 = 0;
	rng->index2 = 31;
	rng->amp = (dithering < 0.0f) ? 0 : (dithering > 1.0f) ? 256 : (uint32_t) (256 * dithering);
}

/* 
Returns a centered pseudo-random number with 'num_bits' amplitude.
(uses D.Knuth's Difference-based random generator).
'amp' is in VP8_RANDOM_DITHER_FIX fixed-point precision.
*/
static int32_t simplewebp__random_bits2(struct simplewebp__random *rng, int32_t num_bits, int32_t amp)
{
	int32_t diff;

	assert((num_bits + 8) <= 31);
	diff = rng->tab[rng->index1];
	if (diff < 0)
		diff += (1u << 31);
	rng->tab[rng->index1] = diff;
	if (++rng->index1 == 55)
		rng->index1 = 0;
	if (++rng->index2 == 55)
		rng->index2 = 0;

	diff = (int) ((((uint32_t) diff) << 1) >> (32 - num_bits));
	diff = (diff * amp) >> 8;
	diff += (1 << (num_bits - 1));
	return diff;
}

static const uint8_t simplewebp__bands[17] = {
	0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0
};

static simplewebp_error simplewebp__load_header_lossy(struct simplewebp__vp8_decoder *vp8d, uint8_t *buf, size_t bufsize)
{
	struct simplewebp__segment_header *segmnt_hdr;
	struct simplewebp__filter_header *filt_hdr;
	struct simplewebp__bitread br;

	/* Populate picture headers */
	vp8d->mb_w = (vp8d->picture_header.width + 15) >> 4;
	vp8d->mb_h = (vp8d->picture_header.height + 15) >> 4;

	/* Reset proba */
	memset(vp8d->proba.segments, 255, sizeof(vp8d->proba.segments));
	/* Reset segment header */
	vp8d->segment_header.use_segment = 0;
	vp8d->segment_header.update_map = 0;
	vp8d->segment_header.absolute_delta = 1;
	memset(&vp8d->segment_header.quantizer, 0, sizeof(vp8d->segment_header.quantizer));
	memset(&vp8d->segment_header.filter_strength, 0, sizeof(vp8d->segment_header.filter_strength));

	/* Initialize bitreader */
	simplewebp__bitread_init(&br, buf, vp8d->frame_header.partition_length);
	buf += vp8d->frame_header.partition_length;
	bufsize -= vp8d->frame_header.partition_length;

	/* Read more picture headers. This is a keyframe */
	vp8d->picture_header.colorspace = simplewebp__bitread_getval(&br, 1);
	vp8d->picture_header.clamp_type = simplewebp__bitread_getval(&br, 1);

	/* Parse segment header */
	segmnt_hdr = &vp8d->segment_header;
	segmnt_hdr->use_segment = simplewebp__bitread_getval(&br, 1);
	if (segmnt_hdr->use_segment)
	{
		int32_t s;
		segmnt_hdr->update_map = simplewebp__bitread_getval(&br, 1);

		if (simplewebp__bitread_getval(&br, 1) /* update data */)
		{
			segmnt_hdr->absolute_delta = simplewebp__bitread_getval(&br, 1);
			for (s = 0; s < 4; s++)
				segmnt_hdr->quantizer[s] = simplewebp__bitread_getval(&br, 1)
					? simplewebp__bitread_getvalsigned(&br, 7)
					: 0;
			for (s = 0; s < 4; s++)
				segmnt_hdr->filter_strength[s] = simplewebp__bitread_getval(&br, 1)
					? simplewebp__bitread_getvalsigned(&br, 6)
					: 0;
			
		}

		if (segmnt_hdr->update_map)
		{
			for (s = 0; s < 3; s++)
				vp8d->proba.segments[s] = simplewebp__bitread_getval(&br, 1)
					? simplewebp__bitread_getval(&br, 8)
					: 255;
		}
	}
	else
		segmnt_hdr->update_map = 0;

	if (br.eof)
		return SIMPLEWEBP_CORRUPT_ERROR;

	/* Parse filters */
	filt_hdr = &vp8d->filter_header;
	filt_hdr->simple = simplewebp__bitread_getval(&br, 1);
	filt_hdr->level = simplewebp__bitread_getval(&br, 6);
	filt_hdr->sharpness = simplewebp__bitread_getval(&br, 3);
	filt_hdr->use_lf_delta = simplewebp__bitread_getval(&br, 1);

	if (filt_hdr->use_lf_delta)
	{
		if (simplewebp__bitread_getval(&br, 1) /* update lf-delta? */)
		{
			int32_t i;
			
			for (i = 0; i < 4; i++)
			{
				if (simplewebp__bitread_getval(&br, 1))
					filt_hdr->ref_lf_delta[i] = simplewebp__bitread_getvalsigned(&br, 6);
			}

			for (i = 0; i < 4; i++)
			{
				if (simplewebp__bitread_getval(&br, 1))
					filt_hdr->ref_lf_delta[i] = simplewebp__bitread_getvalsigned(&br, 6);
			}
		}
	}

	vp8d->filter_type = filt_hdr->level == 0 ? 0 : (filt_hdr->simple ? 1 : 2);

	if (br.eof)
		return SIMPLEWEBP_CORRUPT_ERROR;

	/* Parse partitions */
	{
		uint8_t *sz, *buf_end, *part_start;
		size_t size_left, last_part, p;

		sz = buf;
		buf_end = buf + bufsize;
		size_left = bufsize;

		last_part = vp8d->nparts_minus_1 = (1 << simplewebp__bitread_getval(&br, 2)) - 1;
		if (3 * last_part > bufsize)
			return SIMPLEWEBP_CORRUPT_ERROR;

		part_start = buf + last_part * 3;
		size_left -= last_part * 3;

		for (p = 0; p < last_part; p++)
		{
			size_t psize = simplewebp__to_uint24(sz);
			if (psize > size_left)
				psize = size_left;

			simplewebp__bitread_init(vp8d->parts + p, part_start, psize);
			part_start += psize;
			size_left -= psize;
			sz += 3;
		}

		simplewebp__bitread_init(vp8d->parts + last_part, part_start, size_left);
		if (part_start >= buf_end)
			return SIMPLEWEBP_CORRUPT_ERROR;
	}

	/* Parse quantizer */
	{
		int32_t base_q0, dqy1_dc, dqy2_dc, dqy2_ac, dquv_dc, dquv_ac, i;
		base_q0 = simplewebp__bitread_getval(&br, 7);
		dqy1_dc = simplewebp__bitread_getval(&br, 1)
			? simplewebp__bitread_getvalsigned(&br, 4)
			: 0;
		dqy2_dc = simplewebp__bitread_getval(&br, 1)
			? simplewebp__bitread_getvalsigned(&br, 4)
			: 0;
		dqy2_ac = simplewebp__bitread_getval(&br, 1)
			? simplewebp__bitread_getvalsigned(&br, 4)
			: 0;
		dquv_dc = simplewebp__bitread_getval(&br, 1)
			? simplewebp__bitread_getvalsigned(&br, 4)
			: 0;
		dquv_ac = simplewebp__bitread_getval(&br, 1)
			? simplewebp__bitread_getvalsigned(&br, 4)
			: 0;

		for (i = 0; i < 4; i++)
		{
			int32_t q;
			struct simplewebp__quantmat *m;

			if (segmnt_hdr->use_segment)
				q = segmnt_hdr->quantizer[i] + (!segmnt_hdr->absolute_delta) * base_q0;
			else
			{
				if (i > 0)
				{
					vp8d->dqm[i] = vp8d->dqm[0];
					continue;
				}
				else
					q = base_q0;
			}

			m = vp8d->dqm + i;
			m->y1_mat[0] = simplewebp__dctab[simplewebp__clip(q + dqy1_dc, 127)];
			m->y1_mat[1] = simplewebp__actab[simplewebp__clip(q + 0,       127)];

			m->y2_mat[0] = simplewebp__dctab[simplewebp__clip(q + dqy2_dc, 127)] * 2;
			m->y2_mat[1] = (simplewebp__actab[simplewebp__clip(q + dqy2_ac, 127)] * 101581) >> 16;
			if (m->y2_mat[1] < 8)
				m->y2_mat[1] = 8;

			m->uv_mat[0] = simplewebp__dctab[simplewebp__clip(q + dquv_dc, 117)];
			m->uv_mat[1] = simplewebp__actab[simplewebp__clip(q + dquv_ac, 127)];

			m->uv_quant = q + dquv_ac;
		}
	}

	/* Ignore "update proba" */
	simplewebp__bitread_getval(&br, 1);

	/* Parse proba */
	{
		struct simplewebp__proba *proba;
		int32_t t, b, c, p, v;

		proba = &vp8d->proba;

		for (t = 0; t < 4; t++)
		{
			for (b = 0; b < 8; b++)
			{
				for (c = 0; c < 3; c++)
				{
					for (p = 0; p < 11; p++)
					{
						v = simplewebp__bitread_getbit(&br, simplewebp__coeff_update_proba[t][b][c][p])
							? simplewebp__bitread_getval(&br, 8)
							: simplewebp__coeff_proba0[t][b][c][p];
						proba->bands[t][b].probas[c][p] = v;
					}
				}
			}

			for (b = 0; b < 17; b++)
				proba->bands_ptr[t][b] = &proba->bands[t][simplewebp__bands[b]];
		}

		vp8d->use_skip_proba = simplewebp__bitread_getval(&br, 1);
		if (vp8d->use_skip_proba)
			vp8d->skip_proba = simplewebp__bitread_getval(&br, 8);
	}

	if (br.eof)
		return SIMPLEWEBP_CORRUPT_ERROR;

	vp8d->br = br;
	vp8d->ready = 1;
	return SIMPLEWEBP_NO_ERROR;
}

static void simplewebp__vp8_enter_critical(struct simplewebp__vp8_decoder *vp8d)
{
	vp8d->tl_mb_x = vp8d->tl_mb_y = 0;
	vp8d->br_mb_x = vp8d->mb_w;
	vp8d->br_mb_y = vp8d->mb_h;

	if (vp8d->filter_type > 0)
	{
		int32_t s;
		struct simplewebp__filter_header *filt_hdr;

		filt_hdr = &vp8d->filter_header;

		for (s = 0; s < 4; s++)
		{
			int32_t i4x4, base_level;

			if (vp8d->segment_header.use_segment)
				base_level = vp8d->segment_header.filter_strength[s]
					+ filt_hdr->level * (!vp8d->segment_header.absolute_delta);
			else
				base_level = filt_hdr->level;

			for (i4x4 = 0; i4x4 <= 1; i4x4++)
			{
				struct simplewebp__finfo *info;
				int32_t level;

				info = &vp8d->fstrengths[s][i4x4];
				level = base_level
					+ filt_hdr->ref_lf_delta[0] * filt_hdr->use_lf_delta
					+ filt_hdr->mode_lf_delta[0] * i4x4 * filt_hdr->use_lf_delta;

				level = (level < 0) ? 0 : ((level > 63) ? 63 : level);
				if (level > 0)
				{
					int32_t ilevel = level;

					if (filt_hdr->sharpness > 0)
					{
						ilevel >>= 1 + (filt_hdr->sharpness > 4);

						if (ilevel > 9 - filt_hdr->sharpness)
							ilevel = 9 - filt_hdr->sharpness;
					}

					if (ilevel < 1)
						ilevel = 1;

					info->ilevel = ilevel;
					info->limit = 2 * level + ilevel;
					info->hev_thresh = (level >= 40) + (level >= 15);
				}
				else
					info->limit = 0;

				info->inner = i4x4;
			}
		}
	}
}

static void simplewebp__vp8_parse_intra_mode(struct simplewebp__vp8_decoder *vp8d, int32_t mb_x)
{
	uint8_t *top, *left;
	struct simplewebp__mblockdata *block;
	struct simplewebp__bitread *br;

	top = vp8d->intra_t + 4 * mb_x;
	left = vp8d->intra_l;
	block = vp8d->mb_data + mb_x;
	br = &vp8d->br;

	if (vp8d->segment_header.update_map)
		block->segment = !simplewebp__bitread_getbit(br, vp8d->proba.segments[0])
			? simplewebp__bitread_getbit(br, vp8d->proba.segments[1])
			: simplewebp__bitread_getbit(br, vp8d->proba.segments[2]) + 2;
	else
		block->segment = 0;

	if (vp8d->use_skip_proba)
		block->skip = simplewebp__bitread_getbit(br, vp8d->skip_proba);

	block->is_i4x4 = !simplewebp__bitread_getbit(br, 145);
	if (!block->is_i4x4)
	{
		int32_t ymode = simplewebp__bitread_getbit(br, 156)
			? (simplewebp__bitread_getbit(br, 128) ? 1 : 3)
			: (simplewebp__bitread_getbit(br, 163) ? 2 : 0);

		block->imodes[0] = ymode;
		memset(top, ymode, 4);
		memset(left, ymode, 4);
	}
	else
	{
		uint8_t *modes;
		int32_t y;

		modes = block->imodes;
		for (y = 0; y < 4; y++)
		{
			int32_t ymode, x;

			ymode = left[y];
			for (x = 0; x < 4; x++)
			{
				const uint8_t *const prob = simplewebp__modes_proba[top[x]][ymode];
				ymode = !simplewebp__bitread_getbit(br, prob[0]) ? 0 :
					!simplewebp__bitread_getbit(br, prob[1]) ? 1 :
						!simplewebp__bitread_getbit(br, prob[2]) ? 2 :
							!simplewebp__bitread_getbit(br, prob[3]) ?
								(!simplewebp__bitread_getbit(br, prob[4]) ? 3 :
									(!simplewebp__bitread_getbit(br, prob[5]) ? 4
																				: 5)) :
								(!simplewebp__bitread_getbit(br, prob[6]) ? 6 :
									(!simplewebp__bitread_getbit(br, prob[7]) ? 7 :
										(!simplewebp__bitread_getbit(br, prob[8]) ? 8
																					: 9))
								);
				top[x] = ymode;
			}

			memcpy(modes, top, 4);
			modes += 4;
			left[y] = ymode;
		}
	}

	block->uvmode = !simplewebp__bitread_getbit(br, 142) ? 0 : (
		!simplewebp__bitread_getbit(br, 114) ? 2 : (
			simplewebp__bitread_getbit(br, 183) ? 1 : 3
		));
}

static int32_t simplewebp__vp8_parse_intra_row(struct simplewebp__vp8_decoder *vp8d)
{
	int32_t mb_x;
	
	for (mb_x = 0; mb_x < vp8d->mb_w; mb_x++)
		simplewebp__vp8_parse_intra_mode(vp8d, mb_x);

	return !vp8d->br.eof;
}

static void simplewebp__vp8_init_scanline(struct simplewebp__vp8_decoder *vp8d)
{
	struct simplewebp__mblock *const left = vp8d->mb_info - 1;
	left->nz = 0;
	left->nz_dc = 0;
	memset(vp8d->intra_l, 0, sizeof(vp8d->intra_l));
	vp8d->mb_x = 0;
}

static uint8_t *simplewebp__vp8_alloc_memory(struct simplewebp__vp8_decoder *vp8d, simplewebp_allocator *allocator)
{
	int32_t mb_w;
	size_t intra_pred_mode_size, top_size, mb_info_size, f_info_size;
	size_t yuv_size, mb_data_size, cache_height, cache_size, alpha_size, needed;
	uint8_t *orig_mem, *mem;

	mb_w = vp8d->mb_w;
	intra_pred_mode_size = 4 * mb_w;
	top_size = sizeof(struct simplewebp__topsmp) * mb_w;
	mb_info_size = (mb_w + 1) * sizeof(struct simplewebp__mblock);
	f_info_size = vp8d->filter_type > 0 ? (mb_w * sizeof(struct simplewebp__finfo)) : 0;
	yuv_size = (32 * 17 + 32 * 9) * sizeof(*vp8d->yuv_b);
	mb_data_size = mb_w * sizeof(*vp8d->mb_data);
	cache_height = (16 + simplewebp__fextrarows[vp8d->filter_type]) * 3 / 2;
	cache_size = top_size * cache_height;
	alpha_size = vp8d->alpha_data ? (vp8d->picture_header.width * vp8d->picture_header.width) : 0;

	needed = intra_pred_mode_size
		+ top_size
		+ mb_info_size
		+ f_info_size
		+ yuv_size
		+ mb_data_size
		+ cache_size
		+ alpha_size + 31;

	mem = orig_mem = (uint8_t *) allocator->alloc(needed);

	if (mem)
	{
		vp8d->mem = mem;
		vp8d->mem_size = needed;

		vp8d->intra_t = mem;
		mem += intra_pred_mode_size;

		vp8d->yuv_t = (struct simplewebp__topsmp *) mem;
		mem += top_size;

		vp8d->mb_info = ((struct simplewebp__mblock *) mem) + 1;
		mem += mb_info_size;

		vp8d->f_info = f_info_size ? (struct simplewebp__finfo *) mem : NULL;
		mem += f_info_size;

		mem = simplewebp__align32(mem);
		vp8d->yuv_b = mem;
		mem += yuv_size;

		vp8d->mb_data = (struct simplewebp__mblockdata *) mem;
		mem += mb_data_size;

		vp8d->cache_y_stride = 16 * mb_w;
		vp8d->cache_uv_stride = 8 * mb_w;
		{
			int32_t extra_rows, extra_y, extra_uv;

			extra_rows = simplewebp__fextrarows[vp8d->filter_type];
			extra_y = extra_rows * vp8d->cache_y_stride;
			extra_uv = (extra_rows / 2) * vp8d->cache_uv_stride;
			vp8d->cache_y = mem + extra_y;
			vp8d->cache_u = vp8d->cache_y + 16 * vp8d->cache_y_stride + extra_uv;
			vp8d->cache_v = vp8d->cache_u + 8 * vp8d->cache_uv_stride + extra_uv;
		}
		mem += cache_size;

		vp8d->alpha_plane = alpha_size ? mem : NULL;
		mem += alpha_size;

		memset(vp8d->mb_info - 1, 0, mb_info_size);
		simplewebp__vp8_init_scanline(vp8d);
		memset(vp8d->intra_t, 0, intra_pred_mode_size);
	}

	return orig_mem;
}

static const uint8_t simplewebp__cat3[] = { 173, 148, 140, 0 };
static const uint8_t simplewebp__cat4[] = { 176, 155, 140, 135, 0 };
static const uint8_t simplewebp__cat5[] = { 180, 157, 141, 134, 130, 0 };
static const uint8_t simplewebp__cat6[] = { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0 };
static const uint8_t* const simplewebp__cat3456[] = {
	simplewebp__cat3,
	simplewebp__cat4,
	simplewebp__cat5,
	simplewebp__cat6
};
static const uint8_t simplewebp__zigzag[16] = {0, 1, 4, 8,  5, 2, 3, 6,  9, 12, 13, 10,  7, 11, 14, 15};

static int32_t simplewebp__get_large_value(struct simplewebp__bitread *br, const uint8_t *p)
{
	int32_t v;

	if (!simplewebp__bitread_getbit(br, p[3]))
	{
		if (!simplewebp__bitread_getbit(br, p[4]))
			v = 2;
		else
			v = 3 + simplewebp__bitread_getbit(br, p[5]);
	}
	else
	{
		if (!simplewebp__bitread_getbit(br, p[6]))
		{
			if (!simplewebp__bitread_getbit(br, p[7]))
				v = 5 + simplewebp__bitread_getbit(br, 159);
			else
			{
				v = 7 + 2 * simplewebp__bitread_getbit(br, 165);
				v += simplewebp__bitread_getbit(br, 145);
			}
		}
		else
		{
			const uint8_t *tab;
			int32_t bit1, bit0, cat;

			bit1 = simplewebp__bitread_getbit(br, p[8]);
			bit0 = simplewebp__bitread_getbit(br, p[9 + bit1]);
			cat = 2 * bit1 + bit0;
			v = 0;

			for (tab = simplewebp__cat3456[cat]; *tab; ++tab)
				v += v + simplewebp__bitread_getbit(br, *tab);

			v += 3 + (8 << cat);
		}
	}

	return v;
}

static int32_t simplewebp__get_coeffs(struct simplewebp__bitread *br, const struct simplewebp__bandprobas *prob[], int32_t ctx, const simplewebp__quant_t dq, int32_t n, int16_t *out)
{
	const uint8_t *p = prob[n]->probas[ctx];

	for (; n < 16; n++)
	{
		if (!simplewebp__bitread_getbit(br, p[0]))
			return n;

		while (!simplewebp__bitread_getbit(br, p[1]))
		{
			p = prob[++n]->probas[0];
			if (n == 16)
				return 1;
		}

		{
			const simplewebp__probarray *p_ctx;
			int32_t v;

			p_ctx = &prob[n + 1]->probas[0];

			if (!simplewebp__bitread_getbit(br, p[2]))
			{
				v = 1;
				p = p_ctx[1];
			}
			else
			{
				v = simplewebp__get_large_value(br, p);
				p = p_ctx[2];
			}

			out[simplewebp__zigzag[n]] = simplewebp__bitread_getsigned(br, v) * dq[n > 0];
		}
	}

	return 16;
}

static uint32_t simplewebp__nz_code_bits(uint32_t nz_coeffs, int32_t nz, char dc_nz)
{
	nz_coeffs <<= 2;
	nz_coeffs |= (nz > 3) ? 3 : (nz > 1) ? 2 : dc_nz;
	return nz_coeffs;
}

static int32_t simplewebp__vp8_decode_macroblock(struct simplewebp__vp8_decoder *vp8d, struct simplewebp__bitread *token_br)
{
	struct simplewebp__mblock *left, *mb;
	struct simplewebp__mblockdata *block;
	uint8_t skip;

	left = vp8d->mb_info - 1;
	mb = vp8d->mb_info + vp8d->mb_x;
	block = vp8d->mb_data + vp8d->mb_x;
	skip = vp8d->use_skip_proba ? block->skip : 0;

	if (!skip)
	{
		/* Parse residuals */
		const struct simplewebp__bandprobas * (*bands)[17], **ac_proba;
		struct simplewebp__quantmat *q;
		int16_t *dst;
		struct simplewebp__mblock *left_mb;
		uint8_t tnz, lnz;
		uint32_t non0_y, non0_uv, out_t_nz, out_l_nz;
		int32_t x, y, ch, first;

		bands = vp8d->proba.bands_ptr;
		q = &vp8d->dqm[block->segment];
		dst = block->coeffs;
		left_mb = vp8d->mb_info - 1;
		non0_y = non0_uv = 0;

		memset(dst, 0, 384 * sizeof(int16_t));
		if (!block->is_i4x4)
		{
			int16_t dc[16];
			int32_t ctx, nz;

			memset(dc, 0, sizeof(dc));
			ctx = mb->nz_dc + left_mb->nz_dc;
			nz = simplewebp__get_coeffs(token_br, bands[1], ctx, q->y2_mat, 0, dc);

			mb->nz_dc = left_mb->nz_dc = nz > 0;
			if (nz > 1)
				simplewebp__transform_wht(dc, dst);
			else
			{
				int32_t i, dc0;
				dc0 = (dc[0] + 3) >> 3;
				for (i = 0; i < 16; i++)
					dst[i * 16] = dc0;
			}

			first = 1;
			ac_proba = bands[0];
		}
		else
		{
			first = 0;
			ac_proba = bands[3];
		}

		tnz = mb->nz & 0xf;
		lnz = left_mb->nz & 0xf;

		for (y = 0; y < 4; y++)
		{
			int32_t l;
			uint32_t nz_coeffs;

			l = lnz & 1;
			nz_coeffs = 0;

			for (x = 0; x < 4; x++)
			{
				int32_t ctx, nz;

				ctx = l + (tnz & 1);
				nz = simplewebp__get_coeffs(token_br, ac_proba, ctx, q->y1_mat, first, dst);
				l = nz > first;
				tnz = (tnz >> 1) | (l << 7);
				nz_coeffs = simplewebp__nz_code_bits(nz_coeffs, nz, dst[0] != 0);
				dst += 16;
			}

			tnz >>= 4;
			lnz = (lnz >> 1) | (l << 7);
			non0_y = (non0_y << 8) | nz_coeffs;
		}

		out_t_nz = tnz;
		out_l_nz = lnz >> 4;

		for (ch = 0; ch < 4; ch += 2)
		{
			uint32_t nz_coeffs = 0;

			tnz = mb->nz >> (4 + ch);
			lnz = left_mb->nz >> (4 + ch);

			for (y = 0; y < 2; y++)
			{
				int32_t l = lnz & 1;

				for (x = 0; x < 2; x++)
				{
					int32_t ctx, nz;

					ctx = l + (tnz & 1);
					nz = simplewebp__get_coeffs(token_br, bands[2], ctx, q->uv_mat, 0, dst);
					l = nz > 0;
					tnz = (tnz >> 1) | (l << 3);
					nz_coeffs = simplewebp__nz_code_bits(nz_coeffs, nz, dst[0] != 0);
					dst += 16;
				}

				tnz >>= 2;
				lnz = (lnz >> 1) | (l << 5);
			}

			non0_uv |= nz_coeffs << (4 * ch);
			out_t_nz |= tnz << (4 + ch);
			out_l_nz |= (lnz & 0xf0) << ch;
		}

		mb->nz = out_t_nz;
		left_mb->nz = out_l_nz;

		block->nonzero_y = non0_y;
		block->nonzero_uv = non0_uv;
		block->dither = (non0_uv & 0xaaaa) ? 0 : q->dither;

		skip = !(non0_y | non0_uv);
	}
	else
	{
		left->nz = mb->nz = 0;
		if (!block->is_i4x4)
			left->nz_dc = mb->nz_dc = 0;
		
		block->nonzero_y = block->nonzero_uv = block->dither = 0;
	}

	if (vp8d->filter_type > 0)
	{
		struct simplewebp__finfo *const finfo = vp8d->f_info + vp8d->mb_x;
		*finfo = vp8d->fstrengths[block->segment][block->is_i4x4];
		finfo->inner |= !skip;
	}

	return !token_br->eof;
}

static void simplewebp__do_transform(uint32_t bits, const int16_t *src, uint8_t *dst)
{
	switch (bits >> 30)
	{
		case 3:
			simplewebp__transform(src, dst, 0);
			break;
		case 2:
			simplewebp__transform_ac3(src, dst);
			break;
		case 1:
			simplewebp__transform_dc(src, dst);
			break;
		default:
			break;
	}
}

static int32_t simplewebp__check_mode(int32_t mb_x, int32_t mb_y, int32_t mode)
{
	if (mode == 0) {
		if (mb_x == 0)
			return (mb_y == 0) ? 6 : 5;
		else
			return (mb_y == 0) ? 4 : 0;
	}

	return mode;
}

static void simplewebp__do_transform_uv(uint32_t bits, const int16_t *src, uint8_t *dst)
{
	if (bits & 0xff)
	{
		if (bits & 0xaa)
			simplewebp__transform_uv(src, dst);
		else
			simplewebp__transform_dcuv(src, dst);
	}
}

static int32_t simplewebp__multhi(int32_t v, int32_t coeff)
{
	return (v * coeff) >> 8;
}

static uint8_t simplewebp__yuv2rgb_clip8(int32_t v)
{
	return ((v & ~16383) == 0) ? ((uint8_t) (v >> 6)) : (v < 0) ? 0 : 255;
}

static void simplewebp__yuv2rgb_plain(uint8_t y, uint8_t u, uint8_t v, uint8_t *rgb)
{
	int32_t yhi = simplewebp__multhi(y, 19077);

	rgb[0] = simplewebp__yuv2rgb_clip8(yhi + simplewebp__multhi(v, 26149) - 14234);
	rgb[1] = simplewebp__yuv2rgb_clip8(yhi - simplewebp__multhi(u, 6419) - simplewebp__multhi(v, 13320) + 8708);
	rgb[2] = simplewebp__yuv2rgb_clip8(yhi + simplewebp__multhi(u, 33050) - 17685);
}

static void simplewebp__yuv2rgba(uint8_t *y_out, uint8_t *u_out, uint8_t *v_out, int32_t y_start, int32_t y_end, int32_t y_stride, int32_t uv_stride, int32_t width, uint8_t *rgbout)
{
	int32_t y, x;

	for (y = y_start; y < y_end; y++)
	{
		for (x = 0; x < width; x++)
		{
			int32_t iy, iuv;
			iy = (y - y_start) * y_stride + x;
			iuv = ((y - y_start) / 2) * uv_stride + x / 2;

			simplewebp__yuv2rgb_plain(y_out[iy], u_out[iuv], v_out[iuv], &rgbout[(y * width + x) * 4]);
			rgbout[(y * width + x) * 4 + 3] = 255u;
		}
	}
}

static uint8_t simplewebp__interpolate(uint8_t a, uint8_t b)
{
	return (uint8_t) (((uint32_t) a + b) / 2);

}

static uint8_t simplewebp__interpolate2(uint8_t tl, uint8_t tr, uint8_t bl, uint8_t br)
{
	uint8_t tm, bm;

	tm = simplewebp__interpolate(tl, tr);
	bm = simplewebp__interpolate(bl, br);
	return simplewebp__interpolate(tm, bm);
}

static uint8_t simplewebp__do_uv_fancy_upsampling(uint8_t a, uint8_t b, uint8_t c, uint8_t d, char x, char y)
{
	/* X and Y must be 0 or 1*/
	/*
	Notes on interpolation:
	* The interpolation formula is 2x2 upsampling described in libwebp as follows:
	Say you have UV samples of:
	  [a b]
	  [c d]
	Interpolating the pixels is done using this formula:
	  ([9a + 3b + 3c + d   3a + 9b + c + 3d] + [8 8]) / 16
	  ([3a + b + 9c + 3d   a + 3b + 3c + 9d] + [8 8]) / 16
	"a" and "b" is taken from the row above it (row - 1)
	"a" and "c" is taken from the column before it (column - 1)
	*/

	switch (y * 2 + x)
	{
		case 0:
			return (9u*a + 3u*b + 3u*c + d + 8u) / 16u;
		case 1:
			return (3u*a + 9u*b + c + 3u*d + 8u) / 16u;
		case 2:
			return (3u*a + b + 9u*c + 3u*d + 8u) / 16u;
		case 3:
			return (a + 3u*b + 3u*c + 9u*d + 8u) / 16u;
		default:
			return 0;
	}
}

static uint8_t simplewebp__uv_fancy_upsample(uint8_t *v, size_t left_x, size_t x, size_t top_y, size_t y, size_t w, size_t h, char rx, char ry)
{
	uint8_t a, b, c, d;

	a = v[top_y * w + left_x];
	c = v[y * w + left_x];
	b = v[top_y * w + x];
	d = v[y * w + x];

	return simplewebp__do_uv_fancy_upsampling(a, b, c, d, rx, ry);
}

static void simplewebp__yuva2rgba(struct simplewebp__yuvdst *yuva, size_t w, size_t h, uint8_t *rgba)
{
	size_t y, x, uvw, uvh;
	uint8_t buf[4];

	uvw = (w + 1) / 2;
	uvh = (h + 1) / 2;

	for (y = 0; y < h; y++)
	{
		size_t y_uv;
		y_uv = (y + 1) / 2;
		y_uv = y_uv >= uvh ? (y_uv - 1) : y_uv;

		for (x = 0; x < w; x++)
		{
			size_t x_uv;
			uint8_t uval, vval;
			char hit_b, hit_c;

			x_uv = (x + 1) / 2;
			x_uv = x_uv >= uvw ? (x_uv - 1) : x_uv;
			hit_b = (x & 1) == 0;
			hit_c = (y & 1) == 0;
			uval = simplewebp__uv_fancy_upsample(
				yuva->u,
				x_uv == 0 ? 0 : x_uv - 1,
				x_uv,
				y_uv == 0 ? 0 : y_uv - 1,
				y_uv,
				uvw,
				uvh,
				hit_b, hit_c
			);
			vval = simplewebp__uv_fancy_upsample(
				yuva->v,
				x_uv == 0 ? 0 : x_uv - 1,
				x_uv,
				y_uv == 0 ? 0 : y_uv - 1,
				y_uv,
				uvw,
				uvh,
				hit_b, hit_c
			);

			simplewebp__yuv2rgb_plain(yuva->y[y * w + x], uval, vval, rgba + (y * w + x) * 4);
			rgba[(y * w + x) * 4 + 3] = yuva->a[y * w + x];
		}
	}
}

static simplewebp_error simplewebp__vp8_process_row(struct simplewebp__vp8_decoder *vp8d, struct simplewebp__yuvdst *destination)
{
	int32_t filter_row;

	filter_row = vp8d->filter_type > 0 && vp8d->mb_y >= vp8d->tl_mb_y && vp8d->mb_y <= vp8d->br_mb_y;

	/* Reconstruct row */
	{
		int32_t j, mb_x, mb_y;
		uint8_t *y_dst, *u_dst, *v_dst;

		mb_y = vp8d->mb_y;
		y_dst = vp8d->yuv_b + 40;
		u_dst = vp8d->yuv_b + 584;
		v_dst = vp8d->yuv_b + 600;

		for (j = 0; j < 16; j++)
			y_dst[j * 32 - 1] = 129;
		for (j = 0; j < 8; j++)
		{
			u_dst[j * 32 - 1] = 129;
			v_dst[j * 32 - 1] = 129;
		}

		if (mb_y > 0)
			y_dst[-33] = u_dst[-33] = v_dst[-33] = 129;
		else
		{
			memset(y_dst - 33, 127, 21);
			memset(u_dst - 33, 127, 9);
			memset(v_dst - 33, 127, 9);
		}

		for (mb_x = 0; mb_x < vp8d->mb_w; mb_x++)
		{
			struct simplewebp__mblockdata *const block = vp8d->mb_data + mb_x;

			if (mb_x > 0)
			{
				for (j = -1; j < 16; j++)
					memcpy(y_dst + j * 32 - 4, y_dst + j * 32 + 12, 4);
				for (j = -1; j < 8; j++)
				{
					memcpy(u_dst + j * 32 - 4, u_dst + j * 32 + 4, 4);
					memcpy(v_dst + j * 32 - 4, v_dst + j * 32 + 4, 4);
				}
			}

			{
				struct simplewebp__topsmp *top_yuv;
				const int16_t *coeffs;
				uint32_t bits;
				int32_t n;

				top_yuv = vp8d->yuv_t + mb_x;
				coeffs = block->coeffs;
				bits = block->nonzero_y;

				if (mb_y > 0)
				{
					memcpy(y_dst - 32, top_yuv[0].y, 16);
					memcpy(u_dst - 32, top_yuv[0].u, 8);
					memcpy(v_dst - 32, top_yuv[0].v, 8);
				}

				if (block->is_i4x4)
				{
					uint32_t *const top_right = (uint32_t *) (y_dst - 32 + 16);

					if (mb_y > 0)
					{
						if (mb_x >= vp8d->mb_w - 1)
							memset(top_right, top_yuv[0].y[15], 4);
						else
							memcpy(top_right, top_yuv[1].y, 4);
					}

					top_right[32] = top_right[64] = top_right[96] = top_right[0];

					for (n = 0; n < 16; n++, bits <<= 2)
					{
						/* kScan[n] = (n & 3) * 4 + (n >> 2) * 128 */
						uint8_t *const dst = y_dst + ((n & 3) * 4 + (n >> 2) * 128);
						simplewebp__predluma4(block->imodes[n], dst);
						simplewebp__do_transform(bits, coeffs + n * 16, dst);
					}
				}
				else
				{
					const int pred_func = simplewebp__check_mode(mb_x, mb_y, block->imodes[0]);
					simplewebp__predluma16(pred_func, y_dst);

					if (bits)
					{
						for (n = 0; n < 16; n++, bits <<= 2)
						{
							uint8_t *const dst = y_dst + ((n & 3) * 4 + (n >> 2) * 128);
							simplewebp__do_transform(bits, coeffs + n * 16, dst);
						}
					}
				}

				{
					/* Chroma */
					uint32_t bits_uv;
					int32_t pred_func;

					bits_uv = block->nonzero_uv;
					pred_func = simplewebp__check_mode(mb_x, mb_y, block->uvmode);

					simplewebp__predchroma8(pred_func, u_dst);
					simplewebp__predchroma8(pred_func, v_dst);
					simplewebp__do_transform_uv(bits_uv >> 0, coeffs + 16 * 16, u_dst);
					simplewebp__do_transform_uv(bits_uv >> 8, coeffs + 20 * 16, v_dst);
				}

				if (mb_y < vp8d->mb_h - 1)
				{
					memcpy(top_yuv[0].y, y_dst + 15 * 32, 16);
					memcpy(top_yuv[0].u, u_dst + 7 * 32, 8);
					memcpy(top_yuv[0].v, v_dst + 7 * 32, 8);
				}
			}

			/* Transfer reconstructed samples */
			{
				uint8_t *y_out, *u_out, *v_out;

				y_out = vp8d->cache_y + mb_x * 16;
				u_out = vp8d->cache_u + mb_x * 8;
				v_out = vp8d->cache_v + mb_x * 8;

				for (j = 0; j < 16; j++)
					memcpy(y_out + j * vp8d->cache_y_stride, y_dst + j * 32, 16);
				for (j = 0; j < 8; j++)
				{
					memcpy(u_out + j * vp8d->cache_uv_stride, u_dst + j * 32, 8);
					memcpy(v_out + j * vp8d->cache_uv_stride, v_dst + j * 32, 8);
				}
			}
		}
	}

	/* Finish row */
	{
		int32_t extra_y_rows, ysize, uvsize, mb_y;
		uint8_t *ydst, *udst, *vdst, is_first_row, is_last_row;

		extra_y_rows = simplewebp__fextrarows[vp8d->filter_type];
		ysize = extra_y_rows * vp8d->cache_y_stride;
		uvsize = (extra_y_rows / 2) * vp8d->cache_uv_stride;
		ydst = vp8d->cache_y - ysize;
		udst = vp8d->cache_u - uvsize;
		vdst = vp8d->cache_v - uvsize;
		mb_y = vp8d->mb_y;
		is_first_row = mb_y == 0;
		is_last_row = mb_y >= vp8d->br_mb_y - 1;

		if (filter_row)
		{
			/* Filter row */
			int32_t mb_x;

			for (mb_x = vp8d->tl_mb_x; mb_x < vp8d->br_mb_x; mb_x++)
			{
				int32_t y_bps, ilevel, limit;
				struct simplewebp__finfo *f_info;
				uint8_t *y_dst;

				f_info = vp8d->f_info + mb_x;
				limit = f_info->limit;

				if (limit > 0)
				{
					ilevel = f_info->ilevel;
					y_bps = vp8d->cache_y_stride;
					y_dst = vp8d->cache_y + mb_x * 16;

					if (vp8d->filter_type == 1)
					{
						/* Simple filter */
						if (mb_x > 0)
							simplewebp__simple_hfilter16(y_dst, y_bps, limit + 4);
						if (f_info->inner)
							simplewebp__simple_hfilter16_i(y_dst, y_bps, limit);
						if (mb_y > 0)
							simplewebp__simple_vfilter16(y_dst, y_bps, limit + 4);
						if (f_info->inner)
							simplewebp__simple_vfilter16_i(y_dst, y_bps, limit);
					}
					else
					{
						/* Complex filter */
						int32_t uv_bps, hev_thresh;
						uint8_t *u_dst, *v_dst;

						uv_bps = vp8d->cache_uv_stride;
						hev_thresh = f_info->hev_thresh;
						u_dst = vp8d->cache_u + mb_x * 8;
						v_dst = vp8d->cache_v + mb_x * 8;

						if (mb_x > 0)
						{
							simplewebp__hfilter16(y_dst, y_bps, limit + 4, ilevel, hev_thresh);
							simplewebp__hfilter8(u_dst, v_dst, uv_bps, limit + 4, ilevel, hev_thresh);
						}
						if (f_info->inner)
						{
							simplewebp__hfilter16_i(y_dst, y_bps, limit, ilevel, hev_thresh);
							simplewebp__hfilter8_i(u_dst, v_dst, uv_bps, limit, ilevel, hev_thresh);
						}
						if (mb_y > 0)
						{
							simplewebp__vfilter16(y_dst, y_bps, limit + 4, ilevel, hev_thresh);
							simplewebp__vfilter8(u_dst, v_dst, uv_bps, limit + 4, ilevel, hev_thresh);
						}
						if (f_info->inner)
						{
							simplewebp__vfilter16_i(y_dst, y_bps, limit, ilevel, hev_thresh);
							simplewebp__vfilter8_i(u_dst, v_dst, uv_bps, limit, ilevel, hev_thresh);
						}
					}
				}
			}
		}

		if (vp8d->dither)
		{
			/* TODO dither row */
		}

		{
			/* Write YUV */
			uint8_t *y_out, *u_out, *v_out;
			int32_t y_start, y_end;

			y_start = mb_y * 16;
			y_end = (mb_y + 1) * 16;
			if (!is_first_row)
			{
				y_start -= extra_y_rows;
				y_out = ydst;
				u_out = udst;
				v_out = vdst;
			}
			else
			{
				y_out = vp8d->cache_y;
				u_out = vp8d->cache_u;
				v_out = vp8d->cache_v;
			}

			if (!is_last_row)
				y_end -= extra_y_rows;
			if (y_end > vp8d->picture_header.height)
				y_end = vp8d->picture_header.height;

			if (vp8d->alpha_data)
			{
				/* TODO: Alpha Channel */
			}

			/*simplewebp__yuv2rgba(y_out, u_out, v_out, y_start, y_end, vp8d->cache_y_stride, vp8d->cache_uv_stride, vp8d->picture_header.width, rgbout);*/
			{
				/* Copy YUV buffer */
				int32_t row, iwidth, iwidth2, uv_start, uv_end;

				iwidth = vp8d->picture_header.width;
				iwidth2 = (iwidth + 1) / 2;
				uv_start = y_start / 2;
				uv_end = (y_end + 1) / 2;

				/* Copy Y and A buffer */
				for (row = y_start; row < y_end; row++)
				{
					memcpy(destination->y + row * iwidth, y_out + (row - y_start) * vp8d->cache_y_stride, iwidth);
					/* TODO: Alpha decoding */
					memset(destination->a + row * iwidth, 255u, iwidth);
				}

				/* Copy U and V buffer */
				for (row = uv_start; row < uv_end; row++)
				{
					memcpy(destination->u + row * iwidth2, u_out + (row - uv_start) * vp8d->cache_uv_stride, iwidth2);
					memcpy(destination->v + row * iwidth2, v_out + (row - uv_start) * vp8d->cache_uv_stride, iwidth2);
				}
			}
		}
		
		/* Rotate top samples */
		if (!is_last_row)
		{
			memcpy(vp8d->cache_y - ysize, ydst + 16 * vp8d->cache_y_stride, ysize);
			memcpy(vp8d->cache_u - uvsize, udst + 8 * vp8d->cache_uv_stride, uvsize);
			memcpy(vp8d->cache_v - uvsize, vdst + 8 * vp8d->cache_uv_stride, uvsize);
		}
	}

	return SIMPLEWEBP_NO_ERROR;
}

static simplewebp_error simplewebp__vp8_parse_frame(struct simplewebp__vp8_decoder *vp8d, struct simplewebp__yuvdst *destination)
{
	simplewebp_error err;

	for (vp8d->mb_y = 0; vp8d->mb_y < vp8d->br_mb_y; vp8d->mb_y++)
	{
		struct simplewebp__bitread *token_br = &vp8d->parts[vp8d->mb_y & vp8d->nparts_minus_1];

		if (!simplewebp__vp8_parse_intra_row(vp8d))
			return SIMPLEWEBP_CORRUPT_ERROR;

		for (; vp8d->mb_x < vp8d->mb_w; vp8d->mb_x++)
		{
			if (!simplewebp__vp8_decode_macroblock(vp8d, token_br))
				return SIMPLEWEBP_CORRUPT_ERROR;
		}

		/* Prepare for next scanline and reconstruct it */
		simplewebp__vp8_init_scanline(vp8d);
		err = simplewebp__vp8_process_row(vp8d, destination);

		if (err != SIMPLEWEBP_NO_ERROR)
			return err;
	}

	return SIMPLEWEBP_NO_ERROR;
}

static simplewebp_error simplewebp__decode_lossy(simplewebp *simplewebp, struct simplewebp__yuvdst *destination, void *settings)
{
	simplewebp_input input;
	size_t vp8size;
	uint8_t *vp8buffer, *decoder_mem;
	struct simplewebp__vp8_decoder *vp8d;
	simplewebp_error err;

	vp8d = &simplewebp->decoder.vp8;

	if (!simplewebp__seek(0, &simplewebp->vp8_input))
		return SIMPLEWEBP_IO_ERROR;

	vp8size = simplewebp__proxy_size(simplewebp->vp8_input.userdata);
	/* Sanity check */
	if (vp8d->frame_header.partition_length > vp8size)
		return SIMPLEWEBP_CORRUPT_ERROR;

	/* Read all VP8 chunk */
	vp8buffer = (uint8_t *) simplewebp->allocator.alloc(vp8size);
	if (vp8buffer == NULL)
		return SIMPLEWEBP_ALLOC_ERROR;

	if (!simplewebp__read2(vp8size, vp8buffer, &simplewebp->vp8_input))
	{
		simplewebp->allocator.free(vp8buffer);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	err = simplewebp_input_from_memory(vp8buffer, vp8size, &input, &simplewebp->allocator);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		simplewebp->allocator.free(vp8buffer);
		return err;
	}

	/* Let's skip already-read stuff at simplewebp__load_lossy */
	if (!simplewebp__seek(10, &input))
	{
		simplewebp->allocator.free(vp8buffer);
		simplewebp_close_input(&input);
		return SIMPLEWEBP_CORRUPT_ERROR;
	}

	err = simplewebp__load_header_lossy(&simplewebp->decoder.vp8, vp8buffer + 10, vp8size - 10);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		simplewebp->allocator.free(vp8buffer);
		simplewebp_close_input(&input);
		return err;
	}

	/* Enter critical */
	simplewebp__vp8_enter_critical(&simplewebp->decoder.vp8);
	decoder_mem = simplewebp__vp8_alloc_memory(vp8d, &simplewebp->allocator);

	if (decoder_mem == NULL)
	{
		simplewebp->allocator.free(vp8buffer);
		simplewebp_close_input(&input);
		return SIMPLEWEBP_ALLOC_ERROR;
	}

	err = simplewebp__vp8_parse_frame(vp8d, destination);
	if (err != SIMPLEWEBP_NO_ERROR)
	{
		simplewebp->allocator.free(decoder_mem);
		simplewebp->allocator.free(vp8buffer);
		simplewebp_close_input(&input);
		return err;
	}

	/* Exit critical and cleanup */
	simplewebp->allocator.free(decoder_mem);
	simplewebp->allocator.free(vp8buffer);
	vp8d->mem = NULL;
	vp8d->mem_size = 0;
	memset(&vp8d->br, 0, sizeof(struct simplewebp__bitread));
	vp8d->ready = 0;
	simplewebp_close_input(&input);

	return SIMPLEWEBP_NO_ERROR;
}

simplewebp_error simplewebp_decode_yuva(simplewebp *simplewebp, void *y_buffer, void *u_buffer, void *v_buffer, void *a_buffer, void *settings)
{
	if (simplewebp->webp_type == 0)
	{
		struct simplewebp__yuvdst destination;
		destination.y = (uint8_t *) y_buffer;
		destination.u = (uint8_t *) u_buffer;
		destination.v = (uint8_t *) u_buffer;
		destination.a = (uint8_t *) u_buffer;
		return simplewebp__decode_lossy(simplewebp, &destination, settings);
	}

	return SIMPLEWEBP_IS_LOSSLESS_ERROR;
}

simplewebp_error simplewebp_decode(simplewebp *simplewebp, void *buffer, void *settings)
{

	if (simplewebp->webp_type == 0)
	{
		struct simplewebp__yuvdst dest;
		simplewebp_error err;
		uint8_t *mem, *orig_mem;
		size_t needed, yw, yh, uvw, uvh;

		/* Calculate allocation size. The allocated memory layout is [y, a, u, v] */
		yw = simplewebp->decoder.vp8.picture_header.width;
		yh = simplewebp->decoder.vp8.picture_header.height;
		uvw = (yw + 1) / 2;
		uvh = (yh + 1) / 2;
		needed = (yw * yh * 2) + (uvw * uvh * 2);

		mem = orig_mem = (uint8_t *) simplewebp->allocator.alloc(needed);
		if (mem == NULL)
			return SIMPLEWEBP_ALLOC_ERROR;

		dest.y = mem;
		mem += yw * yh;
		dest.a = mem;
		mem += yw * yh;
		dest.u = mem;
		mem += uvw * uvh;
		dest.v = mem;

		err = simplewebp__decode_lossy(simplewebp, &dest, settings);
		if (err != SIMPLEWEBP_NO_ERROR)
		{
			simplewebp->allocator.free(orig_mem);
			return err;
		}

		simplewebp__yuva2rgba(&dest, yw, yh, (uint8_t *) buffer);
		simplewebp->allocator.free(orig_mem);
		return SIMPLEWEBP_NO_ERROR;
	}
	else
		/* TODO */
		return SIMPLEWEBP_UNSUPPORTED_ERROR;
}

#ifndef SIMPLEWEBP_DISABLE_STDIO

static size_t simplewebp__stdfile_read(size_t size, void *dest, void *userdata)
{
	FILE *f = (FILE *) userdata;
	return fread(dest, 1, size, f);
}

static int32_t simplewebp__stdfile_seek(size_t pos, void *userdata)
{
	FILE *f = (FILE *) userdata;
	return fseek(f, (long) pos, SEEK_SET) == 0;
}

static void simplewebp__stdfile_close(void *userdata)
{
	FILE *f = (FILE *) userdata;
	fclose(f);
}

static size_t simplewebp__stdfile_tell(void *userdata)
{
	FILE *f = (FILE *) userdata;
	return ftell(f);
}

simplewebp_error simplewebp_input_from_file(FILE *file, simplewebp_input *out)
{
	out->userdata = file;
	out->read = simplewebp__stdfile_read;
	out->seek = simplewebp__stdfile_seek;
	out->tell = simplewebp__stdfile_tell;
	out->close = simplewebp__stdfile_close;
	return SIMPLEWEBP_NO_ERROR;
}

simplewebp_error simplewebp_input_from_filename(const char *filename, simplewebp_input *out)
{
	FILE *f = fopen(filename, "rb");
	if (f == NULL)
		return SIMPLEWEBP_IO_ERROR;

	return simplewebp_input_from_file(f, out);
}

simplewebp_error simplewebp_load_from_file(FILE *file, const simplewebp_allocator *allocator, simplewebp **out)
{
	simplewebp_input input;
	simplewebp_error err;

	err = simplewebp_input_from_file(file, &input);
	if (err != SIMPLEWEBP_NO_ERROR)
		return err;

	err = simplewebp_load(&input, allocator, out);
	if (err != SIMPLEWEBP_NO_ERROR)
		simplewebp_close_input(&input);

	return err;
}

simplewebp_error simplewebp_load_from_filename(const char *filename, const simplewebp_allocator *allocator, simplewebp **out)
{
	simplewebp_input input;
	simplewebp_error err;

	err = simplewebp_input_from_filename(filename, &input);
	if (err != SIMPLEWEBP_NO_ERROR)
		return err;

	err = simplewebp_load(&input, allocator, out);
	if (err != SIMPLEWEBP_NO_ERROR)
		simplewebp_close_input(&input);

	return err;
}
#endif /* SIMPLEWEBP_DISABLE_STDIO */

#endif /* SIMPLEWEBP_IMPLEMENTATION */

/**
 * This software is available under BSD-3-Clause License
 * 
 * Copyright (c) 2010 Google Inc., 2023 Miku AuahDark
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *    following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Additional IP Rights Grant (Patents)
 * ------------------------------------
 * 
 * "These implementations" means the copyrightable works that implement the WebM codecs distributed by Google as part
 * of the WebM Project.
 * 
 * Google hereby grants to you a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable (except as
 * stated in this section) patent license to make, have made, use, offer to sell, sell, import, transfer, and
 * otherwise run, modify and propagate the contents of these implementations of WebM, where such license applies only
 * to those patent claims, both currently owned by Google and acquired in the future, licensable by Google that are
 * necessarily infringed by these implementations of WebM. This grant does not include claims that would be infringed
 * only as a consequence of further modification of these implementations. If you or your agent or exclusive licensee
 * institute or order or agree to the institution of patent litigation or any other patent enforcement activity
 * against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that any of these
 * implementations of WebM or any code incorporated within any of these implementations of WebM constitute direct or
 * contributory patent infringement, or inducement of patent infringement, then any patent rights granted to you under
 * this License for these implementations of WebM shall terminate as of the date such litigation is filed.
 */
