#include "test.h"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"

FL_TEST_CASE("ChunkedReader: Parse single chunk") {
    fl::ChunkedReader reader;

    // Feed chunk: "5\r\nHello\r\n"
    const char* data = "5\r\nHello\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 5);
    FL_REQUIRE(memcmp(chunk->data(), "Hello", 5) == 0);
    FL_REQUIRE(!reader.hasChunk());
}

FL_TEST_CASE("ChunkedReader: Parse multiple chunks") {
    fl::ChunkedReader reader;

    // Feed chunks: "5\r\nHello\r\n5\r\nWorld\r\n"
    const char* data = "5\r\nHello\r\n5\r\nWorld\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk1 = reader.readChunk();
    FL_REQUIRE(chunk1.has_value());
    FL_REQUIRE(chunk1->size() == 5);
    FL_REQUIRE(memcmp(chunk1->data(), "Hello", 5) == 0);

    FL_REQUIRE(reader.hasChunk());
    auto chunk2 = reader.readChunk();
    FL_REQUIRE(chunk2.has_value());
    FL_REQUIRE(chunk2->size() == 5);
    FL_REQUIRE(memcmp(chunk2->data(), "World", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
}

FL_TEST_CASE("ChunkedReader: Parse final chunk") {
    fl::ChunkedReader reader;

    // Feed chunks: "5\r\nHello\r\n0\r\n\r\n"
    const char* data = "5\r\nHello\r\n0\r\n\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 5);
    FL_REQUIRE(memcmp(chunk->data(), "Hello", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
    FL_REQUIRE(reader.isFinal());
}

FL_TEST_CASE("ChunkedReader: Parse incremental chunks") {
    fl::ChunkedReader reader;

    // Feed data incrementally
    reader.feed(reinterpret_cast<const uint8_t*>("5"), 1);
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(reinterpret_cast<const uint8_t*>("\r\n"), 2);
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(reinterpret_cast<const uint8_t*>("Hel"), 3);
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(reinterpret_cast<const uint8_t*>("lo"), 2);
    FL_REQUIRE(!reader.hasChunk());

    reader.feed(reinterpret_cast<const uint8_t*>("\r\n"), 2);
    FL_REQUIRE(reader.hasChunk());

    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 5);
    FL_REQUIRE(memcmp(chunk->data(), "Hello", 5) == 0);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with hex size") {
    fl::ChunkedReader reader;

    // Feed chunk: "a\r\n0123456789\r\n" (10 bytes)
    const char* data = "a\r\n0123456789\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 10);
    FL_REQUIRE(memcmp(chunk->data(), "0123456789", 10) == 0);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with uppercase hex") {
    fl::ChunkedReader reader;

    // Feed chunk: "A\r\n0123456789\r\n" (10 bytes)
    const char* data = "A\r\n0123456789\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 10);
}

FL_TEST_CASE("ChunkedReader: Parse chunk with extensions (ignore)") {
    fl::ChunkedReader reader;

    // Feed chunk: "5;name=value\r\nHello\r\n"
    const char* data = "5;name=value\r\nHello\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));

    FL_REQUIRE(reader.hasChunk());
    auto chunk = reader.readChunk();
    FL_REQUIRE(chunk.has_value());
    FL_REQUIRE(chunk->size() == 5);
    FL_REQUIRE(memcmp(chunk->data(), "Hello", 5) == 0);
}

FL_TEST_CASE("ChunkedReader: Reset state") {
    fl::ChunkedReader reader;

    // Feed chunk
    const char* data = "5\r\nHello\r\n";
    reader.feed(reinterpret_cast<const uint8_t*>(data), strlen(data));
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
    auto chunk = writer.writeChunk(reinterpret_cast<const uint8_t*>(data), 5);

    // Expected: "5\r\nHello\r\n"
    const char* expected = "5\r\nHello\r\n";
    FL_REQUIRE(chunk.size() == strlen(expected));
    FL_REQUIRE(memcmp(chunk.data(), expected, chunk.size()) == 0);
}

FL_TEST_CASE("ChunkedWriter: Write final chunk") {
    fl::ChunkedWriter writer;

    auto chunk = writer.writeFinal();

    // Expected: "0\r\n\r\n"
    const char* expected = "0\r\n\r\n";
    FL_REQUIRE(chunk.size() == strlen(expected));
    FL_REQUIRE(memcmp(chunk.data(), expected, chunk.size()) == 0);
}

FL_TEST_CASE("ChunkedWriter: Write large chunk") {
    fl::ChunkedWriter writer;

    // Write 256 bytes
    fl::vector<uint8_t> data(256, 'A');
    auto chunk = writer.writeChunk(data.data(), data.size());

    // Expected: "100\r\n<256 bytes>\r\n"
    FL_REQUIRE(chunk.size() == 5 + 256 + 2);  // "100\r\n" + data + "\r\n"
    FL_REQUIRE(chunk[0] == '1');
    FL_REQUIRE(chunk[1] == '0');
    FL_REQUIRE(chunk[2] == '0');
    FL_REQUIRE(chunk[3] == '\r');
    FL_REQUIRE(chunk[4] == '\n');
    FL_REQUIRE(chunk[chunk.size() - 2] == '\r');
    FL_REQUIRE(chunk[chunk.size() - 1] == '\n');
}

FL_TEST_CASE("ChunkedReader/Writer: Round-trip test") {
    fl::ChunkedWriter writer;
    fl::ChunkedReader reader;

    // Write chunks
    const char* data1 = "Hello";
    const char* data2 = "World";
    auto chunk1 = writer.writeChunk(reinterpret_cast<const uint8_t*>(data1), 5);
    auto chunk2 = writer.writeChunk(reinterpret_cast<const uint8_t*>(data2), 5);
    auto chunkFinal = writer.writeFinal();

    // Concatenate chunks
    fl::vector<uint8_t> combined;
    combined.insert(combined.end(), chunk1.begin(), chunk1.end());
    combined.insert(combined.end(), chunk2.begin(), chunk2.end());
    combined.insert(combined.end(), chunkFinal.begin(), chunkFinal.end());

    // Feed to reader
    reader.feed(combined.data(), combined.size());

    // Read chunks
    FL_REQUIRE(reader.hasChunk());
    auto read1 = reader.readChunk();
    FL_REQUIRE(read1.has_value());
    FL_REQUIRE(read1->size() == 5);
    FL_REQUIRE(memcmp(read1->data(), "Hello", 5) == 0);

    FL_REQUIRE(reader.hasChunk());
    auto read2 = reader.readChunk();
    FL_REQUIRE(read2.has_value());
    FL_REQUIRE(read2->size() == 5);
    FL_REQUIRE(memcmp(read2->data(), "World", 5) == 0);

    FL_REQUIRE(!reader.hasChunk());
    FL_REQUIRE(reader.isFinal());
}
