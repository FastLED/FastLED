#include "test.h"
#include "fl/stl/asio/http/chunked_encoding.cpp.hpp"

FL_TEST_FILE(FL_FILEPATH) {

// Helper: wrap a C string as a span for feed()
static fl::span<const uint8_t> asSpan(const char* s, size_t len) {
    return fl::span<const uint8_t>(reinterpret_cast<const uint8_t*>(s), len); // ok reinterpret cast
}
static fl::span<const uint8_t> asSpan(const char* s) {
    return asSpan(s, fl::strlen(s));
}


FL_TEST_CASE("ChunkedReader: Parse single chunk") {
    fl::ChunkedReader reader;

    // Feed chunk: "5\r\nHello\r\n"
    const char* data = "5\r\nHello\r\n";
    reader.feed(asSpan(data));

    FL_REQUIRE(reader.hasChunk());
    FL_REQUIRE(reader.nextChunkSize() == 5);
    uint8_t buf[64];
    auto result = reader.readChunk(buf);
    FL_REQUIRE(result.hasData());
    FL_REQUIRE(result.data.size() == 5);
    FL_REQUIRE(fl::memcmp(result.data.data(), "Hello", 5) == 0);
    FL_REQUIRE(!reader.hasChunk());
}

FL_TEST_CASE("ChunkedReader: Parse multiple chunks") {
    fl::ChunkedReader reader;

    // Feed chunks: "5\r\nHello\r\n5\r\nWorld\r\n"
    const char* data = "5\r\nHello\r\n5\r\nWorld\r\n";
    reader.feed(asSpan(data));

    uint8_t buf[64];

    FL_REQUIRE(reader.hasChunk());
    auto r1 = reader.readChunk(buf);
    FL_REQUIRE(r1.hasData());
    FL_REQUIRE(r1.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r1.data.data(), "Hello", 5) == 0);

    FL_REQUIRE(reader.hasChunk());
    auto r2 = reader.readChunk(buf);
    FL_REQUIRE(r2.hasData());
    FL_REQUIRE(r2.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r2.data.data(), "World", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
}

FL_TEST_CASE("ChunkedReader: Parse final chunk") {
    fl::ChunkedReader reader;

    // Feed chunks: "5\r\nHello\r\n0\r\n\r\n"
    const char* data = "5\r\nHello\r\n0\r\n\r\n";
    reader.feed(asSpan(data));

    uint8_t buf[64];

    FL_REQUIRE(reader.hasChunk());
    auto r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r.data.data(), "Hello", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
    FL_REQUIRE(reader.isFinal());

    // Reading again should return final
    auto r2 = reader.readChunk(buf);
    FL_REQUIRE(r2.isFinal());
    FL_REQUIRE(r2.data.size() == 0);
}

FL_TEST_CASE("ChunkedReader: Parse incremental chunks") {
    fl::ChunkedReader reader;

    // Feed data incrementally
    reader.feed(asSpan("5", 1));
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(asSpan("\r\n", 2));
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(asSpan("Hel", 3));
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(asSpan("lo", 2));
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(asSpan("\r\n", 2));
    FL_REQUIRE(reader.hasChunk());

    uint8_t buf[64];
    auto r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r.data.data(), "Hello", 5) == 0);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with hex size") {
    fl::ChunkedReader reader;

    // Feed chunk: "a\r\n0123456789\r\n" (10 bytes)
    const char* data = "a\r\n0123456789\r\n";
    reader.feed(asSpan(data));

    uint8_t buf[64];
    FL_REQUIRE(reader.hasChunk());
    auto r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 10);
    FL_REQUIRE(fl::memcmp(r.data.data(), "0123456789", 10) == 0);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with uppercase hex") {
    fl::ChunkedReader reader;

    // Feed chunk: "A\r\n0123456789\r\n" (10 bytes)
    const char* data = "A\r\n0123456789\r\n";
    reader.feed(asSpan(data));

    uint8_t buf[64];
    FL_REQUIRE(reader.hasChunk());
    auto r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 10);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with extensions (ignore)") {
    fl::ChunkedReader reader;

    // Feed chunk: "5;name=value\r\nHello\r\n"
    const char* data = "5;name=value\r\nHello\r\n";
    reader.feed(asSpan(data));

    uint8_t buf[64];
    FL_REQUIRE(reader.hasChunk());
    auto r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r.data.data(), "Hello", 5) == 0);
}

FL_TEST_CASE("ChunkedReader: Buffer too small returns NO_DATA") {
    fl::ChunkedReader reader;

    const char* data = "5\r\nHello\r\n";
    reader.feed(asSpan(data));

    FL_REQUIRE(reader.hasChunk());

    // Buffer too small
    uint8_t tinyBuf[2];
    auto r = reader.readChunk(tinyBuf);
    FL_REQUIRE(!r.hasData());
    FL_REQUIRE(r.data.size() == 0);

    // Chunk is still available
    FL_REQUIRE(reader.hasChunk());

    // Now read with adequate buffer
    uint8_t buf[64];
    r = reader.readChunk(buf);
    FL_REQUIRE(r.hasData());
    FL_REQUIRE(r.data.size() == 5);
}

FL_TEST_CASE("ChunkedReader: Reset state") {
    fl::ChunkedReader reader;

    // Feed chunk
    const char* data = "5\r\nHello\r\n";
    reader.feed(asSpan(data));
    FL_REQUIRE(reader.hasChunk());

    // Reset
    reader.reset();
    FL_REQUIRE(!reader.hasChunk());
    FL_REQUIRE(!reader.isFinal());
}

FL_TEST_CASE("ChunkedWriter: Write single chunk") {
    fl::ChunkedWriter writer;

    // Write chunk "Hello"
    const char* data = "Hello";
    uint8_t buf[64];
    size_t written = writer.writeChunk(asSpan(data), fl::span<uint8_t>(buf, sizeof(buf)));

    // Expected: "5\r\nHello\r\n"
    const char* expected = "5\r\nHello\r\n";
    FL_REQUIRE(written == fl::strlen(expected));
    FL_REQUIRE(fl::memcmp(buf, expected, written) == 0);
}

FL_TEST_CASE("ChunkedWriter: Write final chunk") {
    fl::ChunkedWriter writer;

    uint8_t buf[64];
    size_t written = writer.writeFinal(fl::span<uint8_t>(buf, sizeof(buf)));

    // Expected: "0\r\n\r\n"
    const char* expected = "0\r\n\r\n";
    FL_REQUIRE(written == fl::strlen(expected));
    FL_REQUIRE(fl::memcmp(buf, expected, written) == 0);
}

FL_TEST_CASE("ChunkedWriter: Write large chunk") {
    fl::ChunkedWriter writer;

    // Write 256 bytes
    fl::vector<uint8_t> data(256, 'A');
    uint8_t buf[512];
    size_t written = writer.writeChunk(data, fl::span<uint8_t>(buf, sizeof(buf)));

    // Expected: "100\r\n<256 bytes>\r\n"
    FL_REQUIRE(written == 5 + 256 + 2);  // "100\r\n" + data + "\r\n"
    FL_REQUIRE(buf[0] == '1');
    FL_REQUIRE(buf[1] == '0');
    FL_REQUIRE(buf[2] == '0');
    FL_REQUIRE(buf[3] == '\r');
    FL_REQUIRE(buf[4] == '\n');
    FL_REQUIRE(buf[written - 2] == '\r');
    FL_REQUIRE(buf[written - 1] == '\n');
}

FL_TEST_CASE("ChunkedWriter: Buffer too small returns 0") {
    fl::ChunkedWriter writer;

    const char* data = "Hello";
    uint8_t tinyBuf[3];
    size_t written = writer.writeChunk(asSpan(data), fl::span<uint8_t>(tinyBuf, sizeof(tinyBuf)));
    FL_REQUIRE(written == 0);
}

FL_TEST_CASE("ChunkedReader/Writer: Round-trip test") {
    fl::ChunkedWriter writer;
    fl::ChunkedReader reader;

    // Write chunks into buffers
    const char* data1 = "Hello";
    const char* data2 = "World";
    uint8_t wbuf1[64], wbuf2[64], wbuf3[64];
    size_t w1 = writer.writeChunk(asSpan(data1), fl::span<uint8_t>(wbuf1, sizeof(wbuf1)));
    size_t w2 = writer.writeChunk(asSpan(data2), fl::span<uint8_t>(wbuf2, sizeof(wbuf2)));
    size_t w3 = writer.writeFinal(fl::span<uint8_t>(wbuf3, sizeof(wbuf3)));

    // Concatenate chunks
    fl::vector<uint8_t> combined;
    combined.insert(combined.end(), wbuf1, wbuf1 + w1);
    combined.insert(combined.end(), wbuf2, wbuf2 + w2);
    combined.insert(combined.end(), wbuf3, wbuf3 + w3);

    // Feed to reader
    reader.feed(combined);

    // Read chunks
    uint8_t rbuf[64];

    FL_REQUIRE(reader.hasChunk());
    auto r1 = reader.readChunk(rbuf);
    FL_REQUIRE(r1.hasData());
    FL_REQUIRE(r1.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r1.data.data(), "Hello", 5) == 0);

    FL_REQUIRE(reader.hasChunk());
    auto r2 = reader.readChunk(rbuf);
    FL_REQUIRE(r2.hasData());
    FL_REQUIRE(r2.data.size() == 5);
    FL_REQUIRE(fl::memcmp(r2.data.data(), "World", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
    FL_REQUIRE(reader.isFinal());
}

} // FL_TEST_FILE
