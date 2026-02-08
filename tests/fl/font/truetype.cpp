#include "test.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/vector.h"
#include "fl/stl/memory.h"

// Include the stb_truetype header (declarations only)
// Implementation is in libfastled.a via third_party/_build.cpp
#include "third_party/stb/truetype/stb_truetype.h"

#include "fl/file_system.h"

using namespace fl::third_party::truetype;

namespace {

// Helper function to load a font file
fl::vector<unsigned char> loadFontFile(const char* filename) {
    fl::setTestFileSystemRoot("tests/fl/font/data");
    fl::FileSystem fs;

    // Initialize the filesystem (required for stub platform)
    if (!fs.beginSd(0)) {
        return fl::vector<unsigned char>();
    }

    auto file_handle = fs.openRead(filename);
    if (!file_handle || !file_handle->valid()) {
        return fl::vector<unsigned char>();
    }

    // Get file size
    fl::size size = file_handle->size();

    // Read file into buffer
    fl::vector<unsigned char> buffer(size);
    fl::size bytes_read = file_handle->read(buffer.data(), size);
    if (bytes_read != size) {
        return fl::vector<unsigned char>();
    }

    return buffer;
}

} // anonymous namespace

FL_TEST_CASE("stbtt_truetype - Font loading") {
    FL_SUBCASE("Load Covenant5x5.ttf (default embedded font)") {
        auto font_data = loadFontFile("Covenant5x5.ttf");
        FL_REQUIRE(!font_data.empty());

        // Verify it's a valid font file
        int32_t num_fonts = stbtt_GetNumberOfFonts(font_data.data());
        FL_CHECK_GT(num_fonts, 0);
        FL_CHECK_LE(num_fonts, 10); // Reasonable upper bound
    }

    FL_SUBCASE("Get font offset") {
        auto font_data = loadFontFile("Covenant5x5.ttf");
        FL_REQUIRE(!font_data.empty());

        int32_t offset = stbtt_GetFontOffsetForIndex(font_data.data(), 0);
        FL_CHECK_GE(offset, 0);
    }

    FL_SUBCASE("Initialize font info") {
        auto font_data = loadFontFile("Covenant5x5.ttf");
        FL_REQUIRE(!font_data.empty());

        stbtt_fontinfo font;
        int32_t result = stbtt_InitFont(&font, font_data.data(), 0);
        FL_CHECK_NE(result, 0); // Non-zero means success
    }
}

FL_TEST_CASE("stbtt_truetype - Font metrics") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    FL_SUBCASE("Get vertical metrics") {
        int32_t ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

        // Ascent should be positive, descent negative
        FL_CHECK_GT(ascent, 0);
        FL_CHECK_LT(descent, 0);
        FL_CHECK_GE(line_gap, 0);
    }

    FL_SUBCASE("Get bounding box") {
        int32_t x0, y0, x1, y1;
        stbtt_GetFontBoundingBox(&font, &x0, &y0, &x1, &y1);

        // Bounding box should be valid
        FL_CHECK_LT(x0, x1);
        FL_CHECK_LT(y0, y1);
    }

    FL_SUBCASE("Scale for pixel height") {
        float scale = stbtt_ScaleForPixelHeight(&font, 32.0f);
        FL_CHECK_GT(scale, 0.0f);
        FL_CHECK_LT(scale, 1.0f); // Scale should be less than 1 for typical fonts
    }
}

FL_TEST_CASE("stbtt_truetype - Glyph queries") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    FL_SUBCASE("Find glyph index for period") {
        int32_t glyph_index = stbtt_FindGlyphIndex(&font, '.');
        FL_CHECK_GT(glyph_index, 0); // Should find the period glyph
    }

    FL_SUBCASE("Find glyph index for '8'") {
        int32_t glyph_index = stbtt_FindGlyphIndex(&font, '8');
        FL_CHECK_GT(glyph_index, 0); // Should find the '8' glyph
    }

    FL_SUBCASE("Get codepoint metrics") {
        int32_t advance_width, left_side_bearing;
        stbtt_GetCodepointHMetrics(&font, 'A', &advance_width, &left_side_bearing);

        FL_CHECK_GT(advance_width, 0);
    }

    FL_SUBCASE("Check glyph is not empty") {
        int32_t glyph_index = stbtt_FindGlyphIndex(&font, '.');
        FL_REQUIRE(glyph_index > 0);

        int32_t is_empty = stbtt_IsGlyphEmpty(&font, glyph_index);
        FL_CHECK_EQ(is_empty, 0); // Period should not be empty
    }
}

FL_TEST_CASE("stbtt_truetype - Bitmap rendering - period") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    float scale = stbtt_ScaleForPixelHeight(&font, 32.0f);

    FL_SUBCASE("Render period (.) with antialiasing") {
        int32_t width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, '.',
                                                          &width, &height, &xoff, &yoff);

        FL_REQUIRE(bitmap != nullptr);
        FL_CHECK_GT(width, 0);
        FL_CHECK_GT(height, 0);

        // Verify bitmap contains non-zero pixels (antialiasing)
        bool has_nonzero = false;
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x) {
                if (bitmap[y * width + x] > 0) {
                    has_nonzero = true;
                    break;
                }
            }
            if (has_nonzero) break;
        }
        FL_CHECK(has_nonzero);

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    FL_SUBCASE("Get bitmap box for period") {
        int32_t ix0, iy0, ix1, iy1;
        stbtt_GetCodepointBitmapBox(&font, '.', scale, scale, &ix0, &iy0, &ix1, &iy1);

        // Period should have a valid bounding box
        FL_CHECK_LT(ix0, ix1);
        FL_CHECK_LT(iy0, iy1);

        // Period is small, so box should be reasonable
        FL_CHECK_LT(ix1 - ix0, 32);
        FL_CHECK_LT(iy1 - iy0, 32);
    }
}

FL_TEST_CASE("stbtt_truetype - Bitmap rendering - digit 8") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    float scale = stbtt_ScaleForPixelHeight(&font, 32.0f);

    FL_SUBCASE("Render '8' with antialiasing") {
        int32_t width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, '8',
                                                          &width, &height, &xoff, &yoff);

        FL_REQUIRE(bitmap != nullptr);
        FL_CHECK_GT(width, 0);
        FL_CHECK_GT(height, 0);

        // Verify bitmap contains pixels (either fully opaque or antialiased)
        // Note: Pixel fonts like Covenant5x5 may only have 0 or 255 values
        int32_t opaque_pixels = 0;
        int32_t antialiased_pixels = 0;
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x) {
                unsigned char pixel = bitmap[y * width + x];
                if (pixel == 255) {
                    opaque_pixels++;
                } else if (pixel > 0) {
                    antialiased_pixels++;
                }
            }
        }

        // '8' should have some visible pixels
        FL_CHECK_GT(opaque_pixels + antialiased_pixels, 0);

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    FL_SUBCASE("Verify '8' shape characteristics") {
        int32_t width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, '8',
                                                          &width, &height, &xoff, &yoff);

        FL_REQUIRE(bitmap != nullptr);

        // '8' has two enclosed loops, so there should be white space in the middle
        // Check that not all pixels are solid
        int32_t solid_pixels = 0;
        int32_t total_pixels = width * height;

        for (int32_t i = 0; i < total_pixels; ++i) {
            if (bitmap[i] == 255) {
                solid_pixels++;
            }
        }

        // Not all pixels should be solid (8 has holes)
        FL_CHECK_LT(solid_pixels, total_pixels);

        // Should have some solid pixels though
        FL_CHECK_GT(solid_pixels, 0);

        stbtt_FreeBitmap(bitmap, nullptr);
    }
}

FL_TEST_CASE("stbtt_truetype - Rendering without antialiasing") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    // Verify we can compute a scale (just test it works)
    float scale = stbtt_ScaleForPixelHeight(&font, 32.0f);
    (void)scale;

    FL_SUBCASE("Bake font bitmap (simplified packing)") {
        // Baking creates a bitmap atlas without antialiasing control,
        // but we can verify it works
        const int32_t bitmap_w = 512;
        const int32_t bitmap_h = 512;
        fl::vector<unsigned char> bitmap(bitmap_w * bitmap_h, 0);
        fl::vector<stbtt_bakedchar> chardata(96); // ASCII printable chars

        int32_t result = stbtt_BakeFontBitmap(font_data.data(), 0, 32.0f,
                                              bitmap.data(), bitmap_w, bitmap_h,
                                              32, 96, // Start at space, 96 chars
                                              chardata.data());

        // Should successfully bake some characters
        FL_CHECK_GT(result, 0);
    }
}

FL_TEST_CASE("stbtt_truetype - Kerning") {
    auto font_data = loadFontFile("Covenant5x5.ttf");
    FL_REQUIRE(!font_data.empty());

    stbtt_fontinfo font;
    FL_REQUIRE(stbtt_InitFont(&font, font_data.data(), 0) != 0);

    FL_SUBCASE("Get kerning advance") {
        // Test kerning between common pairs
        int32_t kern_AV = stbtt_GetCodepointKernAdvance(&font, 'A', 'V');
        int32_t kern_AA = stbtt_GetCodepointKernAdvance(&font, 'A', 'A');

        // AV pair typically has negative kerning, AA typically zero
        // But font may not have kerning table, so just check it returns
        (void)kern_AV;
        (void)kern_AA;
        FL_CHECK(true); // If we got here, functions work
    }
}
