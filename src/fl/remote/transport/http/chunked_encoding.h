#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"

namespace fl {

// ChunkedReader: Parse HTTP/1.1 chunked transfer encoding
// Format: <chunk-size-hex>\r\n<chunk-data>\r\n ... 0\r\n\r\n
class ChunkedReader {
public:
    ChunkedReader();

    // Feed data from socket
    void feed(const u8* data, size_t len);

    // Check if complete chunk is available
    bool hasChunk() const;

    // Read next chunk (returns empty if none available)
    fl::optional<fl::vector<u8>> readChunk();

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

    // Write chunk (returns formatted chunk data: <size-hex>\r\n<data>\r\n)
    fl::vector<u8> writeChunk(const u8* data, size_t len);

    // Write final chunk (returns: 0\r\n\r\n)
    fl::vector<u8> writeFinal();

private:
    // Format chunk: <size-hex>\r\n<data>\r\n
    static fl::vector<u8> formatChunk(const u8* data, size_t len);
};

} // namespace fl
