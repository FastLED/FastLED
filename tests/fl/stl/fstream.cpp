#include "test.h"
#include "fl/stl/fstream.h"
#include "fl/stl/cerrno.h"
#include "platforms/stub/fs_stub.hpp"
#include "test.h"

FL_TEST_CASE("fstream errno - file not found") {
    fl::ifstream ifs("/nonexistent/file.txt");
    FL_CHECK(!ifs.is_open());
    FL_CHECK(ifs.fail());
    FL_CHECK(ifs.error() == ENOENT);
    FL_CHECK(fl::string(ifs.error_message()).find("No such file") != fl::string::npos);
}

FL_TEST_CASE("fstream errno - write after close") {
    fl::string test_dir = "test_fstream_errors";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    fl::ofstream ofs(file_path.c_str());
    FL_REQUIRE(ofs.is_open());

    ofs.close();
    ofs.write("data", 4);  // Write after close

    FL_CHECK(ofs.fail());
    FL_CHECK(ofs.error() != 0);

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

FL_TEST_CASE("fstream errno - clear_error recovery") {
    fl::string test_dir = "test_fstream_clear";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "data");

    fl::ifstream ifs(file_path.c_str());
    FL_REQUIRE(ifs.is_open());

    // Read to EOF
    char buf[10];
    ifs.read(buf, 10);
    FL_CHECK(ifs.eof());

    // Clear and retry
    ifs.clear_error();
    FL_CHECK(ifs.good());

    ifs.seekg(0);
    FL_CHECK(ifs.good());

    ifs.close();
    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

FL_TEST_CASE("fstream errno - error persistence") {
    fl::ifstream ifs("/nonexistent.txt");
    FL_CHECK(!ifs.is_open());

    int err1 = ifs.error();
    int err2 = ifs.error();

    FL_CHECK(err1 == err2);  // Error persists
    FL_CHECK(err1 == ENOENT);
}

FL_TEST_CASE("fstream errno - successful operations clear error") {
    fl::string test_dir = "test_fstream_success";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "test data");

    fl::ifstream ifs(file_path.c_str());
    FL_REQUIRE(ifs.is_open());
    FL_CHECK(ifs.error() == 0);  // No error on successful open
    FL_CHECK(fl::string(ifs.error_message()) == "No error");

    char buf[10];
    ifs.read(buf, 9);
    FL_CHECK(ifs.good());
    FL_CHECK(ifs.error() == 0);  // No error on successful read

    ifs.close();
    FL_CHECK(ifs.error() == 0);  // No error on successful close

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

FL_TEST_CASE("fstream errno - write error detection") {
    fl::string test_dir = "test_fstream_write";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    fl::ofstream ofs(file_path.c_str());
    FL_REQUIRE(ofs.is_open());
    FL_CHECK(ofs.error() == 0);  // No error on successful open

    ofs.write("test data", 9);
    FL_CHECK(ofs.good());
    FL_CHECK(ofs.error() == 0);  // No error on successful write

    ofs.close();
    FL_CHECK(ofs.error() == 0);  // No error on successful close

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

FL_TEST_CASE("fstream errno - tellg and seekg error handling") {
    fl::string test_dir = "test_fstream_seek";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";
    fl::StubFileSystem::createTextFile(file_path, "0123456789");

    fl::ifstream ifs(file_path.c_str());
    FL_REQUIRE(ifs.is_open());

    // Successful seek
    ifs.seekg(5);
    FL_CHECK(ifs.good());
    FL_CHECK(ifs.error() == 0);

    // Successful tell
    fl::size_t pos = ifs.tellg();
    FL_CHECK(pos == 5);
    FL_CHECK(ifs.error() == 0);

    ifs.close();

    // tellg on closed stream
    pos = ifs.tellg();
    FL_CHECK(ifs.error() == EBADF);  // Bad file descriptor

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}

FL_TEST_CASE("fstream errno - fstream read/write") {
    fl::string test_dir = "test_fstream_rw";
    fl::StubFileSystem::createDirectory(test_dir);
    fl::string file_path = test_dir + "/test.txt";

    // Create file with fstream
    fl::fstream fs(file_path.c_str(), fl::ios::out | fl::ios::trunc | fl::ios::binary);
    FL_REQUIRE(fs.is_open());
    FL_CHECK(fs.error() == 0);

    fs.write("hello", 5);
    FL_CHECK(fs.good());
    FL_CHECK(fs.error() == 0);

    fs.close();
    FL_CHECK(fs.error() == 0);

    // Read back with fstream
    fs.open(file_path.c_str(), fl::ios::in | fl::ios::binary);
    FL_REQUIRE(fs.is_open());
    FL_CHECK(fs.error() == 0);

    char buf[10];
    fs.read(buf, 5);
    FL_CHECK(fs.gcount() == 5);
    FL_CHECK(fs.good());
    FL_CHECK(fs.error() == 0);
    FL_CHECK(fl::strncmp(buf, "hello", 5) == 0);

    fs.close();
    FL_CHECK(fs.error() == 0);

    fl::StubFileSystem::removeFile(file_path);
    fl::StubFileSystem::removeDirectory(test_dir);
}
