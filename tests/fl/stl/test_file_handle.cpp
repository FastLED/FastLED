#include "test.h"
#include "fl/stl/detail/file_handle.h"
#include "fl/stl/cerrno.h"
#include "platforms/stub/fs_stub.hpp"

// RAII guard for test files
class FilebufTestGuard {
    fl::string mDir;
public:
    explicit FilebufTestGuard(const char* dir) : mDir(dir) {
        fl::StubFileSystem::forceRemoveDirectory(mDir);
        fl::StubFileSystem::createDirectory(mDir);
    }
    ~FilebufTestGuard() {
        fl::StubFileSystem::forceRemoveDirectory(mDir);
    }
    fl::string filePath(const char* name) const {
        fl::string p = mDir;
        p.append("/");
        p.append(name);
        return p;
    }
};

FL_TEST_CASE("posix_filebuf::size() returns correct file size") {
    FilebufTestGuard guard("test_fh_size");
    fl::string path = guard.filePath("test.bin");
    fl::StubFileSystem::createTextFile(path, "0123456789");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());
    FL_CHECK_EQ(fh.size(), 10);
}

FL_TEST_CASE("posix_filebuf::path() returns path passed to constructor") {
    FilebufTestGuard guard("test_fh_path");
    fl::string path = guard.filePath("hello.txt");
    fl::StubFileSystem::createTextFile(path, "hi");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());
    FL_CHECK_EQ(fl::string(fh.path()), path);
}

FL_TEST_CASE("filebuf::available() default implementation") {
    FilebufTestGuard guard("test_fh_avail");
    fl::string path = guard.filePath("data.txt");
    fl::StubFileSystem::createTextFile(path, "abcd");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());
    FL_CHECK(fh.available());

    // Read all data to trigger EOF
    char buf[10];
    fh.read(buf, 10);
    FL_CHECK(!fh.available()); // EOF reached
}

FL_TEST_CASE("filebuf::bytes_left() tracks correctly") {
    FilebufTestGuard guard("test_fh_bytes_left");
    fl::string path = guard.filePath("data.txt");
    fl::StubFileSystem::createTextFile(path, "0123456789");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());
    FL_CHECK_EQ(fh.bytes_left(), 10);

    char buf[4];
    fh.read(buf, 4);
    FL_CHECK_EQ(fh.bytes_left(), 6);

    fh.read(buf, 4);
    FL_CHECK_EQ(fh.bytes_left(), 2);

    fh.read(buf, 4); // Only 2 left
    FL_CHECK_EQ(fh.bytes_left(), 0);
}

FL_TEST_CASE("posix_filebuf::read(u8*, n) overload") {
    FilebufTestGuard guard("test_fh_u8_read");
    fl::string path = guard.filePath("data.bin");
    fl::StubFileSystem::createTextFile(path, "ABCD");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());

    fl::u8 buf[4];
    fl::size_t n = fh.read(buf, 4);
    FL_CHECK_EQ(n, 4);
    FL_CHECK_EQ(buf[0], 'A');
    FL_CHECK_EQ(buf[1], 'B');
    FL_CHECK_EQ(buf[2], 'C');
    FL_CHECK_EQ(buf[3], 'D');
}

FL_TEST_CASE("filebuf::readRGB8() reads CRGB values") {
    FilebufTestGuard guard("test_fh_rgb8");
    fl::string path = guard.filePath("pixels.bin");
    // 2 pixels: RGB(10,20,30) and RGB(40,50,60)
    char pixel_data[] = {10, 20, 30, 40, 50, 60};
    // Write raw binary data
    {
        fl::detail::posix_filebuf wh(path.c_str(), "wb");
        FL_REQUIRE(wh.is_open());
        wh.write(pixel_data, 6);
    }

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());

    CRGB pixels[2];
    fl::size_t count = fh.readRGB8(fl::span<CRGB>(pixels, 2));
    FL_CHECK_EQ(count, 2);
    FL_CHECK_EQ(pixels[0].r, 10);
    FL_CHECK_EQ(pixels[0].g, 20);
    FL_CHECK_EQ(pixels[0].b, 30);
    FL_CHECK_EQ(pixels[1].r, 40);
    FL_CHECK_EQ(pixels[1].g, 50);
    FL_CHECK_EQ(pixels[1].b, 60);
}

FL_TEST_CASE("posix_filebuf error state") {
    // File not found
    fl::detail::posix_filebuf fh("/nonexistent/path/file.txt", "rb");
    FL_CHECK(!fh.is_open());
    FL_CHECK(fh.has_error());
    FL_CHECK(fh.error_code() == ENOENT);
    FL_CHECK(fl::string(fh.error_message()).find("No such file") != fl::string::npos);

    // Clear error
    fh.clear_error();
    FL_CHECK(!fh.has_error());
    FL_CHECK(fh.error_code() == 0);
}

FL_TEST_CASE("posix_filebuf seek directions") {
    FilebufTestGuard guard("test_fh_seek");
    fl::string path = guard.filePath("seek.txt");
    fl::StubFileSystem::createTextFile(path, "0123456789");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());

    // Seek from beginning
    FL_CHECK(fh.seek(3, fl::seek_dir::beg));
    FL_CHECK_EQ(fh.tell(), 3);

    // Seek from current position
    FL_CHECK(fh.seek(2, fl::seek_dir::cur));
    FL_CHECK_EQ(fh.tell(), 5);

    // Seek from end
    FL_CHECK(fh.seek(0, fl::seek_dir::end));
    FL_CHECK_EQ(fh.tell(), 10);

    // Read at various positions
    fh.seek(0, fl::seek_dir::beg);
    char c;
    fh.read(&c, 1);
    FL_CHECK_EQ(c, '0');

    fh.seek(9, fl::seek_dir::beg);
    fh.read(&c, 1);
    FL_CHECK_EQ(c, '9');
}

FL_TEST_CASE("posix_filebuf size() does not change position") {
    FilebufTestGuard guard("test_fh_size_pos");
    fl::string path = guard.filePath("data.txt");
    fl::StubFileSystem::createTextFile(path, "0123456789");

    fl::detail::posix_filebuf fh(path.c_str(), "rb");
    FL_REQUIRE(fh.is_open());

    // Seek to middle
    fh.seek(5, fl::seek_dir::beg);
    FL_CHECK_EQ(fh.tell(), 5);

    // size() should not change position
    FL_CHECK_EQ(fh.size(), 10);
    FL_CHECK_EQ(fh.tell(), 5);
}

FL_TEST_CASE("posix_filebuf path() for default constructed") {
    fl::detail::posix_filebuf fh;
    FL_CHECK_EQ(fl::string(fh.path()), fl::string(""));
}
