#pragma once

// IWYU pragma: private

#include "fl/stl/json_stream_writer.h"

#include "fl/stl/charconv.h" // fl::itoa64
#include "fl/stl/move.h"     // fl::move

namespace fl {

JsonStreamWriter::JsonStreamWriter(Sink sink) FL_NO_EXCEPT
    : mSink(fl::move(sink)), mLen(0), mDepth(0), mPendingKey(false) {}

JsonStreamWriter::~JsonStreamWriter() FL_NO_EXCEPT { flush(); }

void JsonStreamWriter::flush() FL_NO_EXCEPT {
    if (mLen > 0) {
        if (mSink) {
            mSink(mBuf, mLen);
        }
        mLen = 0;
    }
}

void JsonStreamWriter::putc(char c) FL_NO_EXCEPT {
    if (mLen >= kBufSize) {
        flush();
    }
    mBuf[mLen++] = c;
}

void JsonStreamWriter::puts(const char *s) FL_NO_EXCEPT {
    if (!s) {
        return;
    }
    for (; *s; ++s) {
        putc(*s);
    }
}

void JsonStreamWriter::putEscaped(const char *s) FL_NO_EXCEPT {
    putc('"');
    if (s) {
        for (; *s; ++s) {
            const unsigned char c = static_cast<unsigned char>(*s);
            switch (c) {
                case '"':  puts("\\\""); break;
                case '\\': puts("\\\\"); break;
                case '\n': puts("\\n"); break;
                case '\r': puts("\\r"); break;
                case '\t': puts("\\t"); break;
                case '\b': puts("\\b"); break;
                case '\f': puts("\\f"); break;
                default:
                    if (c < 0x20) {
                        // Control character → \u00XX
                        static const char *kHex = "0123456789abcdef";
                        puts("\\u00");
                        putc(kHex[(c >> 4) & 0xF]);
                        putc(kHex[c & 0xF]);
                    } else {
                        putc(static_cast<char>(c));
                    }
                    break;
            }
        }
    }
    putc('"');
}

void JsonStreamWriter::prefixForValue() FL_NO_EXCEPT {
    if (mPendingKey) {
        // We just wrote a key + ':'; the value follows directly.
        mPendingKey = false;
        return;
    }
    // Element in an array (or a bare top-level value). Add a comma if this
    // container already has a prior element.
    if (mDepth > 0) {
        if (mNeedComma[mDepth - 1]) {
            putc(',');
        }
        mNeedComma[mDepth - 1] = true;
    }
}

void JsonStreamWriter::prefixForKey() FL_NO_EXCEPT {
    if (mDepth > 0) {
        if (mNeedComma[mDepth - 1]) {
            putc(',');
        }
        mNeedComma[mDepth - 1] = true;
    }
}

void JsonStreamWriter::beginObject() FL_NO_EXCEPT {
    prefixForValue();
    putc('{');
    if (mDepth < kMaxDepth) {
        mNeedComma[mDepth] = false;
    }
    ++mDepth;
}

void JsonStreamWriter::endObject() FL_NO_EXCEPT {
    if (mDepth > 0) {
        --mDepth;
    }
    putc('}');
}

void JsonStreamWriter::beginArray() FL_NO_EXCEPT {
    prefixForValue();
    putc('[');
    if (mDepth < kMaxDepth) {
        mNeedComma[mDepth] = false;
    }
    ++mDepth;
}

void JsonStreamWriter::endArray() FL_NO_EXCEPT {
    if (mDepth > 0) {
        --mDepth;
    }
    putc(']');
}

void JsonStreamWriter::key(const char *k) FL_NO_EXCEPT {
    prefixForKey();
    putEscaped(k);
    putc(':');
    mPendingKey = true;
}

void JsonStreamWriter::value(const char *s) FL_NO_EXCEPT {
    prefixForValue();
    putEscaped(s);
}

void JsonStreamWriter::value(fl::i64 n) FL_NO_EXCEPT {
    prefixForValue();
    char buf[24];
    int len = fl::itoa64(n, buf, 10);
    for (int i = 0; i < len; ++i) {
        putc(buf[i]);
    }
}

void JsonStreamWriter::value(bool b) FL_NO_EXCEPT {
    prefixForValue();
    puts(b ? "true" : "false");
}

void JsonStreamWriter::valueNull() FL_NO_EXCEPT {
    prefixForValue();
    puts("null");
}

} // namespace fl
