#pragma once

// JsonStreamWriter — emit a JSON document incrementally to a byte sink
// without ever allocating a buffer proportional to the document size.
//
// Motivation (FastLED#3588 follow-up): building a large JSON-RPC response
// as an fl::json object and then serializing it to an fl::string allocates
// TWO buffers proportional to the whole response (N-sized), which exhausts
// the heap on classic ESP32 once WiFi is up. A streaming writer keeps only
// a small fixed buffer live, so an arbitrarily large response (e.g. a
// `help` listing of hundreds of methods) can be returned with a bounded,
// tiny memory footprint. Peak memory = one small buffer + the nesting
// depth, not the document.
//
// The writer performs no heap allocation itself: the scratch buffer and the
// container-nesting stack are fixed-size members. It handles commas between
// elements, object key/value alternation, and JSON string escaping.

#include "fl/stl/int.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {

class JsonStreamWriter {
  public:
    // Sink receives UTF-8 bytes to write to the transport (serial, socket,
    // …). It is called many times with small spans; it must not retain the
    // pointer past the call.
    using Sink = fl::function<void(const char *data, fl::size len)>;

    // kMaxDepth bounds object/array nesting. JSON-RPC responses are shallow;
    // 24 is far more than enough and costs 24 bytes.
    static constexpr fl::size kMaxDepth = 24;
    // Scratch buffer size. Bytes accumulate here and flush to the sink when
    // full, so the sink sees reasonably-sized writes without any heap use.
    static constexpr fl::size kBufSize = 128;

    explicit JsonStreamWriter(Sink sink) FL_NO_EXCEPT;
    ~JsonStreamWriter() FL_NO_EXCEPT;  // flushes

    JsonStreamWriter(const JsonStreamWriter &) FL_NO_EXCEPT = delete;
    JsonStreamWriter &operator=(const JsonStreamWriter &) FL_NO_EXCEPT = delete;

    // Structure.
    void beginObject() FL_NO_EXCEPT;
    void endObject() FL_NO_EXCEPT;
    void beginArray() FL_NO_EXCEPT;
    void endArray() FL_NO_EXCEPT;

    // Object key. Must be followed by exactly one value (or begin*).
    void key(const char *k) FL_NO_EXCEPT;

    // Values.
    void value(const char *s) FL_NO_EXCEPT;   // JSON string (escaped)
    void value(fl::i64 n) FL_NO_EXCEPT;       // JSON number
    void value(int n) FL_NO_EXCEPT { value(static_cast<fl::i64>(n)); }
    void value(bool b) FL_NO_EXCEPT;          // true / false
    void valueNull() FL_NO_EXCEPT;            // null

    // Convenience: key + value in one call.
    void member(const char *k, const char *s) FL_NO_EXCEPT { key(k); value(s); }
    void member(const char *k, fl::i64 n) FL_NO_EXCEPT { key(k); value(n); }
    void member(const char *k, int n) FL_NO_EXCEPT { key(k); value(static_cast<fl::i64>(n)); }
    void member(const char *k, bool b) FL_NO_EXCEPT { key(k); value(b); }

    // Flush the scratch buffer to the sink. Called automatically on
    // destruction; call explicitly to finish a top-level document.
    void flush() FL_NO_EXCEPT;

  private:
    void writeChar(char c) FL_NO_EXCEPT;
    void puts(const char *s) FL_NO_EXCEPT;
    void putEscaped(const char *s) FL_NO_EXCEPT;
    void prefixForValue() FL_NO_EXCEPT;  // emit comma/nothing before a value
    void prefixForKey() FL_NO_EXCEPT;    // emit comma before a new key

    Sink mSink;
    char mBuf[kBufSize];
    fl::size mLen;                 // bytes currently in mBuf
    bool mNeedComma[kMaxDepth];    // does the current container have a prior element?
    fl::size mDepth;              // current nesting depth
    bool mPendingKey;             // true after key(), expecting a value next
};

} // namespace fl
