#pragma once

// TrueType Font Implementation for FastLED
// This file contains the implementation of fl::Font and fl::FontRenderer
// Include this file ONCE in exactly one .cpp file in your project

#include "fl/font/truetype.h"
#include "fl/font/ttf_covenant5x5.h"  // Embedded default font declarations
#include "third_party/stb/truetype/stb_truetype.h"  // declarations only

namespace fl {

// FontImpl - concrete implementation of Font using stb_truetype
class FontImpl : public Font {
public:
    FontImpl(fl::span<const u8> fontData, i32 fontIndex = 0)
        : mFontData(fontData.begin(), fontData.end()) {
        mValid = third_party::truetype::stbtt_InitFont(
            &mFontInfo,
            mFontData.data(),
            third_party::truetype::stbtt_GetFontOffsetForIndex(mFontData.data(), fontIndex)
        ) != 0;
    }

    ~FontImpl() override = default;

    bool isValid() const { return mValid; }

    i32 getNumFonts() const override {
        return third_party::truetype::stbtt_GetNumberOfFonts(mFontData.data());
    }

    FontMetrics getMetrics() const override {
        FontMetrics metrics = {};
        if (!mValid) return metrics;

        third_party::truetype::stbtt_GetFontVMetrics(
            &mFontInfo,
            &metrics.ascent,
            &metrics.descent,
            &metrics.lineGap
        );
        third_party::truetype::stbtt_GetFontBoundingBox(
            &mFontInfo,
            &metrics.x0, &metrics.y0,
            &metrics.x1, &metrics.y1
        );
        return metrics;
    }

    float getScaleForPixelHeight(float pixelHeight) const override {
        if (!mValid) return 0.0f;
        return third_party::truetype::stbtt_ScaleForPixelHeight(&mFontInfo, pixelHeight);
    }

    GlyphMetrics getGlyphMetrics(i32 codepoint) const override {
        GlyphMetrics metrics = {};
        if (!mValid) return metrics;

        third_party::truetype::stbtt_GetCodepointHMetrics(
            &mFontInfo, codepoint,
            &metrics.advanceWidth,
            &metrics.leftSideBearing
        );

        i32 result = third_party::truetype::stbtt_GetCodepointBox(
            &mFontInfo, codepoint,
            &metrics.x0, &metrics.y0,
            &metrics.x1, &metrics.y1
        );
        metrics.isEmpty = (result == 0);

        return metrics;
    }

    i32 getKerning(i32 codepoint1, i32 codepoint2) const override {
        if (!mValid) return 0;
        return third_party::truetype::stbtt_GetCodepointKernAdvance(&mFontInfo, codepoint1, codepoint2);
    }

    GlyphBitmap renderGlyph(i32 codepoint, float scale) const override {
        return renderGlyph(codepoint, scale, 1, 1);
    }

    GlyphBitmap renderGlyph(i32 codepoint, float scale,
                            i32 oversampleX, i32 oversampleY) const override {
        GlyphBitmap result;
        if (!mValid) return result;

        if (oversampleX <= 1 && oversampleY <= 1) {
            // Simple rendering without oversampling
            unsigned char* bitmap = third_party::truetype::stbtt_GetCodepointBitmap(
                &mFontInfo, scale, scale, codepoint,
                &result.width, &result.height,
                &result.xOffset, &result.yOffset
            );

            if (bitmap && result.width > 0 && result.height > 0) {
                size_t size = static_cast<size_t>(result.width) * static_cast<size_t>(result.height);
                result.data.resize(size);
                for (size_t i = 0; i < size; ++i) {
                    result.data[i] = bitmap[i];
                }
                third_party::truetype::stbtt_FreeBitmap(bitmap, nullptr);
            }
        } else {
            // Rendering with oversampling for smoother antialiasing
            unsigned char* bitmap = third_party::truetype::stbtt_GetCodepointBitmapSubpixel(
                &mFontInfo,
                scale * static_cast<float>(oversampleX),
                scale * static_cast<float>(oversampleY),
                0.0f, 0.0f,
                codepoint,
                &result.width, &result.height,
                &result.xOffset, &result.yOffset
            );

            if (bitmap && result.width > 0 && result.height > 0) {
                // Downsample to final size
                i32 finalWidth = (result.width + oversampleX - 1) / oversampleX;
                i32 finalHeight = (result.height + oversampleY - 1) / oversampleY;

                result.data.resize(
                    static_cast<size_t>(finalWidth) * static_cast<size_t>(finalHeight)
                );

                for (i32 y = 0; y < finalHeight; ++y) {
                    for (i32 x = 0; x < finalWidth; ++x) {
                        i32 sum = 0;
                        i32 count = 0;

                        for (i32 oy = 0; oy < oversampleY; ++oy) {
                            i32 srcY = y * oversampleY + oy;
                            if (srcY >= result.height) break;

                            for (i32 ox = 0; ox < oversampleX; ++ox) {
                                i32 srcX = x * oversampleX + ox;
                                if (srcX >= result.width) break;

                                sum += bitmap[srcY * result.width + srcX];
                                ++count;
                            }
                        }

                        result.data[y * finalWidth + x] = static_cast<u8>(
                            count > 0 ? sum / count : 0
                        );
                    }
                }

                result.width = finalWidth;
                result.height = finalHeight;
                result.xOffset = result.xOffset / oversampleX;
                result.yOffset = result.yOffset / oversampleY;

                third_party::truetype::stbtt_FreeBitmap(bitmap, nullptr);
            }
        }

        return result;
    }

    const third_party::truetype::stbtt_fontinfo& getFontInfo() const { return mFontInfo; }

private:
    fl::vector<u8> mFontData;
    third_party::truetype::stbtt_fontinfo mFontInfo;
    bool mValid;
};

// Font static factory methods
fl::shared_ptr<Font> Font::loadDefault() {
    return load(ttf::covenant5x5(), 0);
}

fl::shared_ptr<Font> Font::load(fl::span<const u8> fontData) {
    return load(fontData, 0);
}

fl::shared_ptr<Font> Font::load(fl::span<const u8> fontData, i32 fontIndex) {
    auto impl = fl::make_shared<FontImpl>(fontData, fontIndex);
    if (!impl->isValid()) {
        return nullptr;
    }
    return impl;
}

// FontRenderer implementation
FontRenderer::FontRenderer(FontPtr font, float pixelHeight)
    : mFont(font)
    , mPixelHeight(pixelHeight)
    , mScale(font ? font->getScaleForPixelHeight(pixelHeight) : 0.0f) {
}

FontRenderer::~FontRenderer() = default;

FontRenderer::ScaledMetrics FontRenderer::getScaledMetrics() const {
    ScaledMetrics result = {};
    if (!mFont) return result;

    FontMetrics metrics = mFont->getMetrics();
    result.ascent = static_cast<float>(metrics.ascent) * mScale;
    result.descent = static_cast<float>(metrics.descent) * mScale;
    result.lineGap = static_cast<float>(metrics.lineGap) * mScale;
    return result;
}

GlyphBitmap FontRenderer::render(i32 codepoint) const {
    // Use 2x2 oversampling by default for LED displays
    return render(codepoint, 2, 2);
}

GlyphBitmap FontRenderer::render(i32 codepoint, i32 oversampleX, i32 oversampleY) const {
    if (!mFont) return GlyphBitmap();
    return mFont->renderGlyph(codepoint, mScale, oversampleX, oversampleY);
}

GlyphBitmap FontRenderer::renderNoAA(i32 codepoint) const {
    return render(codepoint, 1, 1);
}

float FontRenderer::getAdvance(i32 codepoint) const {
    if (!mFont) return 0.0f;
    GlyphMetrics metrics = mFont->getGlyphMetrics(codepoint);
    return static_cast<float>(metrics.advanceWidth) * mScale;
}

float FontRenderer::getKerning(i32 codepoint1, i32 codepoint2) const {
    if (!mFont) return 0.0f;
    return static_cast<float>(mFont->getKerning(codepoint1, codepoint2)) * mScale;
}

float FontRenderer::measureString(const char* str) const {
    if (!str || !mFont) return 0.0f;
    return measureString(fl::span<const char>(str, strlen(str)));
}

float FontRenderer::measureString(fl::span<const char> str) const {
    if (str.empty() || !mFont) return 0.0f;

    float width = 0.0f;
    i32 prevCodepoint = 0;

    for (size_t i = 0; i < str.size(); ++i) {
        i32 codepoint = static_cast<i32>(static_cast<unsigned char>(str[i]));

        // Add kerning adjustment if this isn't the first character
        if (prevCodepoint != 0) {
            width += getKerning(prevCodepoint, codepoint);
        }

        // Add character advance
        width += getAdvance(codepoint);
        prevCodepoint = codepoint;
    }

    return width;
}

} // namespace fl
