"""Run the production Arduino storage adapter against a strict vendor boundary."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"
COMMON = r"""
#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#define FL_NO_EXCEPT noexcept
#define FL_IS_ESP32
#define FL_HAS_INCLUDE(header) __has_include(header)
namespace fl {
using u8 = std::uint8_t; using size_t = std::size_t;
using string = std::string;
using std::strlen;
using std::make_shared; using std::move;
enum class seek_dir { beg, cur, end };
class filebuf {
public:
    virtual ~filebuf() = default;
    virtual bool is_open() const noexcept = 0;
    virtual bool available() const noexcept = 0;
    virtual size_t size() const noexcept = 0;
    virtual size_t read(char*, size_t) noexcept = 0;
    virtual size_t write(const char*, size_t) noexcept = 0;
    virtual size_t tell() noexcept = 0;
    virtual const char* path() const noexcept = 0;
    virtual bool seek(size_t, seek_dir) noexcept = 0;
    virtual void close() noexcept = 0;
    virtual bool is_eof() const noexcept = 0;
    virtual bool has_error() const noexcept = 0;
    virtual void clear_error() noexcept = 0;
    virtual int error_code() const noexcept = 0;
    virtual const char* error_message() const noexcept = 0;
};
using filebuf_ptr = std::shared_ptr<filebuf>;
class FsImpl {
public:
    virtual ~FsImpl() = default;
    virtual bool begin() noexcept = 0;
    virtual void end() noexcept = 0;
    virtual filebuf_ptr openRead(const char*) noexcept = 0;
};
using FsImplPtr = std::shared_ptr<FsImpl>;
}
class File {
    bool valid = false;
    bool directory = false;
    std::size_t offset = 0;
public:
    File() = default;
    File(bool directory) : valid(true), directory(directory) {}
    explicit operator bool() const { return valid; }
    bool isDirectory() const { return directory; }
    void close() { valid = false; }
    std::size_t size() const { return directory ? 0 : 7; }
    std::size_t position() const { return offset; }
    std::size_t available() const { return valid ? size() - offset : 0; }
    bool seek(std::size_t position) { offset = position; return valid; }
    std::size_t read(unsigned char* out, std::size_t count) {
        if (!valid) return 0;
        const auto available = size() - offset;
        if (count > available) count = available;
        std::memcpy(out, "payload" + offset, count); offset += count; return count;
    }
};
struct StrictLittleFs {
    unsigned fileOpens = 0;
    File open(const char* name, const char*) {
        if (!std::strcmp(name, "/")) return File(true);
        if (!std::strcmp(name, "/file.bin")) { ++fileOpens; return File(false); }
        return File();
    }
};
static StrictLittleFs LittleFS;
"""


@pytest.mark.parametrize("path", ["file.bin", "/file.bin"])
def test_embedded_fs_arduino_accepts_relative_and_absolute_paths(
    tmp_path: Path, path: str
) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("C++ compiler required for Arduino adapter path probe")
    (tmp_path / "common.h").write_text(COMMON)
    for name in (
        "fl/stl/has_include.h",
        "fl/stl/cstring.h",
        "fl/fs/fs.h",
        "fl/fs/file_handle.h",
        "fl/stl/memory.h",
        "fl/stl/noexcept.h",
        "fl/stl/string.h",
        "FS.h",
        "LittleFS.h",
    ):
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text('#include "common.h"\n')
    fixture = tmp_path / "probe.cpp"
    fixture.write_text(
        '#include "platforms/arduino/fs/embedded_fs_arduino.hpp"\n'
        "int main() {\n"
        " auto fs = fl::platforms::makeArduinoEmbeddedFs(false);\n"
        " if (!fs || !fs->begin()) return 1;\n"
        f' auto file = fs->openRead("{path}");\n'
        " if (!file || !file->is_open() || file->size() != 7) return 2;\n"
        f' if (std::strcmp(file->path(), "{path}")) return 3;\n'
        " char bytes[8] = {};\n"
        ' if (file->read(bytes, 7) != 7 || std::strcmp(bytes, "payload")) return 4;\n'
        " if (LittleFS.fileOpens != 1) return 5;\n"
        ' if (fs->openRead("") || fs->openRead(nullptr)) return 6;\n'
        " return 0;\n}\n"
    )
    executable = tmp_path / "probe"
    build = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            "-O0",
            "-g",
            f"-I{tmp_path}",
            f"-I{SRC}",
            str(fixture),
            "-o",
            str(executable),
        ],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
    )
    assert build.returncode == 0, build.stdout
    result = RunningProcess.run(
        [str(executable)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
