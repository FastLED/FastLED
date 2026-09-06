
// g++ --std=c++11 test.cpp



#include "crgb.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/stl/shared_ptr.h"
#include "fl/fx/video.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/vector.h"
#include "test.h"
#include "fl/fs/fs.h"
#include "fl/gfx/crgb.h"
#include "fl/fx/fx.h"
#include "fl/fx/fx2d.h"
#include "fl/fx/frame.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/cstring.h"
#include "fl/math/xymap.h"
#include "fl/video/pixel_stream.h"
#include "fl/fled/color.h"
#include "fl/fled/pixel_format.h"
#include "FastLED.h"

FL_TEST_FILE(FL_FILEPATH) {

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define VIDEO_WIDTH 10
#define VIDEO_HEIGHT 10
#define LEDS_PER_FRAME VIDEO_WIDTH *VIDEO_HEIGHT

FASTLED_SHARED_PTR(FakeFilebuf);


class FakeFilebuf : public fl::filebuf {
  public:
    virtual ~FakeFilebuf() {}
    bool is_open() const override { return true; }
    bool available() const override { return mPos < data.size(); }
    size_t size() const override { return data.size(); }

    size_t writeData(const uint8_t *src, size_t len) {
        data.insert(data.end(), src, src + len);
        return len;
    }
    size_t writeCRGB(const CRGB *src, size_t len) {
        size_t bytes_written = writeData((const uint8_t *)src, len * 3);
        return bytes_written / 3;
    }
    size_t read(char *dst, size_t bytesToRead) override {
        size_t bytesRead = 0;
        while (bytesRead < bytesToRead && mPos < data.size()) {
            dst[bytesRead] = static_cast<char>(data[mPos]);
            bytesRead++;
            mPos++;
        }
        return bytesRead;
    }
    using fl::filebuf::read; // u8 overload
    size_t write(const char *dat, size_t count) override {
        (void)dat; (void)count;
        return 0;
    }
    size_t tell() override { return mPos; }
    const char *path() const override { return "fake"; }
    bool seek(size_t pos, fl::seek_dir dir) override {
        if (dir == fl::seek_dir::beg) { this->mPos = pos; }
        else if (dir == fl::seek_dir::cur) { this->mPos += pos; }
        else { this->mPos = data.size() + pos; }
        return true;
    }
    using fl::filebuf::seek; // single-arg overload
    void close() override {}
    bool is_eof() const override { return mPos >= data.size(); }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char *error_message() const override { return "No error"; }

    fl::vector<uint8_t> data;
    size_t mPos = 0;
};

class NonSeekableFakeFilebuf : public FakeFilebuf {
  public:
    bool seek(size_t, fl::seek_dir) override { return false; }
};

FL_TEST_CASE("PixelStream rejects zero and negative base frame sizes") {
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    const uint8_t byte = 0x42;
    FL_REQUIRE_EQ(fileHandle->writeData(&byte, 1), 1);

    FL_SUBCASE("zero") {
        fl::PixelStream stream(0);
        FL_CHECK_FALSE(stream.begin(fileHandle));
        FL_CHECK_FALSE(stream.available());
    }
    FL_SUBCASE("negative") {
        fl::PixelStream stream(-3);
        FL_CHECK_FALSE(stream.begin(fileHandle));
        FL_CHECK_FALSE(stream.available());
    }
}

FL_TEST_CASE("PixelStream rejects RGB16 FLED stride that overflows i32") {
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    const uint8_t rgb16Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
    };
    FL_REQUIRE_EQ(fileHandle->writeData(rgb16Fled, sizeof(rgb16Fled)),
                  sizeof(rgb16Fled));

    // This positive RGB8 base stride is divisible by three, but multiplying
    // its LED count by RGB16's six-byte storage does not fit in i32.
    fl::PixelStream stream(2147483646);
    FL_CHECK_FALSE(stream.begin(fileHandle));
    FL_CHECK_FALSE(stream.available());
}

FL_TEST_CASE("PixelStream rejects partial FLED frames and reserved header bytes") {
    struct InvalidFled {
        fl::u8 format;
        fl::u8 payloadBytes;
        fl::u8 reservedOffset;
    };
    const InvalidFled invalid[] = {
        {0x00, 4, 0}, // rgb8 tail
        {0x05, 7, 0}, // rgb16 tail
        {0x00, 3, 6}, // reserved byte 0
        {0x00, 3, 7}, // reserved byte 1
    };
    for (const InvalidFled& item : invalid) {
        FakeFilebufPtr file = fl::make_shared<FakeFilebuf>();
        fl::vector<fl::u8> bytes(12 + item.payloadBytes, 0);
        bytes[0] = 'F'; bytes[1] = 'L'; bytes[2] = 'E'; bytes[3] = 'D';
        bytes[4] = 1;
        bytes[5] = item.format;
        if (item.reservedOffset != 0) {
            bytes[item.reservedOffset] = 1;
        }
        FL_REQUIRE_EQ(file->writeData(bytes.data(), bytes.size()), bytes.size());
        fl::PixelStream stream(3);
        CRGB pixel = CRGB::Red;
        FL_CHECK_FALSE(stream.begin(file));
        FL_CHECK_FALSE(stream.available());
        FL_CHECK_FALSE(stream.readPixel(&pixel));
        FL_CHECK_EQ(pixel, CRGB::Red);
    }
}

FL_TEST_CASE("Video Frame draw fails dark after rejected FLED admission") {
    FakeFilebufPtr file = fl::make_shared<FakeFilebuf>();
    const uint8_t invalidFled[] = {
        'F', 'L', 'E', 'D', 1, 0x00, 1, 0, 0, 0, 0, 0, 1, 2, 3,
    };
    FL_REQUIRE_EQ(file->writeData(invalidFled, sizeof(invalidFled)),
                  sizeof(invalidFled));
    fl::Video video(1, 30, 1);
    FL_CHECK_FALSE(video.begin(file));
    fl::Frame frame(1);
    frame.rgb()[0] = CRGB::Red;
    FL_CHECK_FALSE(video.draw(0, &frame));
    FL_CHECK_EQ(frame.rgb()[0], CRGB::Black);
}

FL_TEST_CASE("PixelStream rejects an unsupported FLED format instead of reading its header as RGB") {
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    // A complete FLED header for rgb16_linear followed by one six-byte
    // sample. PixelStream only renders RGB8 today, so recognizing the magic
    // must reject this container rather than rewinding and exposing "FLE".
    const uint8_t fledRgb16[] = {
        'F', 'L', 'E', 'D', 1, 0xff, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    FL_REQUIRE_EQ(fileHandle->writeData(fledRgb16, sizeof(fledRgb16)),
                  sizeof(fledRgb16));

    fl::PixelStream stream(3);
    FL_CHECK_FALSE(stream.begin(fileHandle));
    CRGB pixel;
    FL_CHECK_FALSE(stream.available());
    FL_CHECK_FALSE(stream.readPixel(&pixel));
}

FL_TEST_CASE("PixelStream FLED recognition only falls back when magic is absent") {
    FL_SUBCASE("raw RGB remains playable") {
        FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
        const uint8_t raw[] = {0x11, 0x22, 0x33};
        FL_REQUIRE_EQ(fileHandle->writeData(raw, sizeof(raw)), sizeof(raw));

        fl::PixelStream stream(3);
        FL_REQUIRE(stream.begin(fileHandle));
        CRGB pixel;
        FL_REQUIRE(stream.readPixel(&pixel));
        FL_CHECK_EQ(pixel, CRGB(0x11, 0x22, 0x33));
    }

    FL_SUBCASE("valid RGB8 FLED skips the header") {
        FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
        const uint8_t fledRgb8[] = {
            'F', 'L', 'E', 'D', 1, 0x00, 0, 0, 0, 0, 0, 0,
            0x11, 0x22, 0x33,
        };
        FL_REQUIRE_EQ(fileHandle->writeData(fledRgb8, sizeof(fledRgb8)),
                      sizeof(fledRgb8));

        fl::PixelStream stream(3);
        FL_REQUIRE(stream.begin(fileHandle));
        CRGB pixel;
        FL_REQUIRE(stream.readPixel(&pixel));
        FL_CHECK_EQ(pixel, CRGB(0x11, 0x22, 0x33));
    }

    FL_SUBCASE("future format version and truncation are rejected") {
        const uint8_t invalidHeaders[][12] = {
            {'F', 'L', 'E', 'D', 1, 0x06, 0, 0, 0, 0, 0, 0},
            {'F', 'L', 'E', 'D', 2, 0x00, 0, 0, 0, 0, 0, 0},
            {'F', 'L', 'E', 'D', 1, 0x00, 0, 0, 1, 0, 0, 0},
        };
        for (fl::size i = 0; i < sizeof(invalidHeaders) / sizeof(invalidHeaders[0]); ++i) {
            FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
            FL_REQUIRE_EQ(fileHandle->writeData(invalidHeaders[i],
                                                sizeof(invalidHeaders[i])),
                          sizeof(invalidHeaders[i]));
            fl::PixelStream stream(3);
            FL_CHECK_FALSE(stream.begin(fileHandle));
        }

        FakeFilebufPtr truncated = fl::make_shared<FakeFilebuf>();
        const uint8_t magicOnly[] = {'F', 'L', 'E', 'D', 1};
        FL_REQUIRE_EQ(truncated->writeData(magicOnly, sizeof(magicOnly)),
                      sizeof(magicOnly));
        fl::PixelStream stream(3);
        FL_CHECK_FALSE(stream.begin(truncated));
    }
}

FL_TEST_CASE("PixelStream rejects FLED magic on a non-seekable input") {
    fl::shared_ptr<NonSeekableFakeFilebuf> fileHandle =
        fl::make_shared<NonSeekableFakeFilebuf>();
    const uint8_t fledRgb16[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    FL_REQUIRE_EQ(fileHandle->writeData(fledRgb16, sizeof(fledRgb16)),
                  sizeof(fledRgb16));

    fl::PixelStream stream(3);
    FL_CHECK_FALSE(stream.begin(fileHandle));
    FL_CHECK_FALSE(stream.available());
}

FL_TEST_CASE("Video carries RGB16 FLED source metadata and darkens rejected playback") {
    FakeFilebufPtr rgb16 = fl::make_shared<FakeFilebuf>();
    const uint8_t rgb16Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    FL_REQUIRE_EQ(rgb16->writeData(rgb16Fled, sizeof(rgb16Fled)),
                  sizeof(rgb16Fled));
    fl::Video video(1, 30, 1);
    FL_REQUIRE(video.begin(rgb16));

    fl::fled::VideoColor color;
    fl::fled::PixelStorage storage;
    FL_REQUIRE(video.videoColor(&color));
    FL_REQUIRE(video.pixelStorage(&storage));
    FL_CHECK_EQ(storage.mFormat, fl::PixelFormat::Rgb16);
    FL_CHECK_EQ(storage.mComponentByteOrder,
                fl::fled::ComponentByteOrder::LittleEndian);
    FL_CHECK_EQ(color.transfer, fl::fled::ColorTransfer::Linear);

    CRGB leds[] = {CRGB::Red};
    FL_CHECK_FALSE(video.draw(0, leds));
    FL_CHECK_EQ(leds[0], CRGB::Black);
}

FL_TEST_CASE("Known non-RGB8 FLED formats never reach RGB8 playback") {
    struct UnsupportedFormat {
        fl::u8 mFormat;
        fl::u8 bytesPerLed;
    };
    const UnsupportedFormat formats[] = {
        {0x01, 1}, // gray8
        {0x02, 4}, // rgba8
        {0x03, 4}, // rgbw8
        {0x04, 2}, // rgb565_le
    };
    const char metadata[] =
        "{\"video\":{\"color\":{\"primaries\":\"bt709\",\"transfer\":\"srgb\","
        "\"matrix\":\"rgb\",\"range\":\"full\"}}}";

    for (const UnsupportedFormat& format : formats) {
        fl::vector<uint8_t> bytes(12 + sizeof(metadata) - 1 + format.bytesPerLed, 0);
        bytes[0] = 'F'; bytes[1] = 'L'; bytes[2] = 'E'; bytes[3] = 'D';
        bytes[4] = 1;
        bytes[5] = format.mFormat;
        bytes[8] = sizeof(metadata) - 1;
        for (fl::size i = 0; i < sizeof(metadata) - 1; ++i) {
            bytes[12 + i] = metadata[i];
        }
        for (fl::size i = 0; i < format.bytesPerLed; ++i) {
            bytes[12 + sizeof(metadata) - 1 + i] = static_cast<uint8_t>(i + 1);
        }

        FakeFilebufPtr streamFile = fl::make_shared<FakeFilebuf>();
        FL_REQUIRE_EQ(streamFile->writeData(bytes.data(), bytes.size()), bytes.size());
        fl::PixelStream stream(3);
        FL_REQUIRE(stream.begin(streamFile));
        CRGB legacy = CRGB::Red;
        FL_CHECK_FALSE(stream.readPixel(&legacy));
        FL_CHECK_EQ(legacy, CRGB::Red);

        FakeFilebufPtr videoFile = fl::make_shared<FakeFilebuf>();
        FL_REQUIRE_EQ(videoFile->writeData(bytes.data(), bytes.size()), bytes.size());
        fl::Video video(1, 30, 1);
        FL_REQUIRE(video.begin(videoFile));
        CRGB leds[] = {CRGB::Red};
        FL_CHECK_FALSE(video.draw(0, leds));
        FL_CHECK_EQ(leds[0], CRGB::Black);
    }
}

FL_TEST_CASE("PixelStream reads RGB16 FLED through a typed sample only") {
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    const uint8_t rgb16Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    FL_REQUIRE_EQ(fileHandle->writeData(rgb16Fled, sizeof(rgb16Fled)),
                  sizeof(rgb16Fled));

    fl::PixelStream stream(3);
    FL_REQUIRE(stream.begin(fileHandle));
    CRGB legacy;
    FL_CHECK_FALSE(stream.readPixel(&legacy));

    fl::video::PixelSample sample;
    FL_REQUIRE(stream.readSample(&sample));
    FL_CHECK_EQ(sample.mStorage.mFormat, fl::PixelFormat::Rgb16);
    FL_CHECK_EQ(sample.mStorage.mComponentByteOrder,
                fl::fled::ComponentByteOrder::LittleEndian);
    FL_CHECK_EQ(sample.mColor.transfer, fl::fled::ColorTransfer::Linear);
    FL_CHECK_EQ(sample.mComponents[0], fl::u16(0x1200));
    FL_CHECK_EQ(sample.mComponents[1], fl::u16(0x1201));
    FL_CHECK_EQ(sample.mComponents[2], fl::u16(0x1202));
}

FL_TEST_CASE("PixelStream resets FLED metadata and stride across reopen") {
    const uint8_t rgb16Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    const uint8_t rgb8Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x00, 0, 0, 0, 0, 0, 0,
        0x12, 0x34, 0x56,
    };
    fl::PixelStream stream(3);
    fl::fled::PixelStorage storage;
    for (int i = 0; i < 2; ++i) {
        FakeFilebufPtr file = fl::make_shared<FakeFilebuf>();
        FL_REQUIRE_EQ(file->writeData(rgb16Fled, sizeof(rgb16Fled)),
                      sizeof(rgb16Fled));
        FL_REQUIRE(stream.begin(file));
        FL_REQUIRE(stream.pixelStorage(&storage));
        FL_CHECK_EQ(storage.mFormat, fl::PixelFormat::Rgb16);
        FL_CHECK_EQ(stream.bytesPerFrame(), 6);
    }
    stream.close();
    FL_CHECK_FALSE(stream.pixelStorage(&storage));

    FakeFilebufPtr rgb8 = fl::make_shared<FakeFilebuf>();
    FL_REQUIRE_EQ(rgb8->writeData(rgb8Fled, sizeof(rgb8Fled)), sizeof(rgb8Fled));
    FL_REQUIRE(stream.begin(rgb8));
    FL_REQUIRE(stream.pixelStorage(&storage));
    FL_CHECK_EQ(storage.mFormat, fl::PixelFormat::Rgb8);
    FL_CHECK_EQ(stream.bytesPerFrame(), 3);

    FakeFilebufPtr raw = fl::make_shared<FakeFilebuf>();
    const uint8_t rawBytes[] = {1, 2, 3};
    FL_REQUIRE_EQ(raw->writeData(rawBytes, sizeof(rawBytes)), sizeof(rawBytes));
    FL_REQUIRE(stream.begin(raw));
    FL_CHECK_FALSE(stream.pixelStorage(&storage));
    FL_CHECK_EQ(stream.bytesPerFrame(), 3);

    fl::PixelStream nullStream(3);
    FL_CHECK_FALSE(nullStream.begin(fl::filebuf_ptr()));

    FakeFilebufPtr truncated = fl::make_shared<FakeFilebuf>();
    const uint8_t truncatedHeader[] = {'F', 'L', 'E', 'D', 1};
    FL_REQUIRE_EQ(truncated->writeData(truncatedHeader, sizeof(truncatedHeader)),
                  sizeof(truncatedHeader));
    FL_CHECK_FALSE(stream.begin(truncated));
    FL_CHECK_FALSE(stream.pixelStorage(&storage));
}

FL_TEST_CASE("Video requires explicit best effort for invalid advisory RGB8 metadata") {
    const char metadata[] =
        "{\"video\":{\"color\":{\"primaries\":\"not-a-space\"}}}";
    FakeFilebufPtr strictFile = fl::make_shared<FakeFilebuf>();
    fl::vector<uint8_t> bytes(12 + sizeof(metadata) - 1 + 3, 0);
    bytes[0] = 'F'; bytes[1] = 'L'; bytes[2] = 'E'; bytes[3] = 'D';
    bytes[4] = 1;
    bytes[8] = sizeof(metadata) - 1;
    for (fl::size i = 0; i < sizeof(metadata) - 1; ++i) {
        bytes[12 + i] = metadata[i];
    }
    bytes[12 + sizeof(metadata) - 1] = 0x12;
    bytes[13 + sizeof(metadata) - 1] = 0x34;
    bytes[14 + sizeof(metadata) - 1] = 0x56;
    FL_REQUIRE_EQ(strictFile->writeData(bytes.data(), bytes.size()), bytes.size());

    fl::Video strict(1, 30, 1);
    FL_CHECK_FALSE(strict.begin(strictFile));
    CRGB leds[] = {CRGB::Red};
    strict.draw(fl::DrawContext(0, leds));
    FL_CHECK_EQ(leds[0], CRGB::Black);

    FakeFilebufPtr bestEffortFile = fl::make_shared<FakeFilebuf>();
    FL_REQUIRE_EQ(bestEffortFile->writeData(bytes.data(), bytes.size()), bytes.size());
    fl::Video bestEffort(1, 30, 1);
    bestEffort.setFade(0, 0);
    bestEffort.setFledPlaybackMode(fl::FledPlaybackMode::BestEffort);
    FL_REQUIRE(bestEffort.begin(bestEffortFile));
    FL_REQUIRE(bestEffort.draw(0, leds));
    FL_CHECK_EQ(leds[0], CRGB(0x12, 0x34, 0x56));
}

FL_TEST_CASE("Video best effort never admits malformed FLED envelopes") {
    const char* const envelopes[] = {"{", "[]"};
    for (fl::size index = 0; index < sizeof(envelopes) / sizeof(envelopes[0]);
         ++index) {
        const char* envelope = envelopes[index];
        const fl::size length = fl::strlen(envelope);
        FakeFilebufPtr file = fl::make_shared<FakeFilebuf>();
        fl::vector<uint8_t> bytes(12 + length + 3, 0);
        bytes[0] = 'F'; bytes[1] = 'L'; bytes[2] = 'E'; bytes[3] = 'D';
        bytes[4] = 1;
        bytes[8] = static_cast<uint8_t>(length);
        for (fl::size i = 0; i < length; ++i) {
            bytes[12 + i] = static_cast<uint8_t>(envelope[i]);
        }
        FL_REQUIRE_EQ(file->writeData(bytes.data(), bytes.size()), bytes.size());
        fl::Video video(1, 30, 1);
        video.setFledPlaybackMode(fl::FledPlaybackMode::BestEffort);
        FL_CHECK_FALSE(video.begin(file));
    }
}

FL_TEST_CASE("Video exposes typed RGB16 samples without CRGB conversion") {
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    const uint8_t rgb16Fled[] = {
        'F', 'L', 'E', 'D', 1, 0x05, 0, 0, 0, 0, 0, 0,
        0x00, 0x12, 0x01, 0x12, 0x02, 0x12,
    };
    FL_REQUIRE_EQ(fileHandle->writeData(rgb16Fled, sizeof(rgb16Fled)),
                  sizeof(rgb16Fled));
    fl::Video video(1, 30, 1);
    FL_REQUIRE(video.begin(fileHandle));
    fl::video::PixelSample sample;
    FL_REQUIRE(video.readSample(&sample));
    FL_CHECK_EQ(sample.mComponents[0], fl::u16(0x1200));
    FL_CHECK_EQ(sample.mComponents[1], fl::u16(0x1201));
}

FL_TEST_CASE("PixelStream defers fragmented stream classification without losing raw bytes") {
    fl::shared_ptr<NonSeekableFakeFilebuf> fled =
        fl::make_shared<NonSeekableFakeFilebuf>();
    const uint8_t partialMagic[] = {'F', 'L', 'E'};
    FL_REQUIRE_EQ(fled->writeData(partialMagic, sizeof(partialMagic)),
                  sizeof(partialMagic));
    fl::PixelStream fledStream(3);
    FL_REQUIRE(fledStream.begin(fled));
    FL_CHECK_FALSE(fledStream.available());

    const uint8_t finalMagic = 'D';
    FL_REQUIRE_EQ(fled->writeData(&finalMagic, 1), 1);
    FL_CHECK_FALSE(fledStream.available());
    CRGB pixel;
    FL_CHECK_FALSE(fledStream.readPixel(&pixel));

    fl::shared_ptr<NonSeekableFakeFilebuf> raw =
        fl::make_shared<NonSeekableFakeFilebuf>();
    const uint8_t rawBytes[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    FL_REQUIRE_EQ(raw->writeData(rawBytes, sizeof(rawBytes)), sizeof(rawBytes));
    fl::PixelStream rawStream(3);
    FL_REQUIRE(rawStream.begin(raw));
    uint8_t replayed[sizeof(rawBytes)] = {};
    FL_REQUIRE_EQ(rawStream.readBytes(replayed, sizeof(replayed)),
                  sizeof(replayed));
    for (fl::size i = 0; i < sizeof(rawBytes); ++i) {
        FL_CHECK_EQ(replayed[i], rawBytes[i]);
    }
}

FL_TEST_CASE("video with memory stream") {
    // fl::Video video(LEDS_PER_FRAME, FPS);
    fl::Video video(LEDS_PER_FRAME, FPS, 1);
    video.setFade(0, 0);
    fl::memorybufPtr memoryStream =
        fl::make_shared<fl::memorybuf>(LEDS_PER_FRAME * 3);
    CRGB testData[LEDS_PER_FRAME] = {};
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = i % 2 == 0 ? CRGB::Red : CRGB::Black;
    }
    size_t pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    FL_REQUIRE_EQ(pixels_written, LEDS_PER_FRAME);
    video.begin(memoryStream);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(FRAME_TIME + 1, leds);
    FL_REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_CHECK_EQ(leds[i], testData[i]);
    }
    ok = video.draw(2 * FRAME_TIME + 1, leds);
    FL_REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        // FL_CHECK_EQ(leds[i], testData[i]);
        FL_REQUIRE_EQ(leds[i].r, testData[i].r);
        FL_REQUIRE_EQ(leds[i].g, testData[i].g);
        FL_REQUIRE_EQ(leds[i].b, testData[i].b);
    }
}

FL_TEST_CASE("video with memory stream, interpolated") {
    // fl::Video video(LEDS_PER_FRAME, FPS);
    fl::Video video(LEDS_PER_FRAME, 1);
    video.setFade(0, 0);
    fl::memorybufPtr memoryStream =
        fl::make_shared<fl::memorybuf>(LEDS_PER_FRAME * sizeof(CRGB) * 2);
    CRGB testData[LEDS_PER_FRAME] = {};
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = CRGB::Red;
    }
    size_t pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    FL_CHECK_EQ(pixels_written, LEDS_PER_FRAME);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = CRGB::Black;
    }
    pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    FL_CHECK_EQ(pixels_written, LEDS_PER_FRAME);
    video.begin(memoryStream); // One frame per second.
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(0, leds); // First frame starts time 0.
    ok = video.draw(500, leds);    // Half a frame.
    FL_CHECK(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        int r = leds[i].r;
        int g = leds[i].g;
        int b = leds[i].b;
        FL_REQUIRE_EQ(128, r); // We expect the color to be interpolated to 128.
        FL_REQUIRE_EQ(0, g);
        FL_REQUIRE_EQ(0, b);
    }
}

FL_TEST_CASE("video with file handle") {
    // fl::Video video(LEDS_PER_FRAME, FPS);
    fl::Video video(LEDS_PER_FRAME, FPS);
    video.setFade(0, 0);
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    CRGB led_frame[LEDS_PER_FRAME];
    // alternate between red and black
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = i % 2 == 0 ? CRGB::Red : CRGB::Black;
    }
    // now write the data
    size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
    FL_CHECK_EQ(leds_written, LEDS_PER_FRAME);
    video.begin(fileHandle);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(FRAME_TIME + 1, leds);
    FL_REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_CHECK_EQ(leds[i], led_frame[i]);
    }
    ok = video.draw(2 * FRAME_TIME + 1, leds);
    FL_CHECK(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_CHECK_EQ(leds[i], led_frame[i]);
    }
}

FL_TEST_CASE("Video duration") {
    fl::Video video(LEDS_PER_FRAME, FPS);
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    CRGB led_frame[LEDS_PER_FRAME];
    // just set all the leds to white

    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = CRGB::White;
    }
    // fill frames for all of one second
    for (uint32_t i = 0; i < FPS; i++) {
        size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
        FL_CHECK_EQ(leds_written, LEDS_PER_FRAME);
    }

    video.begin(fileHandle);
    int32_t duration = video.durationMicros();
    float duration_f = duration / 1000.0;
    FL_CHECK_EQ(1000, uint32_t(duration_f + 0.5));
}

FL_TEST_CASE("video with end frame fadeout") {
    fl::Video video(LEDS_PER_FRAME, FPS);
    video.setFade(0, 1000);
    FakeFilebufPtr fileHandle = fl::make_shared<FakeFilebuf>();
    CRGB led_frame[LEDS_PER_FRAME];
    // just set all the leds to white

    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = CRGB::White;
    }
    // fill frames for all of one second
    for (uint32_t i = 0; i < FPS; i++) {
        size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
        FL_CHECK_EQ(leds_written, LEDS_PER_FRAME);
    }

    video.begin(fileHandle);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(0, leds);
    FL_REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_CHECK_EQ(leds[i], led_frame[i]);
    }
    ok = video.draw(500, leds);
    // test that the leds are about half as bright
    FL_REQUIRE(ok);



    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        // This is what the values should be but we don't do inter-frame
        // interpolation yet. FL_CHECK_EQ(leds[i].r, 127); FL_CHECK_EQ(leds[i].g,
        // 127); FL_CHECK_EQ(leds[i].b, 127);
        FL_CHECK_EQ(leds[i].r, 110);
        FL_CHECK_EQ(leds[i].g, 110);
        FL_CHECK_EQ(leds[i].b, 110);
    }

    ok = video.draw(900, leds); // close to last frame
    FL_REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_CHECK_EQ(leds[i].r, 8);
        FL_CHECK_EQ(leds[i].g, 8);
        FL_CHECK_EQ(leds[i].b, 8);
    }

    ok = video.draw(965, leds); // Last frame
    FL_REQUIRE(ok);
    // test that the leds are almost black
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_REQUIRE_EQ(leds[i], CRGB(0, 0, 0));
    }
    #if 0  // Bug - we do not handle wrapping around
    ok = video.draw(1000, leds); // Bug - we have to let the buffer drain with one frame.
    ok = video.draw(1000, leds); // After last frame we flip around
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        FL_REQUIRE_EQ(leds[i], CRGB(4, 4, 4));
    }
    #endif  //
}

// VideoFxWrapper tests

namespace {

FASTLED_SHARED_PTR(Fake2d);

// Simple Fx2d object which writes a single red pixel to the first LED
// with the red component being the intensity of the frame counter.
class Fake2d : public fl::Fx2d {
  public:
    Fake2d() : Fx2d(XYMap::constructRectangularGrid(1,1)) {}

    void draw(DrawContext context) override {
        CRGB c = mColors[mFrameCounter % mColors.size()];
        context.leds[0] = c;
        mFrameCounter++;
    }

    bool hasFixedFrameRate(float *fps) const override {
        *fps = 1;
        return true;
    }

    fl::string fxName() const override { return "Fake2d"; }
    uint8_t mFrameCounter = 0;
    fl::FixedVector<CRGB, 5> mColors;
};

} // anonymous namespace

FL_TEST_CASE("test_fixed_fps") {
    Fake2dPtr fake = fl::make_shared<Fake2d>();
    fake->mColors.push_back(CRGB(0, 0, 0));
    fake->mColors.push_back(CRGB(255, 0, 0));
    fl::VideoFxWrapper wrapper(fake);
    wrapper.setFade(0, 0);
    CRGB leds[1];
    fl::Fx::DrawContext context(0, leds);
    wrapper.draw(context);
    FL_CHECK_EQ(1, fake->mFrameCounter);
    FL_CHECK_EQ(leds[0], CRGB(0, 0, 0));
    context.now = 500;
    wrapper.draw(context);
    FL_CHECK_EQ(2, fake->mFrameCounter);
    FL_CHECK_EQ(leds[0], CRGB(127, 0, 0));
}

} // FL_TEST_FILE
