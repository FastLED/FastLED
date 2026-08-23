// Tests for the LittleFS filesystem backend.
//
// LittleFS is an on-chip flash filesystem and only exists where the
// platform core ships <LittleFS.h> — ESP32, ESP8266, RP2040. The host
// test build has none of that, so what is exercised here is the contract
// that matters on every *other* platform: the weak fallback.
//
// That fallback is why a sketch can call make_littlefs_filesystem()
// unconditionally. Without it the call would fail to link on any platform
// lacking LittleFS; with it the sketch gets a null filesystem and a clean
// `begin()` failure it can branch on.
//
// The real backend is covered by compiling for a LittleFS-capable board —
// mounting a flash partition cannot be exercised on the host.

#include "test.h"

#include "fl/system/file_system.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("make_littlefs_filesystem links on a platform without LittleFS") {
    // The point of the weak definition: this call resolves at link time
    // even though no LittleFS implementation exists in this build.
    fl::FsImplPtr impl = fl::make_littlefs_filesystem();

    // Null, because there is nothing here to mount.
    FL_CHECK(!impl);
}

FL_TEST_CASE("format_on_fail argument does not change the fallback") {
    FL_CHECK(!fl::make_littlefs_filesystem(true));
    FL_CHECK(!fl::make_littlefs_filesystem(false));
}

FL_TEST_CASE("FileSystem::begin reports failure rather than crashing") {
    // The reason the fallback returns null instead of aborting: a sketch
    // written for ESP32 should still build and run on a board with no
    // flash filesystem, and simply find out at runtime.
    fl::FileSystem fs;
    FL_CHECK(!fs.begin(fl::make_littlefs_filesystem()));
}

FL_TEST_CASE("a failed mount leaves the FileSystem safe to call") {
    fl::FileSystem fs;
    fs.begin(fl::make_littlefs_filesystem());

    // No backend was installed, so reads fail closed rather than
    // dereferencing a null impl.
    fl::ifstream handle = fs.openRead("does_not_exist.bin");
    FL_CHECK(!handle.is_open());

    // And tearing down an unmounted filesystem is harmless.
    fs.end();
}

} // FL_TEST_FILE
