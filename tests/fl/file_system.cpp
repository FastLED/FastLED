// Test for the filesystem implementation
//
// This test demonstrates how to use the test filesystem that maps SD card
// operations to real hard drive paths in the testing environment.
//
// Usage in tests:
// 1. Use StubFileSystem static methods to create test files and directories:
//    - StubFileSystem::createDirectory("test_dir")
//    - StubFileSystem::createTextFile("test_dir/test.txt", "content")
// 2. Call setTestFileSystemRoot("test_dir") to set the root directory
// 3. Create a FileSystem instance and call beginSd()
// 4. Use normal FileSystem methods (openRead, readText, etc.)
// 5. Clean up using StubFileSystem static methods:
//    - StubFileSystem::removeFile("test_dir/test.txt")
//    - StubFileSystem::removeDirectory("test_dir")
//
// This allows testing SD card functionality without requiring actual SD hardware
// and keeps all file operations centralized in the stub platform implementation.

#include "test.h"

#include "fl/file_system.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#include "fl/stl/stdint.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/string.h"
#include "fl/stl/new.h"
#include "fl/stl/vector.h"
#include "doctest.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/int.h"
#endif


TEST_CASE("FileSystem test with real hard drive") {
    // Create a temporary test directory and file
    fl::string test_dir = "test_filesystem_temp";
    fl::string test_file = "test_data.txt";
    fl::string test_content = "Hello, FastLED filesystem test!";

    // Create test directory using stub filesystem utilities
    REQUIRE(fl::StubFileSystem::createDirectory(test_dir.c_str()));

    // Create test file
    fl::string full_path = test_dir;
    full_path.append("/");
    full_path.append(test_file);
    REQUIRE(fl::StubFileSystem::createTextFile(full_path.c_str(), test_content.c_str()));

    // Set the test filesystem root
    fl::setTestFileSystemRoot(test_dir.c_str());

    // Verify the root was set
    CHECK_EQ(fl::string(fl::getTestFileSystemRoot()), test_dir);

    // Create filesystem and test reading
    fl::FileSystem fs;
    bool ok = fs.beginSd(5); // CS pin doesn't matter for test implementation
    REQUIRE(ok);

    // Try to read the test file
    fl::FileHandlePtr handle = fs.openRead(test_file.c_str());
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // Check file size
    CHECK_EQ(handle->size(), test_content.length());

    // Read the content
    fl::vector<uint8_t> buffer(test_content.length());
    fl::size bytes_read = handle->read(buffer.data(), buffer.size());
    CHECK_EQ(bytes_read, test_content.length());

    // Verify content
    fl::string read_content;
    read_content.assign((const char*)buffer.data(), buffer.size());
    CHECK_EQ(read_content, test_content);

    // Test seeking
    REQUIRE(handle->seek(7)); // Seek to position 7 ("FastLED...")
    fl::vector<uint8_t> seek_buffer(7);
    fl::size seek_bytes = handle->read(seek_buffer.data(), seek_buffer.size());
    CHECK_EQ(seek_bytes, 7);
    fl::string seek_content;
    seek_content.assign((const char*)seek_buffer.data(), seek_buffer.size());
    CHECK_EQ(seek_content, "FastLED");

    // Clean up
    handle->close();
    fs.end();

    // Remove test files using stub filesystem utilities
    fl::StubFileSystem::removeFile(full_path.c_str());
    fl::StubFileSystem::removeDirectory(test_dir.c_str());
}

TEST_CASE("FileSystem test with subdirectories") {
    // Create a nested directory structure
    fl::string test_dir = "test_fs_nested";
    fl::string sub_dir = "data";
    fl::string test_file = "video.rgb";
    fl::string test_content = "RGB video data here";

    // Create directories using stub filesystem utilities
    REQUIRE(fl::StubFileSystem::createDirectory(test_dir.c_str()));
    fl::string sub_dir_path = test_dir;
    sub_dir_path.append("/");
    sub_dir_path.append(sub_dir);
    REQUIRE(fl::StubFileSystem::createDirectory(sub_dir_path.c_str()));

    // Create test file in subdirectory
    fl::string full_path = sub_dir_path;
    full_path.append("/");
    full_path.append(test_file);
    REQUIRE(fl::StubFileSystem::createTextFile(full_path.c_str(), test_content.c_str()));

    // Set the test filesystem root
    fl::setTestFileSystemRoot(test_dir.c_str());

    // Create filesystem and test reading from subdirectory
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);

    // Try to read the test file using forward slash path
    fl::string file_path = sub_dir;
    file_path.append("/");
    file_path.append(test_file);
    fl::FileHandlePtr handle = fs.openRead(file_path.c_str());
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // Read and verify content
    fl::vector<uint8_t> buffer(test_content.length());
    fl::size bytes_read = handle->read(buffer.data(), buffer.size());
    CHECK_EQ(bytes_read, test_content.length());

    fl::string read_content;
    read_content.assign((const char*)buffer.data(), buffer.size());
    CHECK_EQ(read_content, test_content);

    // Clean up
    handle->close();
    fs.end();

    // Remove test files and directories using stub filesystem utilities
    fl::StubFileSystem::removeFile(full_path.c_str());
    fl::StubFileSystem::removeDirectory(sub_dir_path.c_str());
    fl::StubFileSystem::removeDirectory(test_dir.c_str());
}

TEST_CASE("FileSystem test with text file reading") {
    // Test the readText functionality
    fl::string test_dir = "test_fs_text";
    fl::string test_file = "config.json";
    fl::string test_content = R"({
    "led_count": 100,
    "fps": 30,
    "brightness": 255
})";

    // Create test directory and file using stub filesystem utilities
    REQUIRE(fl::StubFileSystem::createDirectory(test_dir.c_str()));
    fl::string full_path = test_dir;
    full_path.append("/");
    full_path.append(test_file);
    REQUIRE(fl::StubFileSystem::createTextFile(full_path.c_str(), test_content.c_str()));

    // Set the test filesystem root
    fl::setTestFileSystemRoot(test_dir.c_str());

    // Create filesystem and test text reading
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);

    // Read text file
    fl::string content;
    bool read_ok = fs.readText(test_file.c_str(), &content);
    REQUIRE(read_ok);

    // Normalize line endings (remove \r characters) for cross-platform compatibility
    fl::string content_str;
    for (fl::size i = 0; i < content.size(); ++i) {
        if (content[i] != '\r') {
            content_str += content[i];
        }
    }

    CHECK_EQ(content_str, test_content);

    // Clean up
    fs.end();
    fl::StubFileSystem::removeFile(full_path.c_str());
    fl::StubFileSystem::removeDirectory(test_dir.c_str());
}

TEST_CASE("FileSystem test with binary file loading") {
    // Test loading a binary JPEG file to verify byte-accurate reading

    // Set the test filesystem root to the tests directory
    fl::setTestFileSystemRoot("tests");

    // Create filesystem and test reading binary file
    fl::FileSystem fs;
    bool ok = fs.beginSd(5);
    REQUIRE(ok);

    // Try to read the JPEG test file
    fl::FileHandlePtr handle = fs.openRead("data/codec/file.jpg");
    REQUIRE(handle != nullptr);
    REQUIRE(handle->valid());

    // JPEG files should start with FF D8 (JPEG SOI marker)
    uint8_t jpeg_header[2];
    fl::size bytes_read = handle->read(jpeg_header, 2);
    CHECK_EQ(bytes_read, 2);
    CHECK_EQ(jpeg_header[0], 0xFF);
    CHECK_EQ(jpeg_header[1], 0xD8);

    // Check that we can get the file size
    fl::size file_size = handle->size();
    CHECK(file_size > 0);

    // Test seeking to the end to check for JPEG EOI marker (FF D9)
    REQUIRE(handle->seek(file_size - 2));
    uint8_t jpeg_footer[2];
    bytes_read = handle->read(jpeg_footer, 2);
    CHECK_EQ(bytes_read, 2);
    CHECK_EQ(jpeg_footer[0], 0xFF);
    CHECK_EQ(jpeg_footer[1], 0xD9);

    // Test reading the entire file into a buffer
    REQUIRE(handle->seek(0));
    fl::vector<uint8_t> file_buffer(file_size);
    bytes_read = handle->read(file_buffer.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    // Verify JPEG header and footer in the buffer
    CHECK_EQ(file_buffer[0], 0xFF);
    CHECK_EQ(file_buffer[1], 0xD8);
    CHECK_EQ(file_buffer[file_size - 2], 0xFF);
    CHECK_EQ(file_buffer[file_size - 1], 0xD9);

    // Clean up
    handle->close();
    fs.end();
}
