// Decode a file sent via JSON-RPC + base64 transport.
// Reads the payload from a hardcoded path (.build/decode_payload.json) by
// default. Override with the FASTLED_DECODE_PAYLOAD env var. When the payload
// file does not exist, codec tests are skipped so the normal test suite is
// unaffected. An always-run test validates the base64 roundtrip through the
// JSON-RPC pipeline.
//
// Usage:
//   bash validate --decode tests/data/codec/test.mp4
//   bash test decode_file --cpp                          (uses default path)
//   FASTLED_DECODE_PAYLOAD=<payload.json> bash test decode_file --cpp

// ok standalone // ok cpp include
#include "test.h"

#include "fl/remote/remote.h"

#if FASTLED_ENABLE_JSON

#include <cstdio>   // ok include
#include <cstdlib>  // ok include

#include "fl/remote/rpc/base64.h"
#include "fl/stl/json.h"
#include "fl/string.h"
#include "fl/vector.h"

// Codec headers
#include "fl/codec/gif.h"
#include "fl/codec/h264.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/mp3.h"
#include "fl/codec/mp4_parser.h"
#include "fl/codec/mpeg1.h"

// For decoders that use ByteStream
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

// ---------------------------------------------------------------------------
// TestIO — same pattern as tests/fl/remote/remote.cpp
// ---------------------------------------------------------------------------

struct TestIO {
    fl::vector<fl::json> requests;
    fl::vector<fl::json> responses;
    size_t requestIndex = 0;

    fl::optional<fl::json> pullRequest() {
        if (requestIndex >= requests.size()) {
            return fl::nullopt;
        }
        return requests[requestIndex++];
    }

    void pushResponse(const fl::json& response) {
        responses.push_back(response);
    }
};

// ---------------------------------------------------------------------------
// Constants & helpers
// ---------------------------------------------------------------------------

static const char* DEFAULT_PAYLOAD_PATH = ".build/decode_payload.json";

static const char* getPayloadPath() {
    const char* env = getenv("FASTLED_DECODE_PAYLOAD");
    if (env && env[0]) {
        return env;
    }
    return DEFAULT_PAYLOAD_PATH;
}

static fl::string readFileToString(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return fl::string();
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return fl::string();
    }
    fl::vector<char> buf(len);
    size_t read = fread(buf.data(), 1, len, f);
    fclose(f);
    return fl::string(buf.data(), read);
}

// ---------------------------------------------------------------------------
// Codec validation helpers
// ---------------------------------------------------------------------------

static void validateMp4(const fl::vector<fl::u8>& data) {
    fl::string error;
    fl::Mp4TrackInfo track = fl::parseMp4(data, &error);
    FL_CHECK_MESSAGE(track.isValid, "MP4 parse failed: " << error);
    if (track.isValid) {
        FL_CHECK_GT(track.width, 0);
        FL_CHECK_GT(track.height, 0);
        FL_CHECK_GT(track.sps.size(), 0);
        FL_CHECK_GT(track.pps.size(), 0);
        FL_MESSAGE("MP4: " << track.width << "x" << track.height
                   << " profile=" << (int)track.profile
                   << " level=" << (int)track.level);
    }

    fl::H264Info info = fl::H264::parseH264Info(data, &error);
    FL_CHECK_MESSAGE(info.isValid, "H264 info parse failed: " << error);
    if (info.isValid) {
        FL_CHECK_GT(info.width, 0);
        FL_CHECK_GT(info.height, 0);
        FL_MESSAGE("H264: " << info.width << "x" << info.height);
    }

    if (track.isValid) {
        fl::vector<fl::u8> annexB = fl::extractH264NalUnits(data, track, &error);
        FL_CHECK_GT(annexB.size(), 0);
    }
}

static void validateMpeg(const fl::vector<fl::u8>& data) {
    fl::string error;
    fl::Mpeg1Info info = fl::Mpeg1::parseMpeg1Info(data, &error);
    FL_CHECK_MESSAGE(info.isValid, "MPEG1 metadata parse failed: " << error);
    if (info.isValid) {
        FL_CHECK_GT(info.width, 0);
        FL_CHECK_GT(info.height, 0);
        FL_MESSAGE("MPEG1: " << info.width << "x" << info.height
                   << " fps=" << info.frameRate);
    }

    if (fl::Mpeg1::isSupported()) {
        fl::Mpeg1Config config;
        config.mode = fl::Mpeg1Config::SingleFrame;
        fl::string dec_error;
        auto decoder = fl::Mpeg1::createDecoder(config, &dec_error);
        FL_REQUIRE_MESSAGE(decoder, "MPEG1 decoder creation failed: " << dec_error);
        auto stream = fl::make_shared<fl::MemoryFileHandle>(data.size());
        stream->write(data);
        FL_CHECK(decoder->begin(stream));
        auto result = decoder->decode();
        if (result == fl::DecodeResult::Success) {
            fl::Frame frame = decoder->getCurrentFrame();
            FL_CHECK(frame.isValid());
            FL_CHECK_GT(frame.getWidth(), 0);
            FL_CHECK_GT(frame.getHeight(), 0);
            FL_MESSAGE("MPEG1 first frame: " << frame.getWidth() << "x" << frame.getHeight());
        }
        decoder->end();
    } else {
        FL_MESSAGE("MPEG1 decoder not supported on this platform");
    }
}

static void validateGif(const fl::vector<fl::u8>& data) {
    fl::string error;
    fl::GifInfo info = fl::Gif::parseGifInfo(data, &error);
    FL_CHECK_MESSAGE(info.isValid, "GIF metadata parse failed: " << error);
    if (info.isValid) {
        FL_CHECK_GT(info.width, 0);
        FL_CHECK_GT(info.height, 0);
        FL_MESSAGE("GIF: " << info.width << "x" << info.height
                   << " frames=" << info.frameCount);
    }

    if (fl::Gif::isSupported()) {
        fl::GifConfig config;
        config.mode = fl::GifConfig::SingleFrame;
        config.format = fl::PixelFormat::RGB888;
        fl::string dec_error;
        auto decoder = fl::Gif::createDecoder(config, &dec_error);
        FL_REQUIRE_MESSAGE(decoder, "GIF decoder creation failed: " << dec_error);
        auto stream = fl::make_shared<fl::MemoryFileHandle>(data.size());
        stream->write(data);
        FL_REQUIRE(decoder->begin(stream));
        auto result = decoder->decode();
        if (result == fl::DecodeResult::Success) {
            fl::Frame frame = decoder->getCurrentFrame();
            FL_CHECK(frame.isValid());
            FL_CHECK_GT(frame.getWidth(), 0);
            FL_CHECK_GT(frame.getHeight(), 0);
            FL_MESSAGE("GIF first frame: " << frame.getWidth() << "x" << frame.getHeight());
        }
        decoder->end();
    } else {
        FL_MESSAGE("GIF decoder not supported on this platform");
    }
}

static void validateJpeg(const fl::vector<fl::u8>& data) {
    if (fl::Jpeg::isSupported()) {
        fl::JpegConfig config;
        config.format = fl::PixelFormat::RGB888;
        config.quality = fl::JpegConfig::Quality::High;
        fl::string dec_error;
        fl::FramePtr frame = fl::Jpeg::decode(config, data, &dec_error);
        FL_REQUIRE_MESSAGE(frame, "JPEG decode failed: " << dec_error);
        FL_CHECK(frame->isValid());
        FL_CHECK_GT(frame->getWidth(), 0);
        FL_CHECK_GT(frame->getHeight(), 0);
        FL_MESSAGE("JPEG: " << frame->getWidth() << "x" << frame->getHeight());
    } else {
        FL_MESSAGE("JPEG decoder not supported on this platform");
        FL_CHECK_GE(data.size(), 2);
        FL_CHECK_EQ(data[0], 0xFF);
        FL_CHECK_EQ(data[1], 0xD8);
    }
}

static void validateMp3(const fl::vector<fl::u8>& data) {
    FL_CHECK_GT(data.size(), 0);
    fl::third_party::Mp3HelixDecoder decoder;
    FL_CHECK(decoder.init());
    int frames_decoded = 0;
    int sample_rate = 0;
    decoder.decode(data.data(), data.size(), [&](const fl::third_party::Mp3Frame& frame) {
        frames_decoded++;
        if (sample_rate == 0) {
            sample_rate = frame.sample_rate;
        }
    });
    FL_CHECK_GT(frames_decoded, 0);
    FL_CHECK_GT(sample_rate, 0);
    FL_MESSAGE("MP3: " << frames_decoded << " frames, " << sample_rate << " Hz");
}

static void validateWebP(const fl::vector<fl::u8>& data) {
    if (fl::Jpeg::isSupported()) {
        fl::JpegConfig config;
        config.format = fl::PixelFormat::RGB888;
        fl::string dec_error;
        fl::FramePtr frame = fl::Jpeg::decode(config, data, &dec_error);
        if (frame) {
            FL_CHECK(frame->isValid());
            FL_CHECK_GT(frame->getWidth(), 0);
            FL_CHECK_GT(frame->getHeight(), 0);
            FL_MESSAGE("WebP: " << frame->getWidth() << "x" << frame->getHeight());
        } else {
            FL_MESSAGE("WebP decode returned null: " << dec_error);
        }
    } else {
        FL_MESSAGE("WebP/JPEG decoder not supported on this platform");
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Always-run test: validates base64 roundtrip through JSON-RPC pipeline
FL_TEST_CASE("decode_file: base64 roundtrip via JSON-RPC") {
    // Create known binary data with all byte values 0-255
    fl::vector<fl::u8> original(256);
    for (int i = 0; i < 256; i++) {
        original[i] = static_cast<fl::u8>(i);
    }

    // Base64 encode
    fl::string b64 = fl::base64_encode(original);
    FL_REQUIRE(!b64.empty());

    // Build JSON-RPC request with base64 data as first param
    fl::json params = fl::json::array();
    params.push_back(fl::json(b64.c_str()));       // base64 data
    params.push_back(fl::json(".bin"));              // extension

    fl::json request = fl::json::object();
    request.set("method", "decodeFile");
    request.set("params", params);
    request.set("id", 1);

    // Set up Remote with TestIO
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::json& r) { io.pushResponse(r); }
    );

    // Bind handler — fl::vector<fl::u8> auto-decodes from base64
    fl::vector<fl::u8> received_data;
    fl::string received_ext;
    remote.bind("decodeFile", [&](fl::vector<fl::u8> data, fl::string ext) {
        received_data = data;
        received_ext = ext;
    });

    // Dispatch
    io.requests.push_back(request);
    remote.update(0);

    // Verify handler was called with correct decoded data
    FL_REQUIRE_EQ(received_data.size(), original.size());
    for (fl::size i = 0; i < original.size(); i++) {
        FL_CHECK_EQ(received_data[i], original[i]);
    }
    FL_CHECK_EQ(received_ext, fl::string(".bin"));

    // Verify response indicates success (no error)
    FL_REQUIRE_EQ(io.responses.size(), 1);
    FL_CHECK(!io.responses[0].contains("error"));
}

// Codec test: reads payload from hardcoded path or env var override
FL_TEST_CASE("decode_file: JSON-RPC dispatch") {
    const char* payload_path = getPayloadPath();

    // Read the JSON-RPC payload file (skip if it doesn't exist)
    fl::string payload_str = readFileToString(payload_path);
    if (payload_str.empty()) {
        FL_MESSAGE("Payload file not found at " << payload_path << " — skipping");
        return;
    }
    FL_REQUIRE_MESSAGE(!payload_str.empty(),
                       "Failed to read payload file: " << payload_path);

    // Parse as JSON
    fl::json payload = fl::json::parse(payload_str);
    FL_REQUIRE_MESSAGE(!payload.is_null(), "Failed to parse payload JSON");
    FL_REQUIRE(payload.contains("method"));
    FL_REQUIRE(payload.contains("params"));

    // Set up Remote with TestIO
    TestIO io;
    fl::Remote remote(
        [&io]() { return io.pullRequest(); },
        [&io](const fl::json& r) { io.pushResponse(r); }
    );

    // Bind handler — fl::vector<fl::u8> auto-decodes from base64
    fl::vector<fl::u8> decoded_data;
    fl::string decoded_ext;
    bool handler_called = false;
    remote.bind("decodeFile", [&](fl::vector<fl::u8> data, fl::string ext) {
        decoded_data = data;
        decoded_ext = ext;
        handler_called = true;
    });

    // Feed the payload into Remote
    io.requests.push_back(payload);
    remote.update(0);

    // Verify handler was called
    FL_REQUIRE_MESSAGE(handler_called, "decodeFile handler was not called");
    FL_REQUIRE_MESSAGE(!decoded_data.empty(), "Decoded data is empty");
    FL_REQUIRE_MESSAGE(!decoded_ext.empty(), "Extension is empty");

    // Verify response has no error
    FL_REQUIRE_EQ(io.responses.size(), 1);
    FL_CHECK_MESSAGE(!io.responses[0].contains("error"),
                     "JSON-RPC error: " << io.responses[0].to_string());

    FL_MESSAGE("Decoded " << decoded_data.size() << " bytes, extension=" << decoded_ext);

    // Dispatch to codec-specific validation
    if (decoded_ext == ".mp4") {
        validateMp4(decoded_data);
    } else if (decoded_ext == ".mpeg" || decoded_ext == ".mpg") {
        validateMpeg(decoded_data);
    } else if (decoded_ext == ".gif") {
        validateGif(decoded_data);
    } else if (decoded_ext == ".jpg" || decoded_ext == ".jpeg") {
        validateJpeg(decoded_data);
    } else if (decoded_ext == ".mp3") {
        validateMp3(decoded_data);
    } else if (decoded_ext == ".webp") {
        validateWebP(decoded_data);
    } else {
        FL_FAIL("Unknown extension: " << decoded_ext);
    }
}

#endif // FASTLED_ENABLE_JSON
