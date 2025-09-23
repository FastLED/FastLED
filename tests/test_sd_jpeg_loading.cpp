#include "test.h"
#include "fl/file_system.h"
#include "fl/codec/jpeg.h"

// Test SD card JPEG loading functionality

TEST_CASE("FileSystem::loadJpeg API") {
    // Test that the API exists and compiles
    SECTION("Method signature") {
        fl::FileSystem fs;
        fl::JpegDecoderConfig config;
        fl::string error_msg;

        // This should compile even if it fails at runtime due to no SD card
        fl::FramePtr frame = fs.loadJpeg("/test.jpg", config, &error_msg);

        // Without SD card, this will return null frame
        CHECK(!frame);
        CHECK(!error_msg.empty());
    }

    SECTION("Helper function signature") {
        fl::JpegDecoderConfig config;
        fl::string error_msg;

        // This should compile even if it fails at runtime due to no SD card
        fl::FramePtr frame = fl::loadImageFromSD(10, "/test.jpg", config, &error_msg);

        // Without SD card, this will return null frame
        CHECK(!frame);
        CHECK(!error_msg.empty());
    }
}

TEST_CASE("JpegDecoderConfig") {
    SECTION("Default configuration") {
        fl::JpegDecoderConfig config;

        CHECK(config.quality == fl::JpegDecoderConfig::Medium);
        CHECK(config.format == fl::PixelFormat::RGB888);
        CHECK(config.useHardwareAcceleration == true);
        CHECK(config.maxWidth == 1920);
        CHECK(config.maxHeight == 1080);
    }

    SECTION("Custom configuration") {
        fl::JpegDecoderConfig config(fl::JpegDecoderConfig::High, fl::PixelFormat::RGB565);

        CHECK(config.quality == fl::JpegDecoderConfig::High);
        CHECK(config.format == fl::PixelFormat::RGB565);
    }
}

TEST_CASE("SD Card JPEG Integration") {
    SECTION("JPEG support check") {
        // JPEG should always be supported since it uses TJpg_Decoder
        CHECK(fl::Jpeg::isSupported());
    }

    SECTION("Error handling for non-existent file") {
        fl::FileSystem fs;

        // Test with null filesystem (no SD card)
        fl::JpegDecoderConfig config;
        fl::string error_msg;

        fl::FramePtr frame = fs.loadJpeg("/nonexistent.jpg", config, &error_msg);

        CHECK(!frame);
        CHECK(!error_msg.empty());
        CHECK(error_msg.find("Failed to open file") != fl::string::npos);
    }
}

// Mock test for file loading workflow
TEST_CASE("JPEG Loading Workflow") {
    SECTION("Configuration options") {
        fl::JpegDecoderConfig lowQuality(fl::JpegDecoderConfig::Low, fl::PixelFormat::RGB565);
        fl::JpegDecoderConfig mediumQuality(fl::JpegDecoderConfig::Medium, fl::PixelFormat::RGB888);
        fl::JpegDecoderConfig highQuality(fl::JpegDecoderConfig::High, fl::PixelFormat::RGBA8888);

        CHECK(lowQuality.quality == fl::JpegDecoderConfig::Low);
        CHECK(lowQuality.format == fl::PixelFormat::RGB565);

        CHECK(mediumQuality.quality == fl::JpegDecoderConfig::Medium);
        CHECK(mediumQuality.format == fl::PixelFormat::RGB888);

        CHECK(highQuality.quality == fl::JpegDecoderConfig::High);
        CHECK(highQuality.format == fl::PixelFormat::RGBA8888);
    }

    SECTION("Pixel format bytes per pixel") {
        CHECK(fl::getBytesPerPixel(fl::PixelFormat::RGB565) == 2);
        CHECK(fl::getBytesPerPixel(fl::PixelFormat::RGB888) == 3);
        CHECK(fl::getBytesPerPixel(fl::PixelFormat::RGBA8888) == 4);
        CHECK(fl::getBytesPerPixel(fl::PixelFormat::YUV420) == 1);
    }
}