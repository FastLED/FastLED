#include "fl/remote/rpc/base64.h"
#include "fl/stl/cstring.h"

namespace fl {

namespace {

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_char_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace

fl::string base64_encode(fl::span<const fl::u8> data) {
    fl::string out;
    if (data.empty()) return out;

    fl::size encoded_len = 4 * ((data.size() + 2) / 3);
    out.reserve(encoded_len);

    fl::size i = 0;
    fl::size len = data.size();

    while (i + 2 < len) {
        fl::u32 triple = (fl::u32(data[i]) << 16) |
                         (fl::u32(data[i + 1]) << 8) |
                          fl::u32(data[i + 2]);
        out += kBase64Chars[(triple >> 18) & 0x3F];
        out += kBase64Chars[(triple >> 12) & 0x3F];
        out += kBase64Chars[(triple >> 6) & 0x3F];
        out += kBase64Chars[triple & 0x3F];
        i += 3;
    }

    // Handle remaining bytes
    if (i < len) {
        fl::u32 triple = fl::u32(data[i]) << 16;
        if (i + 1 < len) {
            triple |= fl::u32(data[i + 1]) << 8;
        }
        out += kBase64Chars[(triple >> 18) & 0x3F];
        out += kBase64Chars[(triple >> 12) & 0x3F];
        if (i + 1 < len) {
            out += kBase64Chars[(triple >> 6) & 0x3F];
        } else {
            out += '=';
        }
        out += '=';
    }

    return out;
}

fl::vector<fl::u8> base64_decode(const fl::string& encoded) {
    fl::vector<fl::u8> out;
    if (encoded.empty()) return out;

    fl::size len = encoded.size();

    // Validate length is multiple of 4
    if (len % 4 != 0) return out;

    fl::size out_len = (len / 4) * 3;
    if (len >= 1 && encoded[len - 1] == '=') out_len--;
    if (len >= 2 && encoded[len - 2] == '=') out_len--;
    out.reserve(out_len);

    for (fl::size i = 0; i < len; i += 4) {
        int a = base64_char_index(encoded[i]);
        int b = base64_char_index(encoded[i + 1]);
        int c = (encoded[i + 2] == '=') ? 0 : base64_char_index(encoded[i + 2]);
        int d = (encoded[i + 3] == '=') ? 0 : base64_char_index(encoded[i + 3]);

        if (a < 0 || b < 0 || c < 0 || d < 0) {
            return fl::vector<fl::u8>(); // invalid character
        }

        fl::u32 triple = (fl::u32(a) << 18) | (fl::u32(b) << 12) |
                          (fl::u32(c) << 6) | fl::u32(d);

        out.push_back((triple >> 16) & 0xFF);
        if (encoded[i + 2] != '=') {
            out.push_back((triple >> 8) & 0xFF);
        }
        if (encoded[i + 3] != '=') {
            out.push_back(triple & 0xFF);
        }
    }

    return out;
}

} // namespace fl
