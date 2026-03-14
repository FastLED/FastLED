#include "test.h"
#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/system/file_system.h"
#include "platforms/stub/fs_stub.hpp" // ok platform headers

static fl::FileSystem setupCodecFilesystem_h264() {
    fl::setTestFileSystemRoot("tests");
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    FL_REQUIRE(ok);
    return fs;
}

FL_TEST_CASE("H264 parseH264Info from MP4") {
    fl::FileSystem fs = setupCodecFilesystem_h264();

    fl::ifstream handle = fs.openRead("data/codec/test.mp4");
    FL_REQUIRE(handle.is_open());

    fl::size file_size = handle.size();
    fl::vector<fl::u8> file_data(file_size);
    handle.read(file_data.data(), file_size);
    handle.close();

    fl::string error;
    fl::H264Info info = fl::H264::parseH264Info(file_data, &error);

    FL_CHECK_MESSAGE(info.isValid, "H264 info parse failed: " << error);

    if (info.isValid) {
        FL_CHECK_EQ(info.width, 2);
        FL_CHECK_EQ(info.height, 2);
        FL_CHECK_EQ(info.profile, 66); // Constrained Baseline
        FL_CHECK_EQ(info.level, 10);   // Level 1.0

        FL_MESSAGE("H264 info: " << info.width << "x" << info.height
                  << " profile=" << (int)info.profile
                  << " level=" << (int)info.level
                  << " refFrames=" << (int)info.numRefFrames);
    }

    fs.end();
}

FL_TEST_CASE("H264 SPS parsing") {
    fl::FileSystem fs = setupCodecFilesystem_h264();

    // Extract SPS from MP4 and parse it directly
    fl::ifstream handle = fs.openRead("data/codec/test.mp4");
    FL_REQUIRE(handle.is_open());

    fl::size file_size = handle.size();
    fl::vector<fl::u8> file_data(file_size);
    handle.read(file_data.data(), file_size);
    handle.close();

    fl::string error;
    fl::Mp4TrackInfo track = fl::parseMp4(file_data, &error);
    FL_REQUIRE(track.isValid);
    FL_REQUIRE(track.sps.size() > 0);

    // Parse SPS directly
    fl::H264Info spsInfo = fl::H264::parseSPS(track.sps[0], &error);
    FL_CHECK_MESSAGE(spsInfo.isValid, "SPS parse failed: " << error);

    if (spsInfo.isValid) {
        FL_CHECK_EQ(spsInfo.width, 2);
        FL_CHECK_EQ(spsInfo.height, 2);
        FL_CHECK_EQ(spsInfo.profile, 66);
    }

    fs.end();
}

FL_TEST_CASE("H264 decoder factory") {
    FL_SUBCASE("isSupported") {
        // On host, H.264 HW decode is not supported
        FL_CHECK_FALSE(fl::H264::isSupported());
    }

    FL_SUBCASE("createDecoder returns NullDecoder on host") {
        fl::string error;
        auto decoder = fl::H264::createDecoder(&error);
        FL_CHECK(decoder != nullptr);
        // NullDecoder should report error
        FL_CHECK(decoder->hasError());
    }
}

FL_TEST_CASE("H264 error handling") {
    FL_SUBCASE("empty MP4 data") {
        fl::vector<fl::u8> empty;
        fl::string error;
        fl::H264Info info = fl::H264::parseH264Info(empty, &error);
        FL_CHECK_FALSE(info.isValid);
    }

    FL_SUBCASE("invalid MP4 data") {
        fl::vector<fl::u8> garbage(64, 0xFF);
        fl::string error;
        fl::H264Info info = fl::H264::parseH264Info(garbage, &error);
        FL_CHECK_FALSE(info.isValid);
    }

    FL_SUBCASE("SPS too small") {
        fl::vector<fl::u8> tiny = {0x67, 0x42}; // SPS NAL type but too short
        fl::string error;
        fl::H264Info info = fl::H264::parseSPS(tiny, &error);
        FL_CHECK_FALSE(info.isValid);
    }
}
