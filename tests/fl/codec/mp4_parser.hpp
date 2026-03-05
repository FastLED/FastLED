#include "test.h"
#include "fl/codec/mp4_parser.h"
#include "fl/file_system.h"
#include "platforms/stub/fs_stub.hpp" // ok platform headers

static fl::FileSystem setupCodecFilesystem_mp4() {
    fl::setTestFileSystemRoot("tests");
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    FL_REQUIRE(ok);
    return fs;
}

FL_TEST_CASE("MP4 parser - parse test.mp4") {
    fl::FileSystem fs = setupCodecFilesystem_mp4();

    fl::ifstream handle = fs.openRead("data/codec/test.mp4");
    FL_REQUIRE(handle.is_open());

    fl::size file_size = handle.size();
    FL_CHECK_GT(file_size, 0);

    fl::vector<fl::u8> file_data(file_size);
    fl::size bytes_read = handle.read(file_data.data(), file_size);
    FL_CHECK_EQ(bytes_read, file_size);
    handle.close();

    fl::string error;
    fl::Mp4TrackInfo track = fl::parseMp4(file_data, &error);

    FL_CHECK_MESSAGE(track.isValid, "MP4 parse failed: " << error);

    if (track.isValid) {
        FL_CHECK_EQ(track.width, 2);
        FL_CHECK_EQ(track.height, 2);
        FL_CHECK_GT(track.sps.size(), 0);
        FL_CHECK_GT(track.pps.size(), 0);
        FL_CHECK_EQ(track.profile, 66); // Baseline profile
        FL_CHECK_EQ(track.level, 10);   // Level 1.0

        FL_MESSAGE("MP4 track: " << track.width << "x" << track.height
                  << " profile=" << (int)track.profile
                  << " level=" << (int)track.level
                  << " SPS=" << track.sps.size()
                  << " PPS=" << track.pps.size());
    }

    fs.end();
}

FL_TEST_CASE("MP4 parser - extract NAL units") {
    fl::FileSystem fs = setupCodecFilesystem_mp4();

    fl::ifstream handle = fs.openRead("data/codec/test.mp4");
    FL_REQUIRE(handle.is_open());

    fl::size file_size = handle.size();
    fl::vector<fl::u8> file_data(file_size);
    handle.read(file_data.data(), file_size);
    handle.close();

    fl::string error;
    fl::Mp4TrackInfo track = fl::parseMp4(file_data, &error);
    FL_REQUIRE(track.isValid);

    fl::vector<fl::u8> annexB = fl::extractH264NalUnits(file_data, track, &error);
    FL_CHECK_GT(annexB.size(), 0);

    // Annex B stream should start with start code 0x00000001
    FL_REQUIRE(annexB.size() >= 4);
    FL_CHECK_EQ(annexB[0], 0x00);
    FL_CHECK_EQ(annexB[1], 0x00);
    FL_CHECK_EQ(annexB[2], 0x00);
    FL_CHECK_EQ(annexB[3], 0x01);

    FL_MESSAGE("Annex B NAL stream: " << annexB.size() << " bytes");

    fs.end();
}

FL_TEST_CASE("MP4 parser - error handling") {
    FL_SUBCASE("empty data") {
        fl::vector<fl::u8> empty;
        fl::string error;
        fl::Mp4TrackInfo track = fl::parseMp4(empty, &error);
        FL_CHECK_FALSE(track.isValid);
        FL_CHECK_FALSE(error.empty());
    }

    FL_SUBCASE("too small data") {
        fl::vector<fl::u8> small = {0x00, 0x00, 0x00};
        fl::string error;
        fl::Mp4TrackInfo track = fl::parseMp4(small, &error);
        FL_CHECK_FALSE(track.isValid);
    }

    FL_SUBCASE("invalid data") {
        fl::vector<fl::u8> garbage(100, 0x42);
        fl::string error;
        fl::Mp4TrackInfo track = fl::parseMp4(garbage, &error);
        FL_CHECK_FALSE(track.isValid);
    }

    FL_SUBCASE("extract from invalid track") {
        fl::Mp4TrackInfo invalid;
        fl::vector<fl::u8> data = {0x00};
        fl::string error;
        fl::vector<fl::u8> annexB = fl::extractH264NalUnits(data, invalid, &error);
        FL_CHECK(annexB.empty());
    }
}
