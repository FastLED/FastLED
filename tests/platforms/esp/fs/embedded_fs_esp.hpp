// ESP32 hardware-only fixture staged into the canonical AutoResearch sketch.
#pragma once
#include "fl/fs/embedded/embedded_fs.h"
#include "fl/stl/json.h"
#include "fl/stl/stdint.h"
#include "platforms/embedded_fs_provider.h"
#include "esp_littlefs.h"
#include "esp_heap_caps.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#if FL_TEST_EMBEDDED_FS_PROVIDER
#include "platforms/arduino/fs/embedded_fs_arduino.hpp"
inline unsigned& embeddedProviderCalls() { static unsigned calls = 0; return calls; }
namespace fl { namespace platforms {
FsImplPtr fastled_embedded_fs_provider(bool format) noexcept {
    ++embeddedProviderCalls();
    return makeArduinoEmbeddedFs(format);
}
}}
#endif
namespace embedded_fs_hil {
inline fl::json probe(const fl::json&) {
    fl::json response = fl::json::object();
    struct Check { const char* name; bool passed; } records[128] = {};
    int failures = 0;
    int total = 0;
    auto check = [&](const char* name, bool passed) { if (total < 128) records[total] = {name, passed}; ++total; if (!passed) ++failures; };
    check("heap_before", heap_caps_check_integrity_all(true));
    // Fixture preparation owns this registration and only writes named test files.
    // Formatting is enabled only by the explicit host test-fixture option.
    esp_vfs_littlefs_conf_t config = {};
    config.base_path = "/littlefs";
    config.partition_label = "spiffs";
    config.format_if_mount_failed = FL_TEST_EMBEDDED_FS_FORMAT_FIXTURE;
    const bool external = esp_littlefs_mounted("spiffs");
    if (external) {
        response.set("success", false);
        response.set("error", "Test fixture requires an unmounted volume; existing owner was left unchanged");
        return response;
    }
    const bool mounted = external || esp_vfs_littlefs_register(&config) == ESP_OK;
    check("fixture_mount", mounted);
    if (!mounted) { response.set("success", false); return response; }
    unsigned char expected[64];
    for (unsigned i = 0; i < sizeof(expected); ++i) expected[i] = static_cast<unsigned char>(i * 37u ^ 0xa5u);
    const int fd = ::open("/littlefs/fastled_4889.bin", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    check("fixture_open", fd >= 0);
    if (fd >= 0) { check("fixture_write", ::write(fd, expected, sizeof(expected)) == sizeof(expected)); ::close(fd); }
    const int zero = ::open("/littlefs/fastled_4889_empty.bin", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    check("empty_fixture", zero >= 0);
    if (zero >= 0) ::close(zero);
    if (!external) check("fixture_unmount", esp_vfs_littlefs_unregister("spiffs") == ESP_OK);
#if FL_TEST_EMBEDDED_FS_PROVIDER
    {
        auto unavailable = fl::getEmbeddedFs(true);
        check("unmounted_provider_fails", unavailable && !unavailable->begin());
        check("failed_provider_did_not_mount", !esp_littlefs_mounted("spiffs"));
    }
    // Initialize actual Arduino File mount metadata before testing its adapter.
    // Keep the application owner mounted while adapters borrow it.
    check("arduino_init", LittleFS.begin(false));
#endif
    long sdkAppendSize = 0;
    int sdkAppendRead = 0;
    const auto heapStart = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for (int iteration = 0; iteration < 3; ++iteration) {
        auto fs = fl::getEmbeddedFs(false);
        auto other = fl::getEmbeddedFs(false);
        check("factory", static_cast<bool>(fs) && static_cast<bool>(other));
        if (!fs || !other) break;
        check("begin", fs->begin());
        check("begin_idempotent", fs->begin());
        check("other_begin", other->begin());
        auto missing = fs->openRead("/fastled_4889_missing.bin");
        check("missing", !missing);
        auto file = fs->openRead("/fastled_4889.bin");
        check("open", file && file->is_open());
        if (!file) break;
        check("size", file->size() == sizeof(expected));
        check("logical_path", fl::string(file->path()) == "/fastled_4889.bin");
        unsigned char actual[64] = {};
        check("read_all", file->read(actual, sizeof(actual)) == sizeof(actual));
        check("exact_eof", file->is_eof());
#if !FL_TEST_EMBEDDED_FS_PROVIDER
        const int witness = ::open("/littlefs/fastled_4889.bin", O_RDONLY);
        check("sdk_witness_open", witness >= 0 && ::lseek(witness, 64, SEEK_SET) == 64);
        const int append = ::open("/littlefs/fastled_4889.bin", O_WRONLY | O_APPEND);
        const char extraByte = 'z';
        check("append_fixture", append >= 0 && ::write(append, &extraByte, 1) == 1);
        if (append >= 0) ::close(append);
        struct stat sdkMetadata = {};
        check("sdk_metadata", witness >= 0 && ::fstat(witness, &sdkMetadata) == 0);
        sdkAppendSize = sdkMetadata.st_size;
        check("metadata_matches_sdk", file->size() == static_cast<fl::size>(sdkAppendSize));
        char sdkExtra = 0;
        sdkAppendRead = witness >= 0 ? ::read(witness, &sdkExtra, 1) : -1;
        char readExtra = 0;
        const auto actualRead = file->read(&readExtra, 1);
        check("growth_read_matches_sdk", sdkAppendRead >= 0 && actualRead == static_cast<fl::size>(sdkAppendRead) && (!actualRead || readExtra == sdkExtra));
        if (witness >= 0) ::close(witness);
        const int restore = ::open("/littlefs/fastled_4889.bin", O_WRONLY | O_TRUNC);
        check("restore_fixture", restore >= 0 && ::write(restore, expected, sizeof(expected)) == sizeof(expected));
        if (restore >= 0) ::close(restore);
#endif
        check("contents", fl::memcmp(actual, expected, sizeof(actual)) == 0);
        check("seek_begin", file->seek(5, fl::seek_dir::beg));
        check("tell", file->tell() == 5);
        check("seek_current", file->seek(2, fl::seek_dir::cur) && file->tell() == 7);
        check("seek_end", file->seek(0, fl::seek_dir::end) && file->tell() == 64);
        check("readonly", file->write("x", 1) == 0);
        auto empty = fs->openRead("/fastled_4889_empty.bin");
        check("empty_eof", empty && empty->size() == 0 && empty->is_eof());
        if (empty) empty->close();
        fs->end();
        check("other_owner_survives", esp_littlefs_mounted("spiffs"));
        other->end();
        check("file_holds_mount", esp_littlefs_mounted("spiffs"));
        check("read_after_fs_end", file->seek(0) && file->read(actual, sizeof(actual)) == sizeof(actual));
        file->close();
        check("close_releases_mount", (FL_TEST_EMBEDDED_FS_PROVIDER || external) ? esp_littlefs_mounted("spiffs") : !esp_littlefs_mounted("spiffs"));
        check("closed", !file->is_open());
#if !FL_TEST_EMBEDDED_FS_PROVIDER
        check("closed_read_error", file->read(actual, 1) == 0 && file->has_error());
        file->clear_error();
        check("clear_error", !file->has_error());
#endif
        fs->end(); other->end();
    }
    const auto heapEnd = heap_caps_get_free_size(MALLOC_CAP_8BIT);
#if FL_TEST_EMBEDDED_FS_PROVIDER
    if (!external) LittleFS.end(); // Release only the application owner created by this fixture.
#endif
    if (!external) {
        config.format_if_mount_failed = false;
        config.base_path = "/littlefs";
        check("borrow_setup", esp_vfs_littlefs_register(&config) == ESP_OK);
#if FL_TEST_EMBEDDED_FS_PROVIDER
        // A native-mounted volume does not populate Arduino's File mountpoint.
        // Restore then test a genuine externally owned Arduino custom route.
        esp_vfs_littlefs_unregister("spiffs");
        check("custom_owner_mount", LittleFS.begin(false, "/fs4889"));
#endif
        auto borrowed = fl::getEmbeddedFs(true);
        check("borrow_factory", static_cast<bool>(borrowed));
        if (borrowed) {
            check("borrow_begin", borrowed->begin());
            auto file = borrowed->openRead("/fastled_4889.bin");
            check("borrow_read", file && file->size() == 64);
            borrowed->end();
            if (file) file->close();
        }
        check("borrow_not_unmounted", esp_littlefs_mounted("spiffs"));
#if FL_TEST_EMBEDDED_FS_PROVIDER
        LittleFS.end();
#else
        esp_vfs_littlefs_unregister("spiffs");
        config.base_path = "/fs4889";
        check("custom_native_mount", esp_vfs_littlefs_register(&config) == ESP_OK);
        auto unsupported = fl::getEmbeddedFs(true);
        check("custom_route_fails_safe", unsupported && !unsupported->begin());
        if (unsupported) unsupported->end();
        check("failed_begin_not_unmounted", esp_littlefs_mounted("spiffs"));
        esp_vfs_littlefs_unregister("spiffs");
#endif
    }
    check("heap_after", heap_caps_check_integrity_all(true));
    fl::json checks = fl::json::object();
    for (int i=0; i<total && i<128; ++i) checks.set(records[i].name, records[i].passed);
    response.set("factoryPresent", static_cast<bool>(fl::platforms::fastled_embedded_fs_provider));
#if FL_TEST_EMBEDDED_FS_PROVIDER
    response.set("providerCalls", embeddedProviderCalls());
#endif
    response.set("sdkAppendSize", sdkAppendSize);
    response.set("sdkAppendRead", sdkAppendRead);
    response.set("heapStart", heapStart);
    response.set("heapEnd", heapEnd);
    response.set("checks", checks);
    response.set("total", total);
    response.set("failures", failures);
    response.set("provider", static_cast<bool>(FL_TEST_EMBEDDED_FS_PROVIDER));
    response.set("success", failures == 0);
    return response;
}
}
