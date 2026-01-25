/*
 * This file is part of NetSurf's LibNSGIF, http://www.netsurf-browser.org/
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * Copyright 2017 Michael Drake <michael.drake@codethink.co.uk>
 * Copyright 2021 Michael Drake <tlsa@netsurf-browser.org>
 */

#include "fl/stl/cstddef.h"
#include "fl/stl/assert.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"

#include "lzw.h"

namespace fl {
namespace third_party {

/**
 * \file
 * \brief LZW decompression (implementation)
 *
 * Decoder for GIF LZW data.
 */

/** Maximum number of lzw table entries. */
#define LZW_TABLE_ENTRY_MAX (1u << LZW_CODE_MAX)

/**
 * Context for reading LZW data.
 *
 * LZW data is split over multiple sub-blocks.  Each sub-block has a
 * byte at the start, which says the sub-block size, and then the data.
 * Zero-size sub-blocks have no data, and the biggest sub-block size is
 * 255, which means there are 255 bytes of data following the sub-block
 * size entry.
 *
 * Note that an individual LZW code can be split over up to three sub-blocks.
 */
struct lzw_read_ctx {
	const fl::u8 * data; /**< Pointer to start of input data */
	fl::size data_len;              /**< Input data length */
	fl::size data_sb_next;          /**< Offset to sub-block size */

	const fl::u8 *sb_data; /**< Pointer to current sub-block in data */
	fl::size sb_bit;          /**< Current bit offset in sub-block */
	fl::u32 sb_bit_count;  /**< Bit count in sub-block */
};

/**
 * LZW table entry.
 *
 * Records in the table are composed of 1 or more entries.
 * Entries refer to the entry they extend which can be followed to compose
 * the complete record.  To compose the record in reverse order, take
 * the `value` from each entry, and move to the entry it extends.
 * If the extended entries index is < the current clear_code, then it
 * is the last entry in the record.
 */
struct lzw_table_entry {
	fl::u8  value;   /**< Last value for record ending at entry. */
	fl::u8  first;   /**< First value in entry's entire record. */
	fl::u16 count;   /**< Count of values in this entry's record. */
	fl::u16 extends; /**< Offset in table to previous entry. */
};

/**
 * LZW decompression context.
 */
struct lzw_ctx {
	struct lzw_read_ctx input; /**< Input reading context */

	fl::u16 prev_code;       /**< Code read from input previously. */
	fl::u16 prev_code_first; /**< First value of previous code. */
	fl::u16 prev_code_count; /**< Total values for previous code. */

	fl::u8  initial_code_size; /**< Starting LZW code size. */

	fl::u8  code_size; /**< Current LZW code size. */
	fl::u16 code_max;  /**< Max code value for current code size. */

	fl::u16 clear_code; /**< Special Clear code value */
	fl::u16 eoi_code;   /**< Special End of Information code value */

	fl::u16 table_size; /**< Next position in table to fill. */

	fl::u16 output_code; /**< Code that has been partially output. */
	fl::u16 output_left; /**< Number of values left for output_code. */

	bool     has_transparency;     /**< Whether the image is opaque. */
	fl::u8  transparency_idx;     /**< Index representing transparency. */
	const fl::u32 * colour_map; /**< Index to colour mapping. */

	/** LZW code table. Generated during decode. */
	struct lzw_table_entry table[LZW_TABLE_ENTRY_MAX];

	/** Output value stack. */
	fl::u8 stack_base[LZW_TABLE_ENTRY_MAX];
};

/* Exported function, documented in lzw.h */
lzw_result lzw_context_create(struct lzw_ctx **ctx)
{
	struct lzw_ctx *c = static_cast<struct lzw_ctx*>(fl::Malloc(sizeof(*c)));
	if (c == nullptr) {
		return LZW_NO_MEM;
	}

	*ctx = c;
	return LZW_OK;
}

/* Exported function, documented in lzw.h */
void lzw_context_destroy(struct lzw_ctx *ctx)
{
	fl::Free(ctx);
}

/**
 * Advance the context to the next sub-block in the input data.
 *
 * \param[in] ctx  LZW reading context, updated on success.
 * \return LZW_OK or LZW_OK_EOD on success, appropriate error otherwise.
 */
static lzw_result lzw__block_advance(struct lzw_read_ctx * ctx)
{
	fl::size block_size;
	fl::size next_block_pos = ctx->data_sb_next;
	const fl::u8 *data_next = ctx->data + next_block_pos;

	if (next_block_pos >= ctx->data_len) {
		return LZW_NO_DATA;
	}

	block_size = *data_next;

	if ((next_block_pos + block_size) >= ctx->data_len) {
		return LZW_NO_DATA;
	}

	ctx->sb_bit = 0;
	ctx->sb_bit_count = block_size * 8;

	if (block_size == 0) {
		ctx->data_sb_next += 1;
		return LZW_OK_EOD;
	}

	ctx->sb_data = data_next + 1;
	ctx->data_sb_next += block_size + 1;

	return LZW_OK;
}

/**
 * Get the next LZW code of given size from the raw input data.
 *
 * Reads codes from the input data stream coping with GIF data sub-blocks.
 *
 * \param[in]  ctx        LZW reading context, updated.
 * \param[in]  code_size  Size of LZW code to get from data.
 * \param[out] code_out   Returns an LZW code on success.
 * \return LZW_OK or LZW_OK_EOD on success, appropriate error otherwise.
 */
static inline lzw_result lzw__read_code(
		struct lzw_read_ctx * ctx,
		fl::u16 code_size,
		fl::u16 * code_out)
{
	fl::u32 code = 0;
	fl::u32 current_bit = ctx->sb_bit & 0x7;

	if (ctx->sb_bit + 24 <= ctx->sb_bit_count) {
		/* Fast path: read three bytes from this sub-block */
		const fl::u8 *data = ctx->sb_data + (ctx->sb_bit >> 3);
		code |= static_cast<fl::u32>(*data++) <<  0;
		code |= static_cast<fl::u32>(*data++) <<  8;
		code |= static_cast<fl::u32>(*data)   << 16;
		ctx->sb_bit += code_size;
	} else {
		/* Slow path: code spans sub-blocks */
		fl::u8 byte_advance = (current_bit + code_size) >> 3;
		fl::u8 byte = 0;
		fl::u8 bits_remaining_0 = (code_size < (8u - current_bit)) ?
				code_size : (8u - current_bit);
		fl::u8 bits_remaining_1 = code_size - bits_remaining_0;
		fl::u8 bits_used[3] = {
			bits_remaining_0,
			(fl::u8)(bits_remaining_1 < 8 ? bits_remaining_1 : 8),
			(fl::u8)(bits_remaining_1 - 8),
		};

		FL_ASSERT(byte_advance <= 2, "Byte advance too large");

		while (true) {
			const fl::u8 *data = ctx->sb_data;
			lzw_result res;

			/* Get any data from end of this sub-block */
			while (byte <= byte_advance &&
					ctx->sb_bit < ctx->sb_bit_count) {
				code |= data[ctx->sb_bit >> 3] << (byte << 3);
				ctx->sb_bit += bits_used[byte];
				byte++;
			}

			/* Check if we have all we need */
			if (byte > byte_advance) {
				break;
			}

			/* Move to next sub-block */
			res = lzw__block_advance(ctx);
			if (res != LZW_OK) {
				return res;
			}
		}
	}

	*code_out = (code >> current_bit) & ((1 << code_size) - 1);
	return LZW_OK;
}

/**
 * Handle clear code.
 *
 * \param[in]   ctx       LZW reading context, updated.
 * \param[out]  code_out  Returns next code after a clear code.
 * \return LZW_OK or error code.
 */
static inline lzw_result lzw__handle_clear(
		struct lzw_ctx *ctx,
		fl::u16 *code_out)
{
	fl::u16 code = 0;

	/* Reset table building context */
	ctx->code_size = ctx->initial_code_size;
	ctx->code_max = (1 << ctx->initial_code_size) - 1;
	ctx->table_size = ctx->eoi_code + 1;

	/* There might be a sequence of clear codes, so process them all */
	do {
		lzw_result res = lzw__read_code(&ctx->input,
				ctx->code_size, &code);
		if (res != LZW_OK) {
			return res;
		}
	} while (code == ctx->clear_code);

	/* The initial code must be from the initial table. */
	if (code > ctx->clear_code) {
		return LZW_BAD_ICODE;
	}

	*code_out = code;
	return LZW_OK;
}

/* Exported function, documented in lzw.h */
lzw_result lzw_decode_init(
		struct lzw_ctx *ctx,
		fl::u8 minimum_code_size,
		const fl::u8 *input_data,
		fl::size input_length,
		fl::size input_pos)
{
	struct lzw_table_entry *table = ctx->table;
	lzw_result res;
	fl::u16 code;

	if (minimum_code_size >= LZW_CODE_MAX) {
		return LZW_BAD_ICODE;
	}

	/* Initialise the input reading context */
	ctx->input.data = input_data;
	ctx->input.data_len = input_length;
	ctx->input.data_sb_next = input_pos;

	ctx->input.sb_bit = 0;
	ctx->input.sb_bit_count = 0;

	/* Initialise the table building context */
	ctx->initial_code_size = minimum_code_size + 1;

	ctx->clear_code = (1 << minimum_code_size) + 0;
	ctx->eoi_code   = (1 << minimum_code_size) + 1;

	ctx->output_left = 0;

	/* Initialise the standard table entries */
	for (fl::u16 i = 0; i < ctx->clear_code; i++) {
		table[i].first = i;
		table[i].value = i;
		table[i].count = 1;
	}

	res = lzw__handle_clear(ctx, &code);
	if (res != LZW_OK) {
		return res;
	}

	/* Store details of this code as "previous code" to the context. */
	ctx->prev_code_first = ctx->table[code].first;
	ctx->prev_code_count = ctx->table[code].count;
	ctx->prev_code = code;

	/* Add code to context for immediate output. */
	ctx->output_code = code;
	ctx->output_left = 1;

	ctx->has_transparency = false;
	ctx->transparency_idx = 0;
	ctx->colour_map = nullptr;

	return LZW_OK;
}

/* Exported function, documented in lzw.h */
lzw_result lzw_decode_init_map(
		struct lzw_ctx *ctx,
		fl::u8 minimum_code_size,
		fl::u32 transparency_idx,
		const fl::u32 *colour_table,
		const fl::u8 *input_data,
		fl::size input_length,
		fl::size input_pos)
{
	lzw_result res;

	if (colour_table == nullptr) {
		return LZW_BAD_PARAM;
	}

	res = lzw_decode_init(ctx, minimum_code_size,
			input_data, input_length, input_pos);
	if (res != LZW_OK) {
		return res;
	}

	ctx->has_transparency = (transparency_idx <= 0xFF);
	ctx->transparency_idx = transparency_idx;
	ctx->colour_map = colour_table;

	return LZW_OK;
}

/**
 * Create new table entry.
 *
 * \param[in]  ctx   LZW reading context, updated.
 * \param[in]  code  Last value code for new table entry.
 */
static inline void lzw__table_add_entry(
		struct lzw_ctx *ctx,
		fl::u16 code)
{
	struct lzw_table_entry *entry = &ctx->table[ctx->table_size];

	entry->value = code;
	entry->first = ctx->prev_code_first;
	entry->count = ctx->prev_code_count + 1;
	entry->extends = ctx->prev_code;

	ctx->table_size++;
}

typedef fl::u32 (*lzw_writer_fn)(
		struct lzw_ctx *ctx,
		void * output_data,
		fl::u32 output_length,
		fl::u32 output_pos,
		fl::u16 code,
		fl::u16 left);

/**
 * Get the next LZW code and write its value(s) to output buffer.
 *
 * \param[in]     ctx             LZW reading context, updated.
 * \param[in]     write_fn        Function for writing pixels to output.
 * \param[in]     output_data     Array to write output values into.
 * \param[in]     output_length   Size of output array.
 * \param[in,out] output_written  Number of values written. Updated on exit.
 * \return LZW_OK on success, or appropriate error code otherwise.
 */
static inline lzw_result lzw__decode(
		struct lzw_ctx    *ctx,
		lzw_writer_fn      write_fn,
		void     * output_data,
		fl::u32          output_length,
		fl::u32 * output_written)
{
	lzw_result res;
	fl::u16 code = 0;

	/* Get a new code from the input */
	res = lzw__read_code(&ctx->input, ctx->code_size, &code);
	if (res != LZW_OK) {
		return res;
	}

	/* Handle the new code */
	if (code == ctx->eoi_code) {
		/* Got End of Information code */
		return LZW_EOI_CODE;

	} else if (code > ctx->table_size) {
		/* Code is invalid */
		return LZW_BAD_CODE;

	} else if (code == ctx->clear_code) {
		res = lzw__handle_clear(ctx, &code);
		if (res != LZW_OK) {
			return res;
		}

	} else if (ctx->table_size < LZW_TABLE_ENTRY_MAX) {
		fl::u16 size = ctx->table_size;
		lzw__table_add_entry(ctx, (code < size) ?
				ctx->table[code].first :
				ctx->prev_code_first);

		/* Ensure code size is increased, if needed. */
		if (size == ctx->code_max && ctx->code_size < LZW_CODE_MAX) {
			ctx->code_size++;
			ctx->code_max = (1 << ctx->code_size) - 1;
		}
	}

	*output_written += write_fn(ctx,
			output_data, output_length, *output_written,
			code, ctx->table[code].count);

	/* Store details of this code as "previous code" to the context. */
	ctx->prev_code_first = ctx->table[code].first;
	ctx->prev_code_count = ctx->table[code].count;
	ctx->prev_code = code;

	return LZW_OK;
}

/**
 * Write values for this code to the output stack.
 *
 * If there isn't enough space in the output stack, this function will write
 * the as many as it can into the output.  If `ctx->output_left > 0` after
 * this call, then there is more data for this code left to output.  The code
 * is stored to the context as `ctx->output_code`.
 *
 * \param[in]  ctx           LZW reading context, updated.
 * \param[in]  output_data   Array to write output values into.
 * \param[in]  output_length length  Size of output array.
 * \param[in]  output_used   Current position in output array.
 * \param[in]  code          LZW code to output values for.
 * \param[in]  left          Number of values remaining to output for this code.
 * \return Number of pixel values written.
 */
static inline fl::u32 lzw__write_fn(struct lzw_ctx *ctx,
		void * output_data,
		fl::u32 output_length,
		fl::u32 output_used,
		fl::u16 code,
		fl::u16 left)
{
	fl::u8 * output_pos = (fl::u8 *)output_data + output_used;
	const struct lzw_table_entry * const table = ctx->table;
	fl::u32 space = output_length - output_used;
	fl::u16 count = left;

	if (count > space) {
		left = count - space;
		count = space;
	} else {
		left = 0;
	}

	ctx->output_code = code;
	ctx->output_left = left;

	/* Skip over any values we don't have space for. */
	for (unsigned i = left; i != 0; i--) {
		const struct lzw_table_entry *entry = table + code;
		code = entry->extends;
	}

	output_pos += count;
	for (unsigned i = count; i != 0; i--) {
		const struct lzw_table_entry *entry = table + code;
		*--output_pos = entry->value;
		code = entry->extends;
	}

	return count;
}

/* Exported function, documented in lzw.h */
lzw_result lzw_decode(struct lzw_ctx *ctx,
		const fl::u8 * *const output_data,
		fl::u32 * output_written)
{
	const fl::u32 output_length = sizeof(ctx->stack_base);

	*output_written = 0;
	*output_data = ctx->stack_base;

	if (ctx->output_left != 0) {
		*output_written += lzw__write_fn(ctx,
				ctx->stack_base, output_length, *output_written,
				ctx->output_code, ctx->output_left);
	}

	while (*output_written != output_length) {
		lzw_result res = lzw__decode(ctx, lzw__write_fn,
				ctx->stack_base, output_length, output_written);
		if (res != LZW_OK) {
			return res;
		}
	}

	return LZW_OK;
}

/**
 * Write colour mapped values for this code to the output.
 *
 * If there isn't enough space in the output stack, this function will write
 * the as many as it can into the output.  If `ctx->output_left > 0` after
 * this call, then there is more data for this code left to output.  The code
 * is stored to the context as `ctx->output_code`.
 *
 * \param[in]  ctx            LZW reading context, updated.
 * \param[in]  output_data    Array to write output values into.
 * \param[in]  output_length  Size of output array.
 * \param[in]  output_used    Current position in output array.
 * \param[in]  code           LZW code to output values for.
 * \param[in]  left           Number of values remaining to output for code.
 * \return Number of pixel values written.
 */
static inline fl::u32 lzw__map_write_fn(struct lzw_ctx *ctx,
		void * output_data,
		fl::u32 output_length,
		fl::u32 output_used,
		fl::u16 code,
		fl::u16 left)
{
	fl::u32 * output_pos = (fl::u32 *)output_data + output_used;
	const struct lzw_table_entry * const table = ctx->table;
	fl::u32 space = output_length - output_used;
	fl::u16 count = left;

	if (count > space) {
		left = count - space;
		count = space;
	} else {
		left = 0;
	}

	ctx->output_code = code;
	ctx->output_left = left;

	for (unsigned i = left; i != 0; i--) {
		const struct lzw_table_entry *entry = table + code;
		code = entry->extends;
	}

	output_pos += count;
	if (ctx->has_transparency) {
		for (unsigned i = count; i != 0; i--) {
			const struct lzw_table_entry *entry = table + code;
			--output_pos;
			if (entry->value != ctx->transparency_idx) {
				*output_pos = ctx->colour_map[entry->value];
			}
			code = entry->extends;
		}
	} else {
		for (unsigned i = count; i != 0; i--) {
			const struct lzw_table_entry *entry = table + code;
			*--output_pos = ctx->colour_map[entry->value];
			code = entry->extends;
		}
	}

	return count;
}

/* Exported function, documented in lzw.h */
lzw_result lzw_decode_map(struct lzw_ctx *ctx,
		fl::u32 * output_data,
		fl::u32 output_length,
		fl::u32 * output_written)
{
	*output_written = 0;

	if (ctx->colour_map == nullptr) {
		return LZW_NO_COLOUR;
	}

	if (ctx->output_left != 0) {
		*output_written += lzw__map_write_fn(ctx,
				output_data, output_length, *output_written,
				ctx->output_code, ctx->output_left);
	}

	while (*output_written != output_length) {
		lzw_result res = lzw__decode(ctx, lzw__map_write_fn,
				output_data, output_length, output_written);
		if (res != LZW_OK) {
			return res;
		}
	}

	return LZW_OK;
}

} // namespace third_party
} // namespace fl
