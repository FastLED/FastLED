#pragma once

/// @file asset.h
/// @brief First-class asset handles for sketches that live under `<sketch>/data/`.
///
/// This header is part of the v1 asset pipeline (FastLED issue #2284).
///
/// Usage from a sketch:
/// @code
///     fl::UIAudio audio("Audio", FL_ASSET("data/track.mp3"));
/// @endcode
///
/// At build time, fbuild scans `<sketch>/data/` for `*.lnk` files containing
/// URLs and emits a manifest. At runtime:
///  - On WASM, the manifest is consulted to turn the relative path into the
///    URL the browser should fetch.
///  - On host/stub, the asset resolves to the file on disk — or, if the raw
///    file is missing but a `*.lnk` exists alongside, to the URL from the
///    `.lnk` (same fallback as WASM).
///
/// **v1 scope:** WASM and host/stub only. ESP32 LittleFS / SD-card resolution
/// is future work and intentionally omitted.
///
/// **Compile-time validation:** `FL_ASSET("data/../foo.mp3")` and any path
/// containing `..` segments is rejected via `FL_STATIC_ASSERT`. The normalized
/// path is stored verbatim; runtime resolution never walks parent directories.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/string.h"
#include "fl/stl/string_view.h"
#include "fl/stl/url.h"

namespace fl {

namespace asset_detail {

/// Compile-time length of a null-terminated C string. Recursive constexpr
/// form — compatible with C++11-only `constexpr` (no loops / locals).
constexpr fl::size clen(const char* s, fl::size n = 0) FL_NOEXCEPT {
    return s[n] == '\0' ? n : clen(s, n + 1);
}

/// Compile-time check: does position `i` in `p` start a ".." segment?
///
/// A ".." segment is two consecutive dots at a segment boundary — i.e. the
/// preceding character is `'/'`, `'\\'`, or the string start, and the
/// following character is `'/'`, `'\\'`, or the string end.
constexpr bool is_parent_at(const char* p, fl::size i) FL_NOEXCEPT {
    // Segment start: either we're at the beginning, or preceded by a separator.
    return (i == 0 || p[i - 1] == '/' || p[i - 1] == '\\')
           && p[i] == '.' && p[i + 1] == '.'
           && (p[i + 2] == '\0' || p[i + 2] == '/' || p[i + 2] == '\\');
}

/// Recursive constexpr walk looking for any ".." segment.
constexpr bool path_has_parent_segment_at(const char* p, fl::size i) FL_NOEXCEPT {
    return p[i] == '\0'
               ? false
               : (is_parent_at(p, i) ? true : path_has_parent_segment_at(p, i + 1));
}

/// Compile-time check: returns true if the string contains a ".." segment.
///
/// Rejects all of the following:
///   - "foo/../bar"
///   - "../foo"
///   - "foo/.."
///   - ".."
/// Does NOT reject single dots (".") or dots inside filename segments
/// ("foo..bar", "file.ext", "hidden.dotfile").
constexpr bool path_has_parent_segment(const char* p) FL_NOEXCEPT {
    return (p == nullptr) ? false : path_has_parent_segment_at(p, 0);
}

} // namespace asset_detail

/// Opaque handle to a sketch-local asset.
///
/// Value type: cheap to copy (a single pointer + size). Created via the
/// `FL_ASSET()` macro or `fl::asset(const char*)` below. The `FL_ASSET()`
/// macro performs an `FL_STATIC_ASSERT` that the path contains no `..` segments.
class asset_ref {
  public:
    /// Construct from a pointer to a null-terminated string with known length.
    /// The pointer MUST outlive the `asset_ref` — typically a string literal.
    constexpr asset_ref(const char* path, fl::size length) FL_NOEXCEPT
        : mPath(path), mLength(length) {}

    /// Default-constructed handle refers to no asset.
    constexpr asset_ref() FL_NOEXCEPT : mPath(nullptr), mLength(0) {}

    /// Copy/move: trivial — pointer + length. Can't combine `constexpr` with
    /// `= default` on the assignment operator pre-C++14, so we leave these as
    /// the implicitly-declared operations (the class is trivially copyable).
    asset_ref(const asset_ref&) FL_NOEXCEPT = default;
    asset_ref& operator=(const asset_ref&) FL_NOEXCEPT = default;

    /// True if this handle refers to an asset path.
    constexpr explicit operator bool() const FL_NOEXCEPT {
        return mPath != nullptr && mLength > 0;
    }

    /// The relative asset path as a string view (e.g. "data/track.mp3").
    fl::string_view path() const FL_NOEXCEPT {
        return fl::string_view(mPath, mLength);
    }

    /// Pointer accessor — useful for JSON serialization.
    constexpr const char* c_str() const FL_NOEXCEPT { return mPath; }
    constexpr fl::size size() const FL_NOEXCEPT { return mLength; }

  private:
    const char* mPath;
    fl::size mLength;
};

/// Construct an asset handle from a relative sketch path at runtime.
///
/// For **compile-time rejection** of `..` paths, use the `FL_ASSET()` macro
/// instead — it wraps this function with `FL_STATIC_ASSERT`.
///
/// This overload accepts any `const char*` (including runtime-computed
/// pointers) and, for safety, returns an empty handle if the path contains
/// a `..` segment. Runtime resolution also refuses to walk past `data/`, so
/// this is defense-in-depth rather than the primary gate.
constexpr asset_ref asset(const char* path) FL_NOEXCEPT {
    return asset_detail::path_has_parent_segment(path)
               ? asset_ref()
               : asset_ref(path, asset_detail::clen(path));
}

/// Preferred entry point: rejects `..` at compile time via `FL_STATIC_ASSERT`.
///
/// Use: `fl::UIAudio audio("Audio", FL_ASSET("data/track.mp3"));`
#define FL_ASSET(LITERAL_PATH)                                                 \
    ([]() -> ::fl::asset_ref {                                                 \
        FL_STATIC_ASSERT(                                                         \
            !::fl::asset_detail::path_has_parent_segment(LITERAL_PATH),        \
            "fl::asset path must not contain '..' segments");                  \
        return ::fl::asset_ref(                                                \
            (LITERAL_PATH), ::fl::asset_detail::clen(LITERAL_PATH));           \
    }())

/// Resolve an asset handle to a URL (or local file path) at runtime.
///
/// Resolution rules:
///   - **WASM:** consults the build-time manifest injected as
///     `window.fastledAssetManifest` (mirrored into the C++ registry by
///     generated code from `ci/compiler/asset_scanner.py`).
///   - **Host/stub (FASTLED_TESTING, FL_IS_STUB):** returns the file path
///     as a `fl::url` if the file exists on disk. Otherwise, if a sibling
///     `<path>.lnk` exists, returns the URL from the `.lnk` — same fallback
///     as WASM.
///   - **Other platforms (e.g., ESP32):** v1 returns an empty url(). ESP32
///     LittleFS / SD-card resolution is tracked as future work.
///
/// The runtime ignores any `sha256=` / `fallback=` metadata in `.lnk` files.
/// These are parsed by `fl::parse_lnk_with_metadata()` for forward-compat and
/// reserved for future integrity/retry features.
///
/// Declared here; defined in `fl/asset/asset.cpp.hpp`.
fl::url resolve_asset(const asset_ref& a) FL_NOEXCEPT;

/// Register an asset path → URL mapping at runtime.
///
/// Populates the same registry `resolve_asset()` consults. Called from:
///   - Generated C++ emitted by the fbuild asset scanner
///     (`ci/compiler/asset_scanner.py`) — populates the full sketch manifest.
///   - Unit tests and host code that want to stand up a manifest without
///     running fbuild.
///
/// Last writer wins for duplicate paths (scan then manually plug in tests).
void register_asset(fl::string_view path, const fl::url& u) FL_NOEXCEPT;

} // namespace fl
