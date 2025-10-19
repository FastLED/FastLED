/*
 * Copyright 2004 Richard Wilson <richard.wilson@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 * Copyright 2013-2022 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf's libnsgif, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stddef.h>  // For NULL definition

#include "fl/assert.h"
#include "fl/int.h"
#include "fl/allocator.h"
#include "fl/memfill.h"
#include "fl/string.h"
#include "fl/str.h"

#include "lzw.h"
#include "../include/nsgif.hpp"

namespace fl {
namespace third_party {

/** Default minimum allowable frame delay in cs. */
#define NSGIF_FRAME_DELAY_MIN 2

/**
 * Default frame delay to apply.
 *
 * Used when a frame delay lower than the minimum is requested.
 */
#define NSGIF_FRAME_DELAY_DEFAULT 10

/** GIF frame data */
typedef struct nsgif_frame {
	struct nsgif_frame_info info;

	/** offset (in bytes) to the GIF frame data */
	fl::size frame_offset;
	/** whether the frame has previously been decoded. */
	bool decoded;
	/** whether the frame is totally opaque */
	bool opaque;
	/** whether a full image redraw is required */
	bool redraw_required;

	/** Amount of LZW data found in scan */
	fl::u32 lzw_data_length;

	/** the index designating a transparent pixel */
	fl::u32 transparency_index;

	/** offset to frame colour table */
	fl::u32 colour_table_offset;

	/* Frame flags */
	fl::u32 flags;
} nsgif_frame;

/** Pixel format: colour component order. */
struct nsgif_colour_layout {
	fl::u8 r; /**< Byte offset within pixel to red component. */
	fl::u8 g; /**< Byte offset within pixel to green component. */
	fl::u8 b; /**< Byte offset within pixel to blue component. */
	fl::u8 a; /**< Byte offset within pixel to alpha component. */
};

/** GIF animation data */
struct nsgif {
	struct nsgif_info info;

	/** LZW decode context */
	void *lzw_ctx;
	/** callbacks for bitmap functions */
	nsgif_bitmap_cb_vt bitmap;
	/** decoded frames */
	nsgif_frame *frames;
	/** current frame */
	fl::u32 frame;
	/** current frame decoded to bitmap */
	fl::u32 decoded_frame;

	/** currently decoded image; stored as bitmap from bitmap_create callback */
	nsgif_bitmap_t *frame_image;
	/** Row span of frame_image in pixels. */
	fl::u32 rowspan;

	/** Minimum allowable frame delay. */
	fl::u16 delay_min;

	/** Frame delay to apply when delay is less than \ref delay_min. */
	fl::u16 delay_default;

	/** number of animation loops so far */
	int loop_count;

	/** number of frames partially decoded */
	fl::u32 frame_count_partial;

	/**
	 * Whether all the GIF data has been supplied, or if there may be
	 * more to come.
	 */
	bool data_complete;

	/** pointer to GIF data */
	const fl::u8 *buf;
	/** current index into GIF data */
	fl::size buf_pos;
	/** total number of bytes of GIF data available */
	fl::size buf_len;

	/** current number of frame holders */
	fl::u32 frame_holders;
	/** background index */
	fl::u32 bg_index;
	/** image aspect ratio (ignored) */
	fl::u32 aspect_ratio;
	/** size of global colour table (in entries) */
	fl::u32 colour_table_size;

	/** current colour table */
	fl::u32 *colour_table;
	/** Client's colour component order. */
	struct nsgif_colour_layout colour_layout;
	/** global colour table */
	fl::u32 global_colour_table[NSGIF_MAX_COLOURS];
	/** local colour table */
	fl::u32 local_colour_table[NSGIF_MAX_COLOURS];

	/** previous frame for NSGIF_FRAME_RESTORE */
	void *prev_frame;
	/** previous frame index */
	fl::u32 prev_index;
};

/**
 * Helper macro to get number of elements in an array.
 *
 * \param[in]  _a  Array to count elements of.
 * \return NUlber of elements in array.
 */
#define NSGIF_ARRAY_LEN(_a) ((sizeof(_a)) / (sizeof(*_a)))

/**
 *
 * \file
 * \brief GIF image decoder
 *
 * The GIF format is thoroughly documented; a full description can be found at
 * http://www.w3.org/Graphics/GIF/spec-gif89a.txt
 *
 * \todo Plain text and comment extensions should be implemented.
 */

/** Internal flag that the colour table needs to be processed */
#define NSGIF_PROCESS_COLOURS 0xaa000000

/** Internal flag that a frame is invalid/unprocessed */
#define NSGIF_FRAME_INVALID UINT32_MAX

/** Transparent colour */
#define NSGIF_TRANSPARENT_COLOUR 0x00

/** No transparency */
#define NSGIF_NO_TRANSPARENCY (0xFFFFFFFFu)

/* GIF Flags */
#define NSGIF_COLOUR_TABLE_MASK 0x80
#define NSGIF_COLOUR_TABLE_SIZE_MASK 0x07
#define NSGIF_BLOCK_TERMINATOR 0x00
#define NSGIF_TRAILER 0x3b

/**
 * Convert an LZW result code to equivalent GIF result code.
 *
 * \param[in]  l_res  LZW response code.
 * \return GIF result code.
 */
static nsgif_error nsgif__error_from_lzw(lzw_result l_res)
{
	static const nsgif_error g_res[] = {
		NSGIF_OK,                 /* LZW_OK */
		NSGIF_ERR_END_OF_DATA,    /* LZW_OK_EOD */
		NSGIF_ERR_OOM,            /* LZW_NO_MEM */
		NSGIF_ERR_END_OF_DATA,    /* LZW_NO_DATA */
		NSGIF_ERR_DATA_FRAME,     /* LZW_EOI_CODE */
		NSGIF_ERR_DATA_FRAME,     /* LZW_NO_COLOUR */
		NSGIF_ERR_DATA_FRAME,     /* LZW_BAD_ICODE */
		NSGIF_ERR_DATA_FRAME,     /* LZW_BAD_PARAM */
		NSGIF_ERR_DATA_FRAME,     /* LZW_BAD_CODE */
	};
	FL_ASSERT(l_res != LZW_BAD_PARAM, "LZW bad parameter");
	FL_ASSERT(l_res != LZW_NO_COLOUR, "LZW no colour");
	return g_res[l_res];
}

/**
 * Updates the sprite memory size
 *
 * \param gif The animation context
 * \param width The width of the sprite
 * \param height The height of the sprite
 * \return NSGIF_ERR_OOM for a memory error NSGIF_OK for success
 */
static nsgif_error nsgif__initialise_sprite(
		struct nsgif *gif,
		fl::u32 width,
		fl::u32 height)
{
	/* Already allocated? */
	if (gif->frame_image) {
		return NSGIF_OK;
	}

	FL_ASSERT(gif->bitmap.create, "GIF bitmap create function required");
	gif->frame_image = gif->bitmap.create(width, height);
	if (gif->frame_image == NULL) {
		return NSGIF_ERR_OOM;
	}

	return NSGIF_OK;
}

/**
 * Helper to get the rendering bitmap for a gif.
 *
 * \param[in]  gif  The gif object we're decoding.
 * \return Client pixel buffer for rendering into.
 */
static inline fl::u32* nsgif__bitmap_get(
		struct nsgif *gif)
{
	nsgif_error ret;

	/* Make sure we have a buffer to decode to. */
	ret = nsgif__initialise_sprite(gif, gif->info.width, gif->info.height);
	if (ret != NSGIF_OK) {
		return NULL;
	}

	gif->rowspan = gif->info.width;
	if (gif->bitmap.get_rowspan) {
		gif->rowspan = gif->bitmap.get_rowspan(gif->frame_image);
	}

	/* Get the frame data */
	FL_ASSERT(gif->bitmap.get_buffer, "GIF bitmap get_buffer function required");
	return reinterpret_cast<fl::u32*>(gif->bitmap.get_buffer(gif->frame_image));
}

/**
 * Helper to tell the client that their bitmap was modified.
 *
 * \param[in]  gif  The gif object we're decoding.
 */
static inline void nsgif__bitmap_modified(
		const struct nsgif *gif)
{
	if (gif->bitmap.modified) {
		gif->bitmap.modified(gif->frame_image);
	}
}

/**
 * Helper to tell the client that whether the bitmap is opaque.
 *
 * \param[in]  gif    The gif object we're decoding.
 * \param[in]  frame  The frame that has been decoded.
 */
static inline void nsgif__bitmap_set_opaque(
		const struct nsgif *gif,
		const struct nsgif_frame *frame)
{
	if (gif->bitmap.set_opaque) {
		gif->bitmap.set_opaque(
				gif->frame_image, frame->opaque);
	}
}

/**
 * Helper to get the client to determine if the bitmap is opaque.
 *
 * \todo: We don't really need to get the client to do this for us.
 *
 * \param[in]  gif    The gif object we're decoding.
 * \return true if the bitmap is opaque, false otherwise.
 */
static inline bool nsgif__bitmap_get_opaque(
		const struct nsgif *gif)
{
	if (gif->bitmap.test_opaque) {
		return gif->bitmap.test_opaque(
				gif->frame_image);
	}

	return false;
}

static void nsgif__record_frame(
		struct nsgif *gif,
		const fl::u32 *bitmap)
{
	fl::size pixel_bytes = sizeof(*bitmap);
	fl::size height = gif->info.height;
	fl::size width  = gif->info.width;
	fl::u32 *prev_frame;

	if (gif->decoded_frame == NSGIF_FRAME_INVALID ||
	    gif->decoded_frame == gif->prev_index) {
		/* No frame to copy, or already have this frame recorded. */
		return;
	}

	bitmap = nsgif__bitmap_get(gif);
	if (bitmap == NULL) {
		return;
	}

	if (gif->prev_frame == NULL) {
		prev_frame = static_cast<fl::u32*>(fl::Malloc(width * height * pixel_bytes));
		if (prev_frame == NULL) {
			return;
		}
		gif->prev_frame = prev_frame;
	} else {
		prev_frame = static_cast<fl::u32*>(gif->prev_frame);
	}

	fl::memcopy(prev_frame, bitmap, width * height * pixel_bytes);

	gif->prev_frame  = prev_frame;
	gif->prev_index  = gif->decoded_frame;
}

static nsgif_error nsgif__recover_frame(
		const struct nsgif *gif,
		fl::u32 *bitmap)
{
	const fl::u32 *prev_frame = static_cast<const fl::u32*>(gif->prev_frame);
	fl::size pixel_bytes = sizeof(*bitmap);
	fl::size height = gif->info.height;
	fl::size width  = gif->info.width;

	fl::memcopy(bitmap, prev_frame, height * width * pixel_bytes);

	return NSGIF_OK;
}

/**
 * Get the next line for GIF decode.
 *
 * Note that the step size must be initialised to 24 at the start of the frame
 * (when y == 0).  This is because of the first two passes of the frame have
 * the same step size of 8, and the step size is used to determine the current
 * pass.
 *
 * \param[in]     height     Frame height in pixels.
 * \param[in,out] y          Current row, starting from 0, updated on exit.
 * \param[in,out] step       Current step starting with 24, updated on exit.
 * \return true if there is a row to process, false at the end of the frame.
 */
static inline bool nsgif__deinterlace(fl::u32 height, fl::u32 *y, fl::u8 *step)
{
	*y += *step & 0xf;

	if (*y < height) return true;

	switch (*step) {
	case 24: *y = 4; *step = 8; if (*y < height) return true;
	         /* Fall through. */
	case  8: *y = 2; *step = 4; if (*y < height) return true;
	         /* Fall through. */
	case  4: *y = 1; *step = 2; if (*y < height) return true;
	         /* Fall through. */
	default:
		break;
	}

	return false;
}

/**
 * Get the next line for GIF decode.
 *
 * \param[in]     interlace  Non-zero if the frame is not interlaced.
 * \param[in]     height     Frame height in pixels.
 * \param[in,out] y          Current row, starting from 0, updated on exit.
 * \param[in,out] step       Current step starting with 24, updated on exit.
 * \return true if there is a row to process, false at the end of the frame.
 */
static inline bool nsgif__next_row(fl::u32 interlace,
		fl::u32 height, fl::u32 *y, fl::u8 *step)
{
	if (!interlace) {
		return (++*y != height);
	} else {
		return nsgif__deinterlace(height, y, step);
	}
}

/**
 * Get any frame clip adjustment for the image extent.
 *
 * \param[in]  frame_off  Frame's X or Y offset.
 * \param[in]  frame_dim  Frame width or height.
 * \param[in]  image_ext  Image width or height constraint.
 * \return the amount the frame needs to be clipped to fit the image in given
 *         dimension.
 */
static inline fl::u32 gif__clip(
		fl::u32 frame_off,
		fl::u32 frame_dim,
		fl::u32 image_ext)
{
	fl::u32 frame_ext = frame_off + frame_dim;

	if (frame_ext <= image_ext) {
		return 0;
	}

	return frame_ext - image_ext;
}

/**
 * Perform any jump over decoded data, to accommodate clipped portion of frame.
 *
 * \param[in,out] skip       Number of pixels of data to jump.
 * \param[in,out] available  Number of pixels of data currently available.
 * \param[in,out] pos        Position in decoded pixel value data.
 */
static inline void gif__jump_data(
		fl::u32 *skip,
		fl::u32 *available,
		const fl::u8 **pos)
{
	fl::u32 jump = (*skip < *available) ? *skip : *available;

	*skip -= jump;
	*available -= jump;
	*pos += jump;
}

static nsgif_error nsgif__decode_complex(
		struct nsgif *gif,
		fl::u32 width,
		fl::u32 height,
		fl::u32 offset_x,
		fl::u32 offset_y,
		fl::u32 interlace,
		const fl::u8 *data,
		fl::u32 transparency_index,
		fl::u32 * frame_data,
		fl::u32 * colour_table)
{
	lzw_result res;
	nsgif_error ret = NSGIF_OK;
	fl::u32 clip_x = gif__clip(offset_x, width, gif->info.width);
	fl::u32 clip_y = gif__clip(offset_y, height, gif->info.height);
	const fl::u8 *uncompressed;
	fl::u32 available = 0;
	fl::u8 step = 24;
	fl::u32 skip = 0;
	fl::u32 y = 0;

	if (offset_x >= gif->info.width ||
	    offset_y >= gif->info.height) {
		return NSGIF_OK;
	}

	width -= clip_x;
	height -= clip_y;

	if (width == 0 || height == 0) {
		return NSGIF_OK;
	}

	/* Initialise the LZW decoding */
	res = lzw_decode_init(static_cast<struct lzw_ctx*>(gif->lzw_ctx), data[0],
			gif->buf, gif->buf_len,
			data + 1 - gif->buf);
	if (res != LZW_OK) {
		return nsgif__error_from_lzw(res);
	}

	do {
		fl::u32 x;
		fl::u32 *frame_scanline;

		frame_scanline = frame_data + offset_x +
				(y + offset_y) * gif->rowspan;

		x = width;
		while (x > 0) {
			unsigned row_available;
			while (available == 0) {
				if (res != LZW_OK) {
					/* Unexpected end of frame, try to recover */
					if (res == LZW_OK_EOD ||
					    res == LZW_EOI_CODE) {
						ret = NSGIF_OK;
					} else {
						ret = nsgif__error_from_lzw(res);
					}
					return ret;
				}
				res = lzw_decode(static_cast<struct lzw_ctx*>(gif->lzw_ctx),
						&uncompressed, &available);

				if (available == 0) {
					return NSGIF_OK;
				}
				gif__jump_data(&skip, &available, &uncompressed);
			}

			row_available = x < available ? x : available;
			x -= row_available;
			available -= row_available;
			if (transparency_index > 0xFF) {
				while (row_available-- > 0) {
					*frame_scanline++ =
						colour_table[*uncompressed++];
				}
			} else {
				while (row_available-- > 0) {
					fl::u32 colour;
					colour = *uncompressed++;
					if (colour != transparency_index) {
						*frame_scanline =
							colour_table[colour];
					}
					frame_scanline++;
				}
			}
		}

		skip = clip_x;
		gif__jump_data(&skip, &available, &uncompressed);
	} while (nsgif__next_row(interlace, height, &y, &step));

	return ret;
}

static nsgif_error nsgif__decode_simple(
		struct nsgif *gif,
		fl::u32 height,
		fl::u32 offset_y,
		const fl::u8 *data,
		fl::u32 transparency_index,
		fl::u32 * frame_data,
		fl::u32 * colour_table)
{
	fl::u32 pixels;
	fl::u32 written = 0;
	nsgif_error ret = NSGIF_OK;
	lzw_result res;

	if (offset_y >= gif->info.height) {
		return NSGIF_OK;
	}

	height -= gif__clip(offset_y, height, gif->info.height);

	if (height == 0) {
		return NSGIF_OK;
	}

	/* Initialise the LZW decoding */
	res = lzw_decode_init_map(static_cast<struct lzw_ctx*>(gif->lzw_ctx), data[0],
			transparency_index, colour_table,
			gif->buf, gif->buf_len,
			data + 1 - gif->buf);
	if (res != LZW_OK) {
		return nsgif__error_from_lzw(res);
	}

	frame_data += (offset_y * gif->info.width);
	pixels = gif->info.width * height;

	while (pixels > 0) {
		res = lzw_decode_map(static_cast<struct lzw_ctx*>(gif->lzw_ctx),
				frame_data, pixels, &written);
		pixels -= written;
		frame_data += written;
		if (res != LZW_OK) {
			/* Unexpected end of frame, try to recover */
			if (res == LZW_OK_EOD || res == LZW_EOI_CODE) {
				ret = NSGIF_OK;
			} else {
				ret = nsgif__error_from_lzw(res);
			}
			break;
		}
	}

	if (pixels == 0) {
		ret = NSGIF_OK;
	}

	return ret;
}

static inline nsgif_error nsgif__decode(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 *data,
		fl::u32 * frame_data)
{
	nsgif_error ret;
	fl::u32 width  = frame->info.rect.x1 - frame->info.rect.x0;
	fl::u32 height = frame->info.rect.y1 - frame->info.rect.y0;
	fl::u32 offset_x = frame->info.rect.x0;
	fl::u32 offset_y = frame->info.rect.y0;
	fl::u32 transparency_index = frame->transparency_index;
	fl::u32 * colour_table = gif->colour_table;

	if (frame->info.interlaced == false && offset_x == 0 &&
			width == gif->info.width &&
			width == gif->rowspan) {
		ret = nsgif__decode_simple(gif, height, offset_y,
				data, transparency_index,
				frame_data, colour_table);
	} else {
		ret = nsgif__decode_complex(gif, width, height,
				offset_x, offset_y, frame->info.interlaced,
				data, transparency_index,
				frame_data, colour_table);
	}

	if (gif->data_complete && ret == NSGIF_ERR_END_OF_DATA) {
		/* This is all the data there is, so make do. */
		ret = NSGIF_OK;
	}

	return ret;
}

/**
 * Restore a GIF to the background colour.
 *
 * \param[in] gif     The gif object we're decoding.
 * \param[in] frame   The frame to clear, or NULL.
 * \param[in] bitmap  The bitmap to clear the frame in.
 */
static void nsgif__restore_bg(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		fl::u32 *bitmap)
{
	fl::size pixel_bytes = sizeof(*bitmap);

	if (frame == NULL) {
		fl::size width  = gif->info.width;
		fl::size height = gif->info.height;

		fl::memfill(bitmap, NSGIF_TRANSPARENT_COLOUR,
				width * height * pixel_bytes);
	} else {
		fl::u32 width  = frame->info.rect.x1 - frame->info.rect.x0;
		fl::u32 height = frame->info.rect.y1 - frame->info.rect.y0;
		fl::u32 offset_x = frame->info.rect.x0;
		fl::u32 offset_y = frame->info.rect.y0;

		if (frame->info.display == false ||
		    frame->info.rect.x0 >= gif->info.width ||
		    frame->info.rect.y0 >= gif->info.height) {
			return;
		}

		width -= gif__clip(offset_x, width, gif->info.width);
		height -= gif__clip(offset_y, height, gif->info.height);

		if (frame->info.transparency) {
			for (fl::u32 y = 0; y < height; y++) {
				fl::u32 *scanline = bitmap + offset_x +
						(offset_y + y) * gif->info.width;
				fl::memfill(scanline, NSGIF_TRANSPARENT_COLOUR,
						width * pixel_bytes);
			}
		} else {
			for (fl::u32 y = 0; y < height; y++) {
				fl::u32 *scanline = bitmap + offset_x +
						(offset_y + y) * gif->info.width;
				for (fl::u32 x = 0; x < width; x++) {
					scanline[x] = gif->info.background;
				}
			}
		}
	}
}

static nsgif_error nsgif__update_bitmap(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 *data,
		fl::u32 frame_idx)
{
	nsgif_error ret;
	fl::u32 *bitmap;

	gif->decoded_frame = frame_idx;

	bitmap = nsgif__bitmap_get(gif);
	if (bitmap == NULL) {
		return NSGIF_ERR_OOM;
	}

	/* Handle any bitmap clearing/restoration required before decoding this
	 * frame. */
	if (frame_idx == 0 || gif->decoded_frame == NSGIF_FRAME_INVALID) {
		nsgif__restore_bg(gif, NULL, bitmap);

	} else {
		struct nsgif_frame *prev = &gif->frames[frame_idx - 1];

		if (prev->info.disposal == NSGIF_DISPOSAL_RESTORE_BG) {
			nsgif__restore_bg(gif, prev, bitmap);

		} else if (prev->info.disposal == NSGIF_DISPOSAL_RESTORE_PREV) {
			ret = nsgif__recover_frame(gif, bitmap);
			if (ret != NSGIF_OK) {
				nsgif__restore_bg(gif, prev, bitmap);
			}
		}
	}

	if (frame->info.disposal == NSGIF_DISPOSAL_RESTORE_PREV) {
		/* Store the previous frame for later restoration */
		nsgif__record_frame(gif, bitmap);
	}

	ret = nsgif__decode(gif, frame, data, bitmap);

	nsgif__bitmap_modified(gif);

	if (!frame->decoded) {
		frame->opaque = nsgif__bitmap_get_opaque(gif);
		frame->decoded = true;
	}
	nsgif__bitmap_set_opaque(gif, frame);

	return ret;
}

/**
 * Parse the graphic control extension
 *
 * \param[in] frame  The gif frame object we're decoding.
 * \param[in] data   The data to decode.
 * \param[in] len    Byte length of data.
 * \return NSGIF_ERR_END_OF_DATA if more data is needed,
 *         NSGIF_OK for success.
 */
static nsgif_error nsgif__parse_extension_graphic_control(
		struct nsgif_frame *frame,
		const fl::u8 *data,
		fl::size len)
{
	enum {
		GIF_MASK_TRANSPARENCY = 0x01,
		GIF_MASK_DISPOSAL     = 0x1c,
	};

	/* 6-byte Graphic Control Extension is:
	 *
	 *  +0  CHAR    Graphic Control Label
	 *  +1  CHAR    Block Size
	 *  +2  CHAR    __Packed Fields__
	 *              3BITS   Reserved
	 *              3BITS   Disposal Method
	 *              1BIT    User Input Flag
	 *              1BIT    Transparent Color Flag
	 *  +3  SHORT   Delay Time
	 *  +5  CHAR    Transparent Color Index
	 */
	if (len < 6) {
		return NSGIF_ERR_END_OF_DATA;
	}

	frame->info.delay = data[3] | (data[4] << 8);

	if (data[2] & GIF_MASK_TRANSPARENCY) {
		frame->info.transparency = true;
		frame->transparency_index = data[5];
	}

	frame->info.disposal = ((data[2] & GIF_MASK_DISPOSAL) >> 2);
	/* I have encountered documentation and GIFs in the
	 * wild that use 0x04 to restore the previous frame,
	 * rather than the officially documented 0x03.  I
	 * believe some (older?)  software may even actually
	 * export this way.  We handle this as a type of
	 * "quirks" mode. */
	if (frame->info.disposal == NSGIF_DISPOSAL_RESTORE_QUIRK) {
		frame->info.disposal = NSGIF_DISPOSAL_RESTORE_PREV;
	}

	/* if we are clearing the background then we need to
	 * redraw enough to cover the previous frame too. */
	frame->redraw_required =
			frame->info.disposal == NSGIF_DISPOSAL_RESTORE_BG ||
			frame->info.disposal == NSGIF_DISPOSAL_RESTORE_PREV;

	return NSGIF_OK;
}

/**
 * Check an app ext identifier and authentication code for loop count extension.
 *
 * \param[in] data  The data to decode.
 * \param[in] len   Byte length of data.
 * \return true if extension is a loop count extension.
 */
static bool nsgif__app_ext_is_loop_count(
		const fl::u8 *data,
		fl::size len)
{
	enum {
		EXT_LOOP_COUNT_BLOCK_SIZE = 0x0b,
	};

	FL_ASSERT(len > 13, "Extension data too short");
	(void)(len);

	if (data[1] == EXT_LOOP_COUNT_BLOCK_SIZE) {
		if (strncmp((const char *)data + 2, "NETSCAPE2.0", 11) == 0 ||
		    strncmp((const char *)data + 2, "ANIMEXTS1.0", 11) == 0) {
			return true;
		}
	}

	return false;
}

/**
 * Parse the application extension
 *
 * \param[in] gif   The gif object we're decoding.
 * \param[in] data  The data to decode.
 * \param[in] len   Byte length of data.
 * \return NSGIF_ERR_END_OF_DATA if more data is needed,
 *         NSGIF_OK for success.
 */
static nsgif_error nsgif__parse_extension_application(
		struct nsgif *gif,
		const fl::u8 *data,
		fl::size len)
{
	/* 14-byte+ Application Extension is:
	 *
	 *  +0    CHAR    Application Extension Label
	 *  +1    CHAR    Block Size
	 *  +2    8CHARS  Application Identifier
	 *  +10   3CHARS  Appl. Authentication Code
	 *  +13   1-256   Application Data (Data sub-blocks)
	 */
	if (len < 17) {
		return NSGIF_ERR_END_OF_DATA;
	}

	if (nsgif__app_ext_is_loop_count(data, len)) {
		enum {
			EXT_LOOP_COUNT_SUB_BLOCK_SIZE = 0x03,
			EXT_LOOP_COUNT_SUB_BLOCK_ID   = 0x01,
		};
		if ((data[13] == EXT_LOOP_COUNT_SUB_BLOCK_SIZE) &&
		    (data[14] == EXT_LOOP_COUNT_SUB_BLOCK_ID)) {
			gif->info.loop_max = data[15] | (data[16] << 8);

			/* The value in the source data means repeat N times
			 * after the first implied play. A value of zero has
			 * the special meaning of loop forever. (The only way
			 * to play the animation once is  not to have this
			 * extension at all. */
			if (gif->info.loop_max > 0) {
				gif->info.loop_max++;
			}
		}
	}

	return NSGIF_OK;
}

/**
 * Parse the frame's extensions
 *
 * \param[in] gif     The gif object we're decoding.
 * \param[in] frame   The frame to parse extensions for.
 * \param[in] pos     Current position in data, updated on exit.
 * \param[in] decode  Whether to decode or skip over the extension.
 * \return NSGIF_ERR_END_OF_DATA if more data is needed,
 *         NSGIF_OK for success.
 */
static nsgif_error nsgif__parse_frame_extensions(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 **pos,
		bool decode)
{
	enum {
		GIF_EXT_INTRODUCER      = 0x21,
		GIF_EXT_GRAPHIC_CONTROL = 0xf9,
		GIF_EXT_COMMENT         = 0xfe,
		GIF_EXT_PLAIN_TEXT      = 0x01,
		GIF_EXT_APPLICATION     = 0xff,
	};
	const fl::u8 *nsgif_data = *pos;
	const fl::u8 *nsgif_end = gif->buf + gif->buf_len;
	int nsgif_bytes = nsgif_end - nsgif_data;

	/* Initialise the extensions */
	while (nsgif_bytes > 0 && nsgif_data[0] == GIF_EXT_INTRODUCER) {
		bool block_step = true;
		nsgif_error ret;

		nsgif_data++;
		nsgif_bytes--;

		if (nsgif_bytes == 0) {
			return NSGIF_ERR_END_OF_DATA;
		}

		/* Switch on extension label */
		switch (nsgif_data[0]) {
		case GIF_EXT_GRAPHIC_CONTROL:
			if (decode) {
				ret = nsgif__parse_extension_graphic_control(
						frame,
						nsgif_data,
						nsgif_bytes);
				if (ret != NSGIF_OK) {
					return ret;
				}
			}
			break;

		case GIF_EXT_APPLICATION:
			if (decode) {
				ret = nsgif__parse_extension_application(
						gif, nsgif_data, nsgif_bytes);
				if (ret != NSGIF_OK) {
					return ret;
				}
			}
			break;

		case GIF_EXT_COMMENT:
			/* Move the pointer to the first data sub-block Skip 1
			 * byte for the extension label. */
			++nsgif_data;
			block_step = false;
			break;

		default:
			break;
		}

		if (block_step) {
			/* Move the pointer to the first data sub-block Skip 2
			 * bytes for the extension label and size fields Skip
			 * the extension size itself
			 */
			if (nsgif_bytes < 2) {
				return NSGIF_ERR_END_OF_DATA;
			}
			nsgif_data += 2 + nsgif_data[1];
		}

		/* Repeatedly skip blocks until we get a zero block or run out
		 * of data.  This data is ignored by this gif decoder. */
		while (nsgif_data < nsgif_end && nsgif_data[0] != NSGIF_BLOCK_TERMINATOR) {
			nsgif_data += nsgif_data[0] + 1;
			if (nsgif_data >= nsgif_end) {
				return NSGIF_ERR_END_OF_DATA;
			}
		}
		nsgif_data++;
		nsgif_bytes = nsgif_end - nsgif_data;
	}

	if (nsgif_data > nsgif_end) {
		nsgif_data = nsgif_end;
	}

	/* Set buffer position and return */
	*pos = nsgif_data;
	return NSGIF_OK;
}

/**
 * Parse a GIF Image Descriptor.
 *
 * The format is:
 *
 *  +0   CHAR   Image Separator (0x2c)
 *  +1   SHORT  Image Left Position
 *  +3   SHORT  Image Top Position
 *  +5   SHORT  Width
 *  +7   SHORT  Height
 *  +9   CHAR   __Packed Fields__
 *              1BIT    Local Colour Table Flag
 *              1BIT    Interlace Flag
 *              1BIT    Sort Flag
 *              2BITS   Reserved
 *              3BITS   Size of Local Colour Table
 *
 * \param[in] gif     The gif object we're decoding.
 * \param[in] frame   The frame to parse an image descriptor for.
 * \param[in] pos     Current position in data, updated on exit.
 * \param[in] decode  Whether to decode the image descriptor.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static nsgif_error nsgif__parse_image_descriptor(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 **pos,
		bool decode)
{
	const fl::u8 *data = *pos;
	fl::size len = gif->buf + gif->buf_len - data;
	enum {
		NSGIF_IMAGE_DESCRIPTOR_LEN = 10u,
		NSGIF_IMAGE_SEPARATOR      = 0x2Cu,
		NSGIF_MASK_INTERLACE       = 0x40u,
	};

	FL_ASSERT(gif != NULL, "GIF object required");
	FL_ASSERT(frame != NULL, "Frame object required");

	if (len < NSGIF_IMAGE_DESCRIPTOR_LEN) {
		return NSGIF_ERR_END_OF_DATA;
	}

	if (decode) {
		fl::u32 x, y, w, h;

		if (data[0] != NSGIF_IMAGE_SEPARATOR) {
			return NSGIF_ERR_DATA_FRAME;
		}

		x = data[1] | (data[2] << 8);
		y = data[3] | (data[4] << 8);
		w = data[5] | (data[6] << 8);
		h = data[7] | (data[8] << 8);
		frame->flags = data[9];

		frame->info.rect.x0 = x;
		frame->info.rect.y0 = y;
		frame->info.rect.x1 = x + w;
		frame->info.rect.y1 = y + h;

		frame->info.interlaced = frame->flags & NSGIF_MASK_INTERLACE;

		/* Allow first frame to grow image dimensions. */
		if (gif->info.frame_count == 0) {
			if (x + w > gif->info.width) {
				gif->info.width = x + w;
			}
			if (y + h > gif->info.height) {
				gif->info.height = y + h;
			}
		}
	}

	*pos += NSGIF_IMAGE_DESCRIPTOR_LEN;
	return NSGIF_OK;
}

/**
 * Extract a GIF colour table into a LibNSGIF colour table buffer.
 *
 * \param[in] colour_table          The colour table to populate.
 * \param[in] layout                la.
 * \param[in] colour_table_entries  The number of colour table entries.
 * \param[in] data                  Raw colour table data.
 */
static void nsgif__colour_table_decode(
		fl::u32 colour_table[NSGIF_MAX_COLOURS],
		const struct nsgif_colour_layout *layout,
		fl::size colour_table_entries,
		const fl::u8 *data)
{
	fl::u8 *entry = (fl::u8 *)colour_table;

	while (colour_table_entries--) {
		/* Gif colour map contents are r,g,b.
		 *
		 * We want to pack them bytewise into the colour table,
		 * according to the client colour layout.
		 */

		entry[layout->r] = *data++;
		entry[layout->g] = *data++;
		entry[layout->b] = *data++;
		entry[layout->a] = 0xff;

		entry += sizeof(fl::u32);
	}
}

/**
 * Extract a GIF colour table into a LibNSGIF colour table buffer.
 *
 * \param[in]  colour_table          The colour table to populate.
 * \param[in]  layout                The target pixel format to decode to.
 * \param[in]  colour_table_entries  The number of colour table entries.
 * \param[in]  data                  Current position in data.
 * \param[in]  data_len              The available length of `data`.
 * \param[out] used                  Number of colour table bytes read.
 * \param[in]  decode                Whether to decode the colour table.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static inline nsgif_error nsgif__colour_table_extract(
		fl::u32 colour_table[NSGIF_MAX_COLOURS],
		const struct nsgif_colour_layout *layout,
		fl::size colour_table_entries,
		const fl::u8 *data,
		fl::size data_len,
		fl::size *used,
		bool decode)
{
	if (data_len < colour_table_entries * 3) {
		return NSGIF_ERR_END_OF_DATA;
	}

	if (decode) {
		nsgif__colour_table_decode(colour_table, layout,
				colour_table_entries, data);
	}

	*used = colour_table_entries * 3;
	return NSGIF_OK;
}

/**
 * Get a frame's colour table.
 *
 * Sets up gif->colour_table for the frame.
 *
 * \param[in] gif     The gif object we're decoding.
 * \param[in] frame   The frame to get the colour table for.
 * \param[in] pos     Current position in data, updated on exit.
 * \param[in] decode  Whether to decode the colour table.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static nsgif_error nsgif__parse_colour_table(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 **pos,
		bool decode)
{
	nsgif_error ret;
	const fl::u8 *data = *pos;
	fl::size len = gif->buf + gif->buf_len - data;
	fl::size used_bytes;

	FL_ASSERT(gif != NULL, "GIF object required");
	FL_ASSERT(frame != NULL, "Frame object required");

	if ((frame->flags & NSGIF_COLOUR_TABLE_MASK) == 0) {
		gif->colour_table = gif->global_colour_table;
		return NSGIF_OK;
	}

	if (decode == false) {
		frame->colour_table_offset = *pos - gif->buf;
	}

	ret = nsgif__colour_table_extract(
			gif->local_colour_table, &gif->colour_layout,
			2 << (frame->flags & NSGIF_COLOUR_TABLE_SIZE_MASK),
			data, len, &used_bytes, decode);
	if (ret != NSGIF_OK) {
		return ret;
	}
	*pos += used_bytes;

	if (decode) {
		gif->colour_table = gif->local_colour_table;
	} else {
		frame->info.local_palette = true;
	}

	return NSGIF_OK;
}

/**
 * Parse the image data for a gif frame.
 *
 * Sets up gif->colour_table for the frame.
 *
 * \param[in] gif     The gif object we're decoding.
 * \param[in] frame   The frame to parse image data for.
 * \param[in] pos     Current position in data, updated on exit.
 * \param[in] decode  Whether to decode the image data.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static nsgif_error nsgif__parse_image_data(
		struct nsgif *gif,
		struct nsgif_frame *frame,
		const fl::u8 **pos,
		bool decode)
{
	const fl::u8 *data = *pos;
	fl::size len = gif->buf + gif->buf_len - data;
	fl::u32 frame_idx = frame - gif->frames;
	fl::u8 minimum_code_size;
	nsgif_error ret;

	FL_ASSERT(gif != NULL, "GIF object required");
	FL_ASSERT(frame != NULL, "Frame object required");

	if (!decode) {
		gif->frame_count_partial = frame_idx + 1;
	}

	/* Ensure sufficient data remains.  A gif trailer or a minimum lzw code
	 * followed by a gif trailer is treated as OK, although without any
	 * image data. */
	switch (len) {
		default: if (data[0] == NSGIF_TRAILER) return NSGIF_OK;
			break;
		case 2: if (data[1] == NSGIF_TRAILER) return NSGIF_OK;
			/* Fall through. */
		case 1: if (data[0] == NSGIF_TRAILER) return NSGIF_OK;
			/* Fall through. */
		case 0: return NSGIF_ERR_END_OF_DATA;
	}

	minimum_code_size = data[0];
	if (minimum_code_size >= LZW_CODE_MAX) {
		return NSGIF_ERR_DATA_FRAME;
	}

	if (decode) {
		ret = nsgif__update_bitmap(gif, frame, data, frame_idx);
	} else {
		fl::u32 block_size = 0;

		/* Skip the minimum code size. */
		data++;
		len--;

		while (block_size != 1) {
			if (len < 1) {
				return NSGIF_ERR_END_OF_DATA;
			}
			block_size = data[0] + 1;
			/* Check if the frame data runs off the end of the file */
			if (block_size > len) {
				frame->lzw_data_length += len;
				return NSGIF_ERR_END_OF_DATA;
			}

			len -= block_size;
			data += block_size;
			frame->lzw_data_length += block_size;
		}

		*pos = data;

		gif->info.frame_count = frame_idx + 1;
		gif->frames[frame_idx].info.display = true;

		return NSGIF_OK;
	}

	return ret;
}

static struct nsgif_frame *nsgif__get_frame(
		struct nsgif *gif,
		fl::u32 frame_idx)
{
	struct nsgif_frame *frame;

	if (gif->frame_holders > frame_idx) {
		frame = &gif->frames[frame_idx];
	} else {
		/* Allocate more memory */
		fl::size count = frame_idx + 1;
		struct nsgif_frame *temp;

		temp = static_cast<struct nsgif_frame*>(fl::Malloc(count * sizeof(*frame)));
		if (temp == NULL) {
			return NULL;
		}
		if (gif->frames) {
			fl::memcopy(temp, gif->frames, gif->frame_holders * sizeof(*frame));
			fl::Free(gif->frames);
		}
		gif->frames = temp;
		gif->frame_holders = count;

		frame = &gif->frames[frame_idx];

		frame->info.local_palette = false;
		frame->info.transparency = false;
		frame->info.display = false;
		frame->info.disposal = 0;
		frame->info.delay = 10;

		frame->transparency_index = NSGIF_NO_TRANSPARENCY;
		frame->frame_offset = gif->buf_pos;
		frame->redraw_required = false;
		frame->lzw_data_length = 0;
		frame->decoded = false;
	}

	return frame;
}

/**
 * Attempts to initialise the next frame
 *
 * \param[in] gif       The animation context
 * \param[in] frame_idx The frame number to decode.
 * \param[in] decode    Whether to decode the graphical image data.
 * \return NSGIF_OK on success, appropriate error otherwise.
*/
static nsgif_error nsgif__process_frame(
		struct nsgif *gif,
		fl::u32 frame_idx,
		bool decode)
{
	nsgif_error ret;
	const fl::u8 *pos;
	const fl::u8 *end;
	struct nsgif_frame *frame;

	frame = nsgif__get_frame(gif, frame_idx);
	if (frame == NULL) {
		return NSGIF_ERR_OOM;
	}

	end = gif->buf + gif->buf_len;

	if (decode) {
		pos = gif->buf + frame->frame_offset;

		/* Ensure this frame is supposed to be decoded */
		if (frame->info.display == false) {
			return NSGIF_OK;
		}

		/* Ensure the frame is in range to decode */
		if (frame_idx > gif->frame_count_partial) {
			return NSGIF_ERR_END_OF_DATA;
		}

		/* Done if frame is already decoded */
		if (frame_idx == gif->decoded_frame) {
			return NSGIF_OK;
		}
	} else {
		pos = gif->buf + gif->buf_pos;

		/* Check if we've finished */
		if (pos < end && pos[0] == NSGIF_TRAILER) {
			return NSGIF_OK;
		}
	}

	ret = nsgif__parse_frame_extensions(gif, frame, &pos, !decode);
	if (ret != NSGIF_OK) {
		goto cleanup;
	}

	ret = nsgif__parse_image_descriptor(gif, frame, &pos, !decode);
	if (ret != NSGIF_OK) {
		goto cleanup;
	}

	ret = nsgif__parse_colour_table(gif, frame, &pos, decode);
	if (ret != NSGIF_OK) {
		goto cleanup;
	}

	ret = nsgif__parse_image_data(gif, frame, &pos, decode);
	if (ret != NSGIF_OK) {
		goto cleanup;
	}

cleanup:
	if (!decode) {
		gif->buf_pos = pos - gif->buf;
	}

	return ret;
}

/* exported function documented in nsgif.h */
void nsgif_destroy(nsgif_t *gif)
{
	if (gif == NULL) {
		return;
	}

	/* Release all our memory blocks */
	if (gif->frame_image) {
		FL_ASSERT(gif->bitmap.destroy, "GIF bitmap destroy function required");
		gif->bitmap.destroy(gif->frame_image);
		gif->frame_image = NULL;
	}

	fl::Free(gif->frames);
	gif->frames = NULL;

	fl::Free(gif->prev_frame);
	gif->prev_frame = NULL;

	lzw_context_destroy(static_cast<struct lzw_ctx*>(gif->lzw_ctx));
	gif->lzw_ctx = NULL;

	fl::Free(gif);
}

/**
 * Check whether the host is little endian.
 *
 * Checks whether least significant bit is in the first byte of a `fl::u16`.
 *
 * \return true if host is little endian.
 */
static inline bool nsgif__host_is_little_endian(void)
{
	const fl::u16 test = 1;

	return ((const fl::u8 *) &test)[0];
}

static struct nsgif_colour_layout nsgif__bitmap_fmt_to_colour_layout(
		nsgif_bitmap_fmt_t bitmap_fmt)
{
	bool le = nsgif__host_is_little_endian();

	/* Map endian-dependant formats to byte-wise format for the host. */
	switch (bitmap_fmt) {
	case NSGIF_BITMAP_FMT_RGBA8888:
		bitmap_fmt = (le) ? NSGIF_BITMAP_FMT_A8B8G8R8
		                  : NSGIF_BITMAP_FMT_R8G8B8A8;
		break;
	case NSGIF_BITMAP_FMT_BGRA8888:
		bitmap_fmt = (le) ? NSGIF_BITMAP_FMT_A8R8G8B8
		                  : NSGIF_BITMAP_FMT_B8G8R8A8;
		break;
	case NSGIF_BITMAP_FMT_ARGB8888:
		bitmap_fmt = (le) ? NSGIF_BITMAP_FMT_B8G8R8A8
		                  : NSGIF_BITMAP_FMT_A8R8G8B8;
		break;
	case NSGIF_BITMAP_FMT_ABGR8888:
		bitmap_fmt = (le) ? NSGIF_BITMAP_FMT_R8G8B8A8
		                  : NSGIF_BITMAP_FMT_A8B8G8R8;
		break;
	case NSGIF_BITMAP_FMT_R8G8B8A8:
	case NSGIF_BITMAP_FMT_B8G8R8A8:
	case NSGIF_BITMAP_FMT_A8R8G8B8:
	case NSGIF_BITMAP_FMT_A8B8G8R8:
		/* These are already byte-wise formats, no conversion needed */
		break;
	default:
		break;
	}

	/* Set up colour component order for bitmap format. */
	switch (bitmap_fmt) {
	default:
		/* Fall through. */
	case NSGIF_BITMAP_FMT_R8G8B8A8:
			{ struct nsgif_colour_layout result = {0, 1, 2, 3}; return result; }

	case NSGIF_BITMAP_FMT_B8G8R8A8:
			{ struct nsgif_colour_layout result = {2, 1, 0, 3}; return result; }

	case NSGIF_BITMAP_FMT_A8R8G8B8:
			{ struct nsgif_colour_layout result = {1, 2, 3, 0}; return result; }

	case NSGIF_BITMAP_FMT_A8B8G8R8:
			{ struct nsgif_colour_layout result = {3, 2, 1, 0}; return result; }

	case NSGIF_BITMAP_FMT_RGBA8888:
	case NSGIF_BITMAP_FMT_BGRA8888:
	case NSGIF_BITMAP_FMT_ARGB8888:
	case NSGIF_BITMAP_FMT_ABGR8888:
		/* These should have been converted to byte-wise formats above */
		{ struct nsgif_colour_layout result = {0, 1, 2, 3}; return result; }
	}
}

/* exported function documented in nsgif.h */
nsgif_error nsgif_create(
		const nsgif_bitmap_cb_vt *bitmap_vt,
		nsgif_bitmap_fmt_t bitmap_fmt,
		nsgif_t **gif_out)
{
	nsgif_t *gif;

	gif = static_cast<nsgif_t*>(fl::Malloc(sizeof(*gif)));
	if (gif) {
		fl::memfill(gif, 0, sizeof(*gif));
	}
	if (gif == NULL) {
		return NSGIF_ERR_OOM;
	}

	gif->bitmap = *bitmap_vt;
	gif->decoded_frame = NSGIF_FRAME_INVALID;
	gif->prev_index = NSGIF_FRAME_INVALID;

	gif->delay_min = NSGIF_FRAME_DELAY_MIN;
	gif->delay_default = NSGIF_FRAME_DELAY_DEFAULT;

	gif->colour_layout = nsgif__bitmap_fmt_to_colour_layout(bitmap_fmt);

	*gif_out = gif;
	return NSGIF_OK;
}

/* exported function documented in nsgif.h */
void nsgif_set_frame_delay_behaviour(
		nsgif_t *gif,
		fl::u16 delay_min,
		fl::u16 delay_default)
{
	gif->delay_min = delay_min;
	gif->delay_default = delay_default;
}

/**
 * Read GIF header.
 *
 * 6-byte GIF file header is:
 *
 *  +0   3CHARS   Signature ('GIF')
 *  +3   3CHARS   Version ('87a' or '89a')
 *
 * \param[in]      gif     The GIF object we're decoding.
 * \param[in,out]  pos     The current buffer position, updated on success.
 * \param[in]      strict  Whether to require a known GIF version.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static nsgif_error nsgif__parse_header(
		struct nsgif *gif,
		const fl::u8 **pos,
		bool strict)
{
	const fl::u8 *data = *pos;
	fl::size len = gif->buf + gif->buf_len - data;

	if (len < 6) {
		return NSGIF_ERR_END_OF_DATA;
	}

	if (strncmp((const char *) data, "GIF", 3) != 0) {
		return NSGIF_ERR_DATA;
	}
	data += 3;

	if (strict == true) {
		if ((strncmp((const char *) data, "87a", 3) != 0) &&
		    (strncmp((const char *) data, "89a", 3) != 0)) {
			return NSGIF_ERR_DATA;
		}
	}
	data += 3;

	*pos = data;
	return NSGIF_OK;
}

/**
 * Read Logical Screen Descriptor.
 *
 * 7-byte Logical Screen Descriptor is:
 *
 *  +0   SHORT   Logical Screen Width
 *  +2   SHORT   Logical Screen Height
 *  +4   CHAR    __Packed Fields__
 *               1BIT    Global Colour Table Flag
 *               3BITS   Colour Resolution
 *               1BIT    Sort Flag
 *               3BITS   Size of Global Colour Table
 *  +5   CHAR    Background Colour Index
 *  +6   CHAR    Pixel Aspect Ratio
 *
 * \param[in]      gif     The GIF object we're decoding.
 * \param[in,out]  pos     The current buffer position, updated on success.
 * \return NSGIF_OK on success, appropriate error otherwise.
 */
static nsgif_error nsgif__parse_logical_screen_descriptor(
		struct nsgif *gif,
		const fl::u8 **pos)
{
	const fl::u8 *data = *pos;
	fl::size len = gif->buf + gif->buf_len - data;

	if (len < 7) {
		return NSGIF_ERR_END_OF_DATA;
	}

	gif->info.width = data[0] | (data[1] << 8);
	gif->info.height = data[2] | (data[3] << 8);
	gif->info.global_palette = data[4] & NSGIF_COLOUR_TABLE_MASK;
	gif->colour_table_size = 2 << (data[4] & NSGIF_COLOUR_TABLE_SIZE_MASK);
	gif->bg_index = data[5];
	gif->aspect_ratio = data[6];
	gif->info.loop_max = 1;

	*pos += 7;
	return NSGIF_OK;
}

/* exported function documented in nsgif.h */
nsgif_error nsgif_data_scan(
		nsgif_t *gif,
		fl::size size,
		const fl::u8 *data)
{
	const fl::u8 *nsgif_data;
	nsgif_error ret;
	fl::u32 frames;

	if (gif->data_complete) {
		return NSGIF_ERR_DATA_COMPLETE;
	}

	/* Initialize values */
	gif->buf_len = size;
	gif->buf = data;

	/* Get our current processing position */
	nsgif_data = gif->buf + gif->buf_pos;

	/* See if we should initialise the GIF */
	if (gif->buf_pos == 0) {
		/* We want everything to be NULL before we start so we've no
		 * chance of freeing bad pointers (paranoia)
		 */
		gif->frame_image = NULL;
		gif->frames = NULL;
		gif->frame_holders = 0;

		/* The caller may have been lazy and not reset any values */
		gif->info.frame_count = 0;
		gif->frame_count_partial = 0;
		gif->decoded_frame = NSGIF_FRAME_INVALID;
		gif->frame = NSGIF_FRAME_INVALID;

		ret = nsgif__parse_header(gif, &nsgif_data, false);
		if (ret != NSGIF_OK) {
			return ret;
		}

		ret = nsgif__parse_logical_screen_descriptor(gif, &nsgif_data);
		if (ret != NSGIF_OK) {
			return ret;
		}

		/* Remember we've done this now */
		gif->buf_pos = nsgif_data - gif->buf;

		/* Some broken GIFs report the size as the screen size they
		 * were created in. As such, we detect for the common cases and
		 * set the sizes as 0 if they are found which results in the
		 * GIF being the maximum size of the frames.
		 */
		if (((gif->info.width == 640) && (gif->info.height == 480)) ||
		    ((gif->info.width == 640) && (gif->info.height == 512)) ||
		    ((gif->info.width == 800) && (gif->info.height == 600)) ||
		    ((gif->info.width == 1024) && (gif->info.height == 768)) ||
		    ((gif->info.width == 1280) && (gif->info.height == 1024)) ||
		    ((gif->info.width == 1600) && (gif->info.height == 1200)) ||
		    ((gif->info.width == 0) || (gif->info.height == 0)) ||
		    ((gif->info.width > 2048) || (gif->info.height > 2048))) {
			gif->info.width = 1;
			gif->info.height = 1;
		}

		/* Set the first colour to a value that will never occur in
		 * reality so we know if we've processed it
		*/
		gif->global_colour_table[0] = NSGIF_PROCESS_COLOURS;

		/* Check if the GIF has no frame data (13-byte header + 1-byte
		 * termination block) Although generally useless, the GIF
		 * specification does not expressly prohibit this
		 */
		if (gif->buf_len == gif->buf_pos + 1) {
			if (nsgif_data[0] == NSGIF_TRAILER) {
				return NSGIF_OK;
			}
		}
	}

	/*  Do the colour map if we haven't already. As the top byte is always
	 *  0xff or 0x00 depending on the transparency we know if it's been
	 *  filled in.
	 */
	if (gif->global_colour_table[0] == NSGIF_PROCESS_COLOURS) {
		/* Check for a global colour map signified by bit 7 */
		if (gif->info.global_palette) {
			fl::size remaining = gif->buf + gif->buf_len - nsgif_data;
			fl::size used;

			ret = nsgif__colour_table_extract(
					gif->global_colour_table,
					&gif->colour_layout,
					gif->colour_table_size,
					nsgif_data, remaining, &used, true);
			if (ret != NSGIF_OK) {
				return ret;
			}

			nsgif_data += used;
			gif->buf_pos = (nsgif_data - gif->buf);
		} else {
			/* Create a default colour table with the first two
			 * colours as black and white. */
			fl::u8 *entry = (fl::u8 *)gif->global_colour_table;

			/* Black */
			entry[gif->colour_layout.r] = 0x00;
			entry[gif->colour_layout.g] = 0x00;
			entry[gif->colour_layout.b] = 0x00;
			entry[gif->colour_layout.a] = 0xFF;

			entry += sizeof(fl::u32);

			/* White */
			entry[gif->colour_layout.r] = 0xFF;
			entry[gif->colour_layout.g] = 0xFF;
			entry[gif->colour_layout.b] = 0xFF;
			entry[gif->colour_layout.a] = 0xFF;

			gif->colour_table_size = 2;
		}

		if (gif->info.global_palette &&
		    gif->bg_index < gif->colour_table_size) {
			fl::size bg_idx = gif->bg_index;
			gif->info.background = gif->global_colour_table[bg_idx];
		} else {
			gif->info.background = gif->global_colour_table[0];
		}
	}

	if (gif->lzw_ctx == NULL) {
		struct lzw_ctx *lzw_ctx_ptr;
		lzw_result res = lzw_context_create(&lzw_ctx_ptr);
		gif->lzw_ctx = lzw_ctx_ptr;
		if (res != LZW_OK) {
			return nsgif__error_from_lzw(res);
		}
	}

	/* Try to initialise all frames. */
	do {
		frames = gif->info.frame_count;
		ret = nsgif__process_frame(gif, frames, false);
	} while (gif->info.frame_count > frames);

	if (ret == NSGIF_ERR_END_OF_DATA && gif->info.frame_count > 0) {
		ret = NSGIF_OK;
	}

	return ret;
}

/* exported function documented in nsgif.h */
void nsgif_data_complete(
		nsgif_t *gif)
{
	if (gif->data_complete == false) {
		fl::u32 start = gif->info.frame_count;
		fl::u32 end = gif->frame_count_partial;

		for (fl::u32 f = start; f < end; f++) {
			nsgif_frame *frame = &gif->frames[f];

			if (frame->lzw_data_length > 0) {
				frame->info.display = true;
				gif->info.frame_count = f + 1;

				if (f == 0) {
					frame->info.transparency = true;
				}
				break;
			}
		}
	}

	gif->data_complete = true;
}

static void nsgif__redraw_rect_extend(
		const nsgif_rect_t *frame,
		nsgif_rect_t *redraw)
{
	if (redraw->x1 == 0 || redraw->y1 == 0) {
		*redraw = *frame;
	} else {
		if (redraw->x0 > frame->x0) {
			redraw->x0 = frame->x0;
		}
		if (redraw->x1 < frame->x1) {
			redraw->x1 = frame->x1;
		}
		if (redraw->y0 > frame->y0) {
			redraw->y0 = frame->y0;
		}
		if (redraw->y1 < frame->y1) {
			redraw->y1 = frame->y1;
		}
	}
}

static fl::u32 nsgif__frame_next(
		const nsgif_t *gif,
		bool partial,
		fl::u32 frame)
{
	fl::u32 frames = partial ?
			gif->frame_count_partial :
			gif->info.frame_count;

	if (frames == 0) {
		return NSGIF_FRAME_INVALID;
	}

	frame++;
	return (frame >= frames) ? 0 : frame;
}

static nsgif_error nsgif__next_displayable_frame(
		const nsgif_t *gif,
		fl::u32 *frame,
		fl::u32 *delay)
{
	fl::u32 next = *frame;

	do {
		next = nsgif__frame_next(gif, false, next);
		if (next <= *frame && *frame != NSGIF_FRAME_INVALID &&
				gif->data_complete == false) {
			return NSGIF_ERR_END_OF_DATA;

		} else if (next == *frame || next == NSGIF_FRAME_INVALID) {
			return NSGIF_ERR_FRAME_DISPLAY;
		}

		if (delay != NULL) {
			*delay += gif->frames[next].info.delay;
		}

	} while (gif->frames[next].info.display == false);

	*frame = next;
	return NSGIF_OK;
}

static inline bool nsgif__animation_complete(int count, int max)
{
	if (max == 0) {
		return false;
	}

	return (count >= max);
}

nsgif_error nsgif_reset(
		nsgif_t *gif)
{
	gif->loop_count = 0;
	gif->frame = NSGIF_FRAME_INVALID;

	return NSGIF_OK;
}

/* exported function documented in nsgif.h */
nsgif_error nsgif_frame_prepare(
		nsgif_t *gif,
		nsgif_rect_t *area,
		fl::u32 *delay_cs,
		fl::u32 *frame_new)
{
	nsgif_error ret;
	nsgif_rect_t rect = {0, 0, 0, 0};
	fl::u32 delay = 0;
	fl::u32 frame = gif->frame;

	if (gif->frame != NSGIF_FRAME_INVALID &&
	    gif->frame < gif->info.frame_count &&
	    gif->frames[gif->frame].info.display) {
		rect = gif->frames[gif->frame].info.rect;
	}

	if (nsgif__animation_complete(
			gif->loop_count,
			gif->info.loop_max)) {
		return NSGIF_ERR_ANIMATION_END;
	}

	ret = nsgif__next_displayable_frame(gif, &frame, &delay);
	if (ret != NSGIF_OK) {
		return ret;
	}

	if (gif->frame != NSGIF_FRAME_INVALID && frame < gif->frame) {
		gif->loop_count++;
	}

	if (gif->data_complete) {
		/* Check for last frame, which has infinite delay. */

		if (gif->info.frame_count == 1) {
			delay = NSGIF_INFINITE;
		} else if (gif->info.loop_max != 0) {
			fl::u32 frame_next = frame;

			ret = nsgif__next_displayable_frame(gif,
					&frame_next, NULL);
			if (ret != NSGIF_OK) {
				return ret;
			}

			if (gif->data_complete && frame_next < frame) {
				if (nsgif__animation_complete(
						gif->loop_count + 1,
						gif->info.loop_max)) {
					delay = NSGIF_INFINITE;
				}
			}
		}
	}

	gif->frame = frame;
	nsgif__redraw_rect_extend(&gif->frames[frame].info.rect, &rect);

	if (delay < gif->delay_min) {
		delay = gif->delay_default;
	}

	*frame_new = gif->frame;
	*delay_cs = delay;
	*area = rect;

	return NSGIF_OK;
}

/* exported function documented in nsgif.h */
nsgif_error nsgif_frame_decode(
		nsgif_t *gif,
		fl::u32 frame,
		nsgif_bitmap_t **bitmap)
{
	fl::u32 start_frame;
	nsgif_error ret = NSGIF_OK;

	if (frame >= gif->info.frame_count) {
		return NSGIF_ERR_BAD_FRAME;
	}

	if (gif->decoded_frame == frame) {
		*bitmap = gif->frame_image;
		return NSGIF_OK;

	} else if (gif->decoded_frame >= frame ||
	           gif->decoded_frame == NSGIF_FRAME_INVALID) {
		/* Can skip to first frame or restart. */
		start_frame = 0;
	} else {
		start_frame = nsgif__frame_next(
				gif, false, gif->decoded_frame);
	}

	for (fl::u32 f = start_frame; f <= frame; f++) {
		ret = nsgif__process_frame(gif, f, true);
		if (ret != NSGIF_OK) {
			return ret;
		}
	}

	*bitmap = gif->frame_image;
	return ret;
}

/* exported function documented in nsgif.h */
const nsgif_info_t *nsgif_get_info(const nsgif_t *gif)
{
	return &gif->info;
}

/* exported function documented in nsgif.h */
const nsgif_frame_info_t *nsgif_get_frame_info(
		const nsgif_t *gif,
		fl::u32 frame)
{
	if (frame >= gif->info.frame_count) {
		return NULL;
	}

	return &gif->frames[frame].info;
}

/* exported function documented in nsgif.h */
void nsgif_global_palette(
		const nsgif_t *gif,
		fl::u32 table[NSGIF_MAX_COLOURS],
		fl::size *entries)
{
	fl::size len = sizeof(*table) * NSGIF_MAX_COLOURS;

	fl::memcopy(table, gif->global_colour_table, len);
	*entries = gif->colour_table_size;
}

/* exported function documented in nsgif.h */
bool nsgif_local_palette(
		const nsgif_t *gif,
		fl::u32 frame,
		fl::u32 table[NSGIF_MAX_COLOURS],
		fl::size *entries)
{
	const nsgif_frame *f;

	if (frame >= gif->frame_count_partial) {
		return false;
	}

	f = &gif->frames[frame];
	if (f->info.local_palette == false) {
		return false;
	}

	*entries = 2 << (f->flags & NSGIF_COLOUR_TABLE_SIZE_MASK);
	nsgif__colour_table_decode(table, &gif->colour_layout,
			*entries, gif->buf + f->colour_table_offset);

	return true;
}

/* exported function documented in nsgif.h */
const char *nsgif_strerror(nsgif_error err)
{
	static const char *const str[] = {
		"Success",                             /* NSGIF_OK */
		"Out of memory",                       /* NSGIF_ERR_OOM */
		"Invalid source data",                 /* NSGIF_ERR_DATA */
		"Requested frame does not exist",      /* NSGIF_ERR_BAD_FRAME */
		"Invalid frame data",                  /* NSGIF_ERR_DATA_FRAME */
		"Unexpected end of GIF source data",   /* NSGIF_ERR_END_OF_DATA */
		"Can't add data to completed GIF",     /* NSGIF_ERR_DATA_COMPLETE */
		"Frame can't be displayed",            /* NSGIF_ERR_FRAME_DISPLAY */
		"Animation complete",                  /* NSGIF_ERR_ANIMATION_END */
	};

	if (err >= NSGIF_ARRAY_LEN(str) || str[err] == NULL) {
		return "Unknown error";
	}

	return str[err];
}

/* exported function documented in nsgif.h */
const char *nsgif_str_disposal(enum nsgif_disposal disposal)
{
	static const char *const str[] = {
		"Unspecified",        /* NSGIF_DISPOSAL_UNSPECIFIED */
		"None",               /* NSGIF_DISPOSAL_NONE */
		"Restore background", /* NSGIF_DISPOSAL_RESTORE_BG */
		"Restore previous",   /* NSGIF_DISPOSAL_RESTORE_PREV */
		"Restore quirk",      /* NSGIF_DISPOSAL_RESTORE_QUIRK */
	};

	if (disposal >= NSGIF_ARRAY_LEN(str) || str[disposal] == NULL) {
		return "Unspecified";
	}

	return str[disposal];
}

} // namespace third_party
} // namespace fl
