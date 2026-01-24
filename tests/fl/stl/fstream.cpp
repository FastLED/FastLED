#include "test.h"
#include "fl/stl/fstream.h"
#include "fl/stl/cerrno.h"
#include "platforms/stub/fs_stub.hpp"
#include "doctest.h"

TEST_CASE("fstream errno - file not found") {
    fl::ifstream ifs("/nonexistent/file.txt");
    CHECK(!ifs.is_open());
    CHECK(ifs.fail());
    CHECK(ifs.error() == ENOENT);
    CHECK(fl::string(ifs.error_message()).find("No such file") != fl::string::npos);
}

TEST_CASE("fstream errno - write after close") {
    fl::string test_dir = "test_fstream_errors";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    fl::ofstream ofs(file_path.c_str());
    REQUIRE(ofs.is_open());

    ofs.close();
    ofs.write("data", 4);  // Write after close

    CHECK(ofs.fail());
    CHECK(ofs.error() != 0);

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

TEST_CASE("fstream errno - clear_error recovery") {
    fl::string test_dir = "test_fstream_clear";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "data");

    fl::ifstream ifs(file_path.c_str());
    REQUIRE(ifs.is_open());

    // Read to EOF
    char buf[10];
    ifs.read(buf, 10);
    CHECK(ifs.eof());

    // Clear and retry
    ifs.clear_error();
    CHECK(ifs.good());

    ifs.seekg(0);
    CHECK(ifs.good());

    ifs.close();
    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

TEST_CASE("fstream errno - error persistence") {
    fl::ifstream ifs("/nonexistent.txt");
    CHECK(!ifs.is_open());

    int err1 = ifs.error();
    int err2 = ifs.error();

    CHECK(err1 == err2);  // Error persists
    CHECK(err1 == ENOENT);
}

TEST_CASE("fstream errno - successful operations clear error") {
    fl::string test_dir = "test_fstream_success";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "test data");

    fl::ifstream ifs(file_path.c_str());
    REQUIRE(ifs.is_open());
    CHECK(ifs.error() == 0);  // No error on successful open
    CHECK(fl::string(ifs.error_message()) == "No error");

    char buf[10];
    ifs.read(buf, 9);
    CHECK(ifs.good());
    CHECK(ifs.error() == 0);  // No error on successful read

    ifs.close();
    CHECK(ifs.error() == 0);  // No error on successful close

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

TEST_CASE("fstream errno - write error detection") {
    fl::string test_dir = "test_fstream_write";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    fl::ofstream ofs(file_path.c_str());
    REQUIRE(ofs.is_open());
    CHECK(ofs.error() == 0);  // No error on successful open

    ofs.write("test data", 9);
    CHECK(ofs.good());
    CHECK(ofs.error() == 0);  // No error on successful write

    ofs.close();
    CHECK(ofs.error() == 0);  // No error on successful close

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

TEST_CASE("fstream errno - tellg and seekg error handling") {
    fl::string test_dir = "test_fstream_seek";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "0123456789");

    fl::ifstream ifs(file_path.c_str());
    REQUIRE(ifs.is_open());

    // Successful seek
    ifs.seekg(5);
    CHECK(ifs.good());
    CHECK(ifs.error() == 0);

    // Successful tell
    fl::size_t pos = ifs.tellg();
    CHECK(pos == 5);
    CHECK(ifs.error() == 0);

    ifs.close();

    // tellg on closed stream
    pos = ifs.tellg();
    CHECK(ifs.error() == EBADF);  // Bad file descriptor

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

TEST_CASE("fstream errno - fstream read/write") {
    fl::string test_dir = "test_fstream_rw";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    // Create file with fstream
    fl::fstream fs(file_path.c_str(), fl::ios::out | fl::ios::trunc | fl::ios::binary);
    REQUIRE(fs.is_open());
    CHECK(fs.error() == 0);

    fs.write("hello", 5);
    CHECK(fs.good());
    CHECK(fs.error() == 0);

    fs.close();
    CHECK(fs.error() == 0);

    // Read back with fstream
    fs.open(file_path.c_str(), fl::ios::in | fl::ios::binary);
    REQUIRE(fs.is_open());
    CHECK(fs.error() == 0);

    char buf[10];
    fs.read(buf, 5);
    CHECK(fs.gcount() == 5);
    CHECK(fs.good());
    CHECK(fs.error() == 0);
    CHECK(fl::strncmp(buf, "hello", 5) == 0);

    fs.close();
    CHECK(fs.error() == 0);

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}
