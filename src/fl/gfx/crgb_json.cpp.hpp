#include "fl/gfx/crgb_json.h"

namespace fl {

namespace {

optional<i64> crgbJsonChannel(const json& value) FL_NO_EXCEPT {
    if (!value.is_int() || value.is_bool()) {
        return fl::nullopt;
    }
    const optional<i64> channel = value.as<i64>();
    if (!channel || *channel < 0 || *channel > 255) {
        return fl::nullopt;
    }
    return channel;
}

optional<CRGB> crgbJsonPixel(const json& value) FL_NO_EXCEPT {
    if (!value.is_array() || value.size() != 3) {
        return fl::nullopt;
    }

    const optional<i64> red = crgbJsonChannel(value[0]);
    const optional<i64> green = crgbJsonChannel(value[1]);
    const optional<i64> blue = crgbJsonChannel(value[2]);
    if (!red || !green || !blue) {
        return fl::nullopt;
    }

    return CRGB(static_cast<u8>(*red), static_cast<u8>(*green),
                static_cast<u8>(*blue));
}

bool crgbJsonMatches(const json& document,
                     span<const CRGB> pixels) FL_NO_EXCEPT {
    if (!document.is_object() || document.size() != 2) {
        return false;
    }

    const json versionJson = document["version"];
    const optional<i64> version = versionJson.as<i64>();
    if (!versionJson.is_int() || versionJson.is_bool() || !version ||
        *version != 1) {
        return false;
    }

    const json encodedPixels = document["pixels"];
    if (!encodedPixels.is_array() || encodedPixels.size() != pixels.size()) {
        return false;
    }

    for (size i = 0; i < encodedPixels.size(); ++i) {
        const optional<CRGB> decoded = crgbJsonPixel(encodedPixels[i]);
        if (!decoded || *decoded != pixels[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

optional<json> crgbToJson(span<const CRGB> pixels) FL_NO_EXCEPT {
    json encodedPixels = json::array();
    if (!encodedPixels.is_array()) {
        return fl::nullopt;
    }

    for (const CRGB& pixel : pixels) {
        json encodedPixel = json::array();
        if (!encodedPixel.is_array()) {
            return fl::nullopt;
        }

        encodedPixel.push_back(json(static_cast<int>(pixel.r)));
        encodedPixel.push_back(json(static_cast<int>(pixel.g)));
        encodedPixel.push_back(json(static_cast<int>(pixel.b)));
        if (encodedPixel.size() != 3 || !crgbJsonPixel(encodedPixel)) {
            return fl::nullopt;
        }

        const size oldSize = encodedPixels.size();
        encodedPixels.push_back(encodedPixel);
        if (encodedPixels.size() != oldSize + 1) {
            return fl::nullopt;
        }
    }

    json document = json::object();
    if (!document.is_object()) {
        return fl::nullopt;
    }
    document.set("version", 1);
    document.set("pixels", encodedPixels);
    if (!crgbJsonMatches(document, pixels)) {
        return fl::nullopt;
    }
    return document;
}

bool crgbFromJson(const json& document, span<CRGB> pixels) FL_NO_EXCEPT {
    if (!document.is_object()) {
        return false;
    }

    const json versionJson = document["version"];
    const optional<i64> version = versionJson.as<i64>();
    if (!versionJson.is_int() || versionJson.is_bool() || !version ||
        *version != 1) {
        return false;
    }

    const json encodedPixels = document["pixels"];
    if (!encodedPixels.is_array() || encodedPixels.size() != pixels.size()) {
        return false;
    }

    for (size i = 0; i < encodedPixels.size(); ++i) {
        if (!crgbJsonPixel(encodedPixels[i])) {
            return false;
        }
    }

    for (size i = 0; i < encodedPixels.size(); ++i) {
        pixels[i] = *crgbJsonPixel(encodedPixels[i]);
    }
    return true;
}

} // namespace fl
