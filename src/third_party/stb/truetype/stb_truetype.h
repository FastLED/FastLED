#pragma once

// stb_truetype.h - Public API for TrueType font processing
// Part of the FastLED library - adapted from stb_truetype v1.26
// Original: public domain by Sean Barrett / RAD Game Tools
//
// =======================================================================
//
//    NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES
//
// This library does no range checking of the offsets found in the file,
// meaning an attacker can use it to read arbitrary memory.
//
// =======================================================================

#include "fl/stl/stdint.h"

namespace fl {
namespace third_party {
namespace truetype {

//////////////////////////////////////////////////////////////////////////////
//
// INTERNAL TYPES (exposed for structure layout)
//

// Internal buffer type (private - do not use directly)
struct stbtt__buf {
    unsigned char *data;
    int32_t cursor;
    int32_t size;
};

// Rectangle packer coordinate type
using stbrp_coord = int32_t;

// Rectangle for packing
struct stbrp_rect {
    stbrp_coord x, y;
    int32_t id, w, h, was_packed;
};

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC TYPES
//

// Baked character data for simple bitmap baking API
struct stbtt_bakedchar {
    unsigned short x0, y0, x1, y1;  // coordinates of bbox in bitmap
    float xoff, yoff, xadvance;
};

// Aligned quad for rendering
struct stbtt_aligned_quad {
    float x0, y0, s0, t0;  // top-left
    float x1, y1, s1, t1;  // bottom-right
};

// Packed character data for advanced packing API
struct stbtt_packedchar {
    unsigned short x0, y0, x1, y1;  // coordinates of bbox in bitmap
    float xoff, yoff, xadvance;
    float xoff2, yoff2;
};

// Pack range definition
struct stbtt_pack_range {
    float font_size;
    int32_t first_unicode_codepoint_in_range;  // if non-zero, then the chars are continuous
    int32_t *array_of_unicode_codepoints;       // if non-zero, then this is an array
    int32_t num_chars;
    stbtt_packedchar *chardata_for_range;
    unsigned char h_oversample, v_oversample;
};

// Kerning entry
struct stbtt_kerningentry {
    int32_t glyph1;  // use stbtt_FindGlyphIndex
    int32_t glyph2;
    int32_t advance;
};

// Vertex type for glyph shapes
#ifndef stbtt_vertex_type
#define stbtt_vertex_type short
#endif

struct stbtt_vertex {
    stbtt_vertex_type x, y, cx, cy, cx1, cy1;
    unsigned char type, padding;
};

// Vertex types
enum {
    STBTT_vmove = 1,
    STBTT_vline,
    STBTT_vcurve,
    STBTT_vcubic
};

//////////////////////////////////////////////////////////////////////////////
//
// COMPLETE STRUCT DEFINITIONS
//

// Packing context - treat as opaque, but defined for stack allocation
struct stbtt_pack_context {
    void *user_allocator_context;
    void *pack_info;
    int32_t width;
    int32_t height;
    int32_t stride_in_bytes;
    int32_t padding;
    int32_t skip_missing;
    uint32_t h_oversample, v_oversample;
    unsigned char *pixels;
    void *nodes;
};

// Font info - treat as opaque, but defined for stack allocation
struct stbtt_fontinfo {
    void *userdata;
    unsigned char *data;              // pointer to .ttf file
    int32_t fontstart;                // offset of start of font

    int32_t numGlyphs;                // number of glyphs, needed for range checking

    int32_t loca, head, glyf, hhea, hmtx, kern, gpos, svg;  // table locations
    int32_t index_map;                // a cmap mapping for our chosen character encoding
    int32_t indexToLocFormat;         // format needed to map from glyph index to glyph

    stbtt__buf cff;                   // cff font data
    stbtt__buf charstrings;           // the charstring index
    stbtt__buf gsubrs;                // global charstring subroutines index
    stbtt__buf subrs;                 // private charstring subroutines index
    stbtt__buf fontdicts;             // array of font dicts
    stbtt__buf fdselect;              // map from glyph to fontdict
};

//////////////////////////////////////////////////////////////////////////////
//
// TEXTURE BAKING API
//
// Simple API - just call these two functions
//

// Bake a font bitmap with simple packing
int32_t stbtt_BakeFontBitmap(const unsigned char *data, int32_t offset,
                             float pixel_height,
                             unsigned char *pixels, int32_t pw, int32_t ph,
                             int32_t first_char, int32_t num_chars,
                             stbtt_bakedchar *chardata);

// Get quad coordinates for a baked character
void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int32_t pw, int32_t ph,
                        int32_t char_index, float *xpos, float *ypos,
                        stbtt_aligned_quad *q, int32_t opengl_fillrule);

// Get scaled font vertical metrics (helper function)
void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int32_t index,
                                 float size, float *ascent, float *descent, float *lineGap);

//////////////////////////////////////////////////////////////////////////////
//
// ADVANCED PACKING API
//
// More control over packing behavior
//

// Begin packing characters into a bitmap
int32_t stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels,
                        int32_t width, int32_t height, int32_t stride_in_bytes,
                        int32_t padding, void *alloc_context);

// End packing - cleanup
void stbtt_PackEnd(stbtt_pack_context *spc);

// Pack a range of characters
int32_t stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata,
                            int32_t font_index, float font_size,
                            int32_t first_unicode_codepoint_in_range, int32_t num_chars_in_range,
                            stbtt_packedchar *chardata_for_range);

// Pack multiple ranges
int32_t stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata,
                             int32_t font_index, stbtt_pack_range *ranges, int32_t num_ranges);

// Set oversampling for higher quality
void stbtt_PackSetOversampling(stbtt_pack_context *spc, uint32_t h_oversample, uint32_t v_oversample);

// Skip missing codepoints
void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int32_t skip);

// Get quad for a packed character
void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int32_t pw, int32_t ph,
                         int32_t char_index, float *xpos, float *ypos,
                         stbtt_aligned_quad *q, int32_t align_to_integer);

// Advanced: separate gather/pack/render phases
int32_t stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info,
                                        stbtt_pack_range *ranges, int32_t num_ranges, stbrp_rect *rects);
void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int32_t num_rects);
int32_t stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info,
                                            stbtt_pack_range *ranges, int32_t num_ranges, stbrp_rect *rects);

//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//

// Get number of fonts in a TrueType collection
int32_t stbtt_GetNumberOfFonts(const unsigned char *data);

// Get offset for a specific font in a collection
int32_t stbtt_GetFontOffsetForIndex(const unsigned char *data, int32_t index);

// Initialize a font
int32_t stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int32_t offset);

//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH INDEX CONVERSION
//

// Find glyph index for a unicode codepoint
int32_t stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int32_t unicode_codepoint);

//////////////////////////////////////////////////////////////////////////////
//
// FONT METRICS
//

// Scale for pixel height
float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);

// Scale for em to pixels mapping
float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

// Get vertical metrics
void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int32_t *ascent, int32_t *descent, int32_t *lineGap);

// Get OS/2 vertical metrics
int32_t stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int32_t *typoAscent,
                                 int32_t *typoDescent, int32_t *typoLineGap);

// Get bounding box
void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);

// Get horizontal metrics for a codepoint
void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int32_t codepoint,
                                int32_t *advanceWidth, int32_t *leftSideBearing);

// Get kerning advance
int32_t stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int32_t ch1, int32_t ch2);

// Get bounding box for a codepoint
int32_t stbtt_GetCodepointBox(const stbtt_fontinfo *info, int32_t codepoint,
                              int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);

// Get horizontal metrics for a glyph
void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int32_t glyph_index,
                            int32_t *advanceWidth, int32_t *leftSideBearing);

// Get glyph kerning advance
int32_t stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int32_t glyph1, int32_t glyph2);

// Get glyph bounding box
int32_t stbtt_GetGlyphBox(const stbtt_fontinfo *info, int32_t glyph_index,
                          int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);

// Get kerning table
int32_t stbtt_GetKerningTableLength(const stbtt_fontinfo *info);
int32_t stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry *table, int32_t table_length);

//////////////////////////////////////////////////////////////////////////////
//
// GLYPH SHAPES
//

// Check if glyph is empty
int32_t stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int32_t glyph_index);

// Get shape for a codepoint
int32_t stbtt_GetCodepointShape(const stbtt_fontinfo *info, int32_t unicode_codepoint,
                                stbtt_vertex **vertices);

// Get shape for a glyph
int32_t stbtt_GetGlyphShape(const stbtt_fontinfo *info, int32_t glyph_index,
                            stbtt_vertex **vertices);

// Free shape data
void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);

// SVG support
unsigned char *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int32_t gl);
int32_t stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int32_t unicode_codepoint, const char **svg);
int32_t stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int32_t gl, const char **svg);

//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

// Free a bitmap
void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata);

// Get bitmap for a codepoint
unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y,
                                        int32_t codepoint, int32_t *width, int32_t *height,
                                        int32_t *xoff, int32_t *yoff);

// Get bitmap with subpixel positioning
unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y,
                                                float shift_x, float shift_y, int32_t codepoint,
                                                int32_t *width, int32_t *height, int32_t *xoff, int32_t *yoff);

// Render codepoint into existing bitmap
void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output,
                               int32_t out_w, int32_t out_h, int32_t out_stride,
                               float scale_x, float scale_y, int32_t codepoint);

// Render with subpixel positioning
void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
                                       int32_t out_w, int32_t out_h, int32_t out_stride,
                                       float scale_x, float scale_y, float shift_x, float shift_y,
                                       int32_t codepoint);

// Render with subpixel prefiltering
void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output,
                                                int32_t out_w, int32_t out_h, int32_t out_stride,
                                                float scale_x, float scale_y, float shift_x, float shift_y,
                                                int32_t oversample_x, int32_t oversample_y,
                                                float *sub_x, float *sub_y, int32_t codepoint);

// Get bitmap box for a codepoint
void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int32_t codepoint,
                                 float scale_x, float scale_y,
                                 int32_t *ix0, int32_t *iy0, int32_t *ix1, int32_t *iy1);

// Get bitmap box with subpixel positioning
void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int32_t codepoint,
                                         float scale_x, float scale_y, float shift_x, float shift_y,
                                         int32_t *ix0, int32_t *iy0, int32_t *ix1, int32_t *iy1);

// Glyph-based bitmap functions (same as codepoint but use glyph index)
unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y,
                                    int32_t glyph, int32_t *width, int32_t *height,
                                    int32_t *xoff, int32_t *yoff);

unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y,
                                            float shift_x, float shift_y, int32_t glyph,
                                            int32_t *width, int32_t *height, int32_t *xoff, int32_t *yoff);

void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output,
                           int32_t out_w, int32_t out_h, int32_t out_stride,
                           float scale_x, float scale_y, int32_t glyph);

void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
                                   int32_t out_w, int32_t out_h, int32_t out_stride,
                                   float scale_x, float scale_y, float shift_x, float shift_y,
                                   int32_t glyph);

void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output,
                                            int32_t out_w, int32_t out_h, int32_t out_stride,
                                            float scale_x, float scale_y, float shift_x, float shift_y,
                                            int32_t oversample_x, int32_t oversample_y,
                                            float *sub_x, float *sub_y, int32_t glyph);

void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int32_t glyph,
                             float scale_x, float scale_y,
                             int32_t *ix0, int32_t *iy0, int32_t *ix1, int32_t *iy1);

void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int32_t glyph,
                                     float scale_x, float scale_y, float shift_x, float shift_y,
                                     int32_t *ix0, int32_t *iy0, int32_t *ix1, int32_t *iy1);

//////////////////////////////////////////////////////////////////////////////
//
// SIGNED DISTANCE FIELD (SDF) RENDERING
//

// Free SDF bitmap
void stbtt_FreeSDF(unsigned char *bitmap, void *userdata);

// Get SDF for a glyph
unsigned char *stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int32_t glyph,
                                 int32_t padding, unsigned char onedge_value, float pixel_dist_scale,
                                 int32_t *width, int32_t *height, int32_t *xoff, int32_t *yoff);

// Get SDF for a codepoint
unsigned char *stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int32_t codepoint,
                                     int32_t padding, unsigned char onedge_value, float pixel_dist_scale,
                                     int32_t *width, int32_t *height, int32_t *xoff, int32_t *yoff);

//////////////////////////////////////////////////////////////////////////////
//
// FONT NAME QUERIES
//

// Find matching font by name
int32_t stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int32_t flags);

// Compare UTF8 to UTF16 big-endian
int32_t stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int32_t len1, const char *s2, int32_t len2);

// Get font name string
const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int32_t *length,
                                    int32_t platformID, int32_t encodingID,
                                    int32_t languageID, int32_t nameID);

}  // namespace truetype
}  // namespace third_party
}  // namespace fl
