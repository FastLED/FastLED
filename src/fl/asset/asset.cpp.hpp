#pragma once

// IWYU pragma: private

/// @file asset.cpp.hpp
/// @brief Runtime implementation of `fl::resolve_asset`.
///
/// Part of the v1 asset pipeline (FastLED issue #2284). v1 platforms are
/// WASM and host/stub only. Other platforms (e.g. ESP32) are handled by the
/// fallback branch below, which currently returns an empty `fl::url`.
/// TODO(#2284): ESP32 LittleFS / SD-card resolution is future work.

#include "fl/asset/asset.h"
#include "fl/stl/fstream.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/string_view.h"
#include "fl/stl/url.h"
#include "fl/stl/vector.h"
#include "platforms/is_platform.h"

namespace fl {

namespace asset_detail {

/// Process-local registry of (asset_path → resolved URL) pairs.
///
/// Populated in two ways:
///   1. Generated C++ emitted by `ci/compiler/asset_scanner.py` from the
///      fbuild asset scan (WASM/production path).
///   2. `register_asset()` direct calls from tests / host code that want to
///      plug their own mapping without running fbuild.
///
/// The registry is deliberately tiny — a flat vector scanned linearly. Sketch
/// asset counts are expected to be O(10), not O(1000). A hash map would be
/// overkill.
struct AssetEntry {
    fl::string path;  ///< Relative asset path (e.g. "data/track.mp3").
    fl::url url;      ///< Resolved URL (or file:// for host).
};

inline fl::vector<AssetEntry>& registry() FL_NOEXCEPT {
    // Construct-on-first-use avoids static-init ordering issues with the
    // generated asset table (which also uses static init).
    static fl::vector<AssetEntry> s_registry;
    return s_registry;
}

/// Look up an asset path in the registry. Returns invalid url() on miss.
inline fl::url lookup_registry(fl::string_view path) FL_NOEXCEPT {
    auto& r = registry();
    for (fl::size i = 0; i < r.size(); ++i) {
        // Compare as string_view on both sides to avoid any ambiguity in the
        // fl::string / fl::string_view overload resolution.
        fl::string_view candidate(r[i].path.c_str(), r[i].path.size());
        if (candidate == path) {
            return r[i].url;
        }
    }
    return fl::url();
}

#if defined(FASTLED_TESTING) || defined(FL_IS_STUB)
/// Host/stub resolution: try the raw file on disk, then the sibling `.lnk`.
///
/// Rules:
///   - If `<cwd>/<path>` exists, return `file://<absolute_or_relative_path>`.
///     We use the plain path as the URL body since `fl::url` accepts arbitrary
///     strings; the test harness and host consumers treat the returned url's
///     string() as a filesystem path.
///   - Otherwise if `<cwd>/<path>.lnk` exists, return the URL from the
///     `.lnk` file (same fallback as WASM).
///   - Otherwise return invalid url().
inline fl::url resolve_host(fl::string_view path) FL_NOEXCEPT {
    // Try raw file on disk.
    fl::string pathStr(path.data(), path.size());
    {
        fl::ifstream probe(pathStr.c_str(), fl::ios::in | fl::ios::binary);
        if (probe.is_open()) {
            probe.close();
            // Return the path as-is (a relative/absolute filesystem path).
            // fl::url accepts any string; callers on host treat .string() as
            // a local path. This matches the existing AudioUrl test pattern
            // that stores a raw URL string in the url field.
            return fl::url(pathStr);
        }
    }

    // Fallback: try <path>.lnk alongside.
    fl::string lnkPath = pathStr;
    lnkPath += ".lnk";
    fl::ifstream lnk(lnkPath.c_str(), fl::ios::in | fl::ios::binary);
    if (!lnk.is_open()) {
        return fl::url();
    }
    // Read entire file. .lnk files are expected to be tiny (URL + a few
    // metadata lines); cap the read at 8 KiB to avoid pathological inputs.
    constexpr fl::size kMaxLnkBytes = 8 * 1024;
    fl::size sz = lnk.size();
    if (sz > kMaxLnkBytes) {
        sz = kMaxLnkBytes;
    }
    fl::string content;
    content.resize(sz);
    if (sz > 0) {
        lnk.read(&content[0], sz);
    }
    lnk.close();
    return parse_lnk(fl::string_view(content.data(), content.size()));
}
#endif

} // namespace asset_detail

/// Public helper: plug an asset mapping at runtime. Used by:
///   - Generated C++ from `ci/compiler/asset_scanner.py`.
///   - Unit tests that want to simulate a manifest without fbuild.
void register_asset(fl::string_view path, const fl::url& u) FL_NOEXCEPT {
    asset_detail::registry().push_back(
        {fl::string(path.data(), path.size()), u});
}

fl::url resolve_asset(const asset_ref& a) FL_NOEXCEPT {
    if (!a) {
        return fl::url();
    }
    const fl::string_view p = a.path();

    // 1. Registry always wins. On WASM this is populated by generated code
    //    from the asset scanner; on host/stub tests may populate it manually.
    fl::url fromRegistry = asset_detail::lookup_registry(p);
    if (fromRegistry.isValid()) {
        return fromRegistry;
    }

    // 2. Host/stub: fall back to filesystem probe + sibling .lnk.
#if defined(FASTLED_TESTING) || defined(FL_IS_STUB)
    return asset_detail::resolve_host(p);
#else
    // 3. Other platforms (WASM without manifest, ESP32 v1): no fallback.
    //    TODO(#2284): ESP32 LittleFS / SD-card resolution.
    return fl::url();
#endif
}

} // namespace fl
