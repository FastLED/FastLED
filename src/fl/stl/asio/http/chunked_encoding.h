#pragma once

#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

// Result of a ChunkedReader::readChunk() call
struct ChunkedReadResult {
    enum Status {
        NO_DATA,     // No complete chunk available yet
        DATA,        // Chunk data written to output span
        FINAL        // Final (zero-length) chunk received, stream ended
    };

    Status status;
    fl::span<const u8> data;  // Subspan of caller's buffer containing the bytes read

    ChunkedReadResult() : status(NO_DATA) {}
    ChunkedReadResult(Status s, fl::span<const u8> d) : status(s), data(d) {}

    bool hasData() const { return status == DATA; }
    bool isFinal() const { return status == FINAL; }
};

// ChunkedReader: Parse HTTP/1.1 chunked transfer encoding
// Format: <chunk-size-hex>\r\n<chunk-data>\r\n ... 0\r\n\r\n
class ChunkedReader {
public:
    ChunkedReader();

    // Feed raw bytes received from a socket into the parser (incremental/streaming)
    void feed(fl::span<const u8> data);

    // Check if complete chunk is available
    bool hasChunk() const;

    // Read next chunk into caller-provided buffer.
    // On CHUNKED_DATA, result.data is a subspan of `out` containing the bytes read.
    // If the output span is too small, the chunk remains buffered (returns CHUNKED_NO_DATA).
    ChunkedReadResult readChunk(fl::span<u8> out);

    // Peek the size of the next available chunk (0 if none)
    size_t nextChunkSize() const;

    // Check if final chunk (size 0) received
    bool isFinal() const;

    // Reset state
    void reset();

private:
    enum State {
        READ_SIZE,      // Reading chunk size (hex + CRLF)
        READ_DATA,      // Reading chunk data
        READ_TRAILER,   // Reading trailing CRLF
        FINAL           // Final chunk received
    };

    State mState;
    fl::vector<u8> mBuffer;
    size_t mChunkSize;
    size_t mBytesRead;
    fl::vector<fl::vector<u8>> mChunks;  // Buffered complete chunks
    fl::vector<u8> mCurrentChunk;  // Chunk being assembled (not yet complete)

    // Parse hex chunk size from buffer
    bool parseChunkSize(size_t& outSize);

    // Check if buffer contains CRLF at current position
    bool hasCRLF() const;

    // Consume n bytes from buffer
    void consume(size_t n);
};

// ChunkedWriter: Format HTTP/1.1 chunked transfer encoding
class ChunkedWriter {
public:
    ChunkedWriter();

    // Write chunk into caller-provided buffer.
    // Output format: <size-hex>\r\n<data>\r\n
    // Returns number of bytes written, or 0 if output span is too small.
    size_t writeChunk(fl::span<const u8> data, fl::span<u8> out);

    // Write final chunk into caller-provided buffer.
    // Output format: 0\r\n\r\n (always 5 bytes)
    // Returns number of bytes written, or 0 if output span is too small.
    size_t writeFinal(fl::span<u8> out);

    // Compute the output size needed for writeChunk with the given data length
    static size_t chunkOverhead(size_t dataLen);

    // Final chunk is always 5 bytes: "0\r\n\r\n"
    static constexpr size_t FINAL_SIZE = 5;
};

} // namespace fl
