#pragma once

/// @file url.h
/// @brief Lightweight URL parser for embedded environments.
/// Parses once on construction; accessors return fl::string_view into the
/// owned fl::string — zero-copy, trivially cheap.

#include "fl/stl/int.h"
#include "fl/stl/string.h"
#include "fl/stl/string_view.h"
#include "fl/stl/noexcept.h"

namespace fl {

class url {
  public:
    url() FL_NOEXCEPT : mValid(false), mRepaired(false) { zeroOffsets(); }

    explicit url(const char *url) FL_NOEXCEPT
        : mUrl(url), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    explicit url(const fl::string &u) FL_NOEXCEPT
        : mUrl(u), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    explicit url(fl::string_view url) FL_NOEXCEPT
        : mUrl(url.data(), url.size()), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    bool isValid() const FL_NOEXCEPT { return mValid; }
    explicit operator bool() const FL_NOEXCEPT { return mValid; }

    /// True if the URL was missing a scheme and "https://" was assumed.
    bool wasRepaired() const FL_NOEXCEPT { return mRepaired; }

    // ---- Component accessors (return views into mUrl) ----
    fl::string_view scheme() const FL_NOEXCEPT { return view(mScheme); }
    fl::string_view userinfo() const FL_NOEXCEPT { return view(mUserinfo); }
    fl::string_view host() const FL_NOEXCEPT { return view(mHost); }
    fl::string_view port_str() const FL_NOEXCEPT { return view(mPort); }
    fl::string_view path() const FL_NOEXCEPT { return view(mPath); }
    fl::string_view query() const FL_NOEXCEPT { return view(mQuery); }
    fl::string_view fragment() const FL_NOEXCEPT { return view(mFragment); }
    fl::string_view authority() const FL_NOEXCEPT { return view(mAuthority); }

    /// Numeric port. Returns the explicit port if present, otherwise the
    /// well-known default for the scheme (80 for http, 443 for https, etc.).
    fl::u16 port() const FL_NOEXCEPT {
        fl::string_view p = port_str();
        if (p.empty()) {
            return defaultPort();
        }
        fl::u16 result = 0;
        for (fl::size i = 0; i < p.size(); ++i) {
            result = static_cast<fl::u16>(result * 10 +
                                          static_cast<fl::u16>(p[i] - '0'));
        }
        return result;
    }

    // ---- Whole-URL access ----
    fl::string_view str() const FL_NOEXCEPT {
        return fl::string_view(mUrl.c_str(), mUrl.size());
    }
    const fl::string &string() const FL_NOEXCEPT { return mUrl; }
    const char *c_str() const FL_NOEXCEPT { return mUrl.c_str(); }

    // ---- Comparison ----
    bool operator==(const url &o) const FL_NOEXCEPT { return mUrl == o.mUrl; }
    bool operator!=(const url &o) const FL_NOEXCEPT { return !(mUrl == o.mUrl); }

  private:
    // A (offset, length) pair stored as uint16_t.
    struct Span {
        fl::u16 off;
        fl::u16 len;
    };

    fl::string_view view(const Span &s) const FL_NOEXCEPT {
        if (s.len == 0)
            return fl::string_view();
        return fl::string_view(mUrl.c_str() + s.off, s.len);
    }

    void zeroOffsets() FL_NOEXCEPT {
        mScheme = {0, 0};
        mUserinfo = {0, 0};
        mHost = {0, 0};
        mPort = {0, 0};
        mPath = {0, 0};
        mQuery = {0, 0};
        mFragment = {0, 0};
        mAuthority = {0, 0};
    }

    static Span makeSpan(fl::size off, fl::size len) FL_NOEXCEPT {
        Span s;
        s.off = static_cast<fl::u16>(off);
        s.len = static_cast<fl::u16>(len);
        return s;
    }

    fl::u16 defaultPort() const FL_NOEXCEPT {
        fl::string_view s = scheme();
        if (s == "https" || s == "wss")
            return 443;
        if (s == "http" || s == "ws")
            return 80;
        if (s == "ftp")
            return 21;
        return 0;
    }

    void parse() FL_NOEXCEPT {
        if (mUrl.empty()) {
            return;
        }

        fl::string_view src(mUrl.c_str(), mUrl.size());
        fl::size pos = 0;

        // 1. Scheme — look for "://"
        fl::size sep = src.find("://");
        if (sep == fl::string_view::npos) {
            // No scheme found — assume https:// and retry
            mUrl = fl::string("https://") + mUrl;
            mRepaired = true;
            src = fl::string_view(mUrl.c_str(), mUrl.size());
            sep = src.find("://");
        }
        mScheme = makeSpan(0, sep);
        pos = sep + 3; // skip past "://"

        // 2. Authority — everything up to first '/', '?', or '#'
        fl::size authStart = pos;
        fl::size authEnd = src.size();
        for (fl::size i = pos; i < src.size(); ++i) {
            char c = src[i];
            if (c == '/' || c == '?' || c == '#') {
                authEnd = i;
                break;
            }
        }
        mAuthority = makeSpan(authStart, authEnd - authStart);

        // 3. Parse authority into userinfo, host, port
        parseAuthority(src.substr(authStart, authEnd - authStart), authStart);

        pos = authEnd;

        // 4. Path — up to '?' or '#'
        if (pos < src.size() && src[pos] == '/') {
            fl::size pathStart = pos;
            fl::size pathEnd = src.size();
            for (fl::size i = pos; i < src.size(); ++i) {
                if (src[i] == '?' || src[i] == '#') {
                    pathEnd = i;
                    break;
                }
            }
            mPath = makeSpan(pathStart, pathEnd - pathStart);
            pos = pathEnd;
        }

        // 5. Query — after '?' up to '#'
        if (pos < src.size() && src[pos] == '?') {
            ++pos; // skip '?'
            fl::size qStart = pos;
            fl::size qEnd = src.size();
            fl::size hashPos = src.find('#', pos);
            if (hashPos != fl::string_view::npos) {
                qEnd = hashPos;
            }
            mQuery = makeSpan(qStart, qEnd - qStart);
            pos = qEnd;
        }

        // 6. Fragment — after '#'
        if (pos < src.size() && src[pos] == '#') {
            ++pos; // skip '#'
            mFragment = makeSpan(pos, src.size() - pos);
        }

        mValid = true;
    }

    void parseAuthority(fl::string_view auth, fl::size baseOff) FL_NOEXCEPT {
        fl::size pos = 0;

        // userinfo — everything before '@'
        fl::size at = auth.find('@');
        if (at != fl::string_view::npos) {
            mUserinfo = makeSpan(baseOff + pos, at);
            pos = at + 1; // skip '@'
        }

        // host (with IPv6 bracket support) and optional port
        if (pos < auth.size() && auth[pos] == '[') {
            // IPv6 literal
            fl::size closeBracket = auth.find(']', pos);
            if (closeBracket == fl::string_view::npos) {
                // Malformed, take rest as host
                mHost = makeSpan(baseOff + pos, auth.size() - pos);
                return;
            }
            // Include brackets in host
            mHost = makeSpan(baseOff + pos, closeBracket - pos + 1);
            pos = closeBracket + 1;
            if (pos < auth.size() && auth[pos] == ':') {
                ++pos;
                mPort = makeSpan(baseOff + pos, auth.size() - pos);
            }
        } else {
            // Regular host — find last ':' that separates host:port
            fl::size colon = auth.find(':', pos);
            if (colon != fl::string_view::npos) {
                mHost = makeSpan(baseOff + pos, colon - pos);
                fl::size portStart = colon + 1;
                mPort = makeSpan(baseOff + portStart, auth.size() - portStart);
            } else {
                mHost = makeSpan(baseOff + pos, auth.size() - pos);
            }
        }
    }

    fl::string mUrl;
    bool mValid;
    bool mRepaired;
    Span mScheme;
    Span mUserinfo;
    Span mHost;
    Span mPort;
    Span mPath;
    Span mQuery;
    Span mFragment;
    Span mAuthority;
};

using Url = url;

/// Metadata extracted from a `.lnk` asset link file.
///
/// v1: parsed but not enforced; reserved for future integrity/fallback
/// features. The runtime ignores everything except `primary`.
struct LnkMetadata {
    fl::url primary;       ///< First non-comment URL in the file.
    fl::string sha256;     ///< Hex-encoded sha256 of the expected asset (reserved).
    fl::url fallback;      ///< Mirror URL used if `primary` fails (reserved).

    bool isValid() const FL_NOEXCEPT { return primary.isValid(); }
    explicit operator bool() const FL_NOEXCEPT { return primary.isValid(); }
};

/// Parse the contents of a `.lnk` file into a `fl::url`.
///
/// Format: one URL per line. Leading/trailing whitespace on each line is
/// ignored. Blank lines and lines whose first non-whitespace character is
/// `#` are treated as comments and skipped. The first non-comment,
/// non-blank line is returned as a `fl::url`.
///
/// Future-compat: additional lines (metadata like `sha256=...`,
/// `fallback=...`) are ignored by this v1 parser so older binaries keep
/// working when future `.lnk` files grow richer.
///
/// See also `parse_lnk_with_metadata()` which returns the URL alongside the
/// (currently unenforced) metadata fields for forward-compat callers.
inline url parse_lnk(fl::string_view content) FL_NOEXCEPT {
    fl::size pos = 0;
    while (pos < content.size()) {
        fl::size eol = content.find('\n', pos);
        fl::size lineEnd = (eol == fl::string_view::npos) ? content.size() : eol;
        fl::string_view line = content.substr(pos, lineEnd - pos);
        pos = (eol == fl::string_view::npos) ? content.size() : eol + 1;

        // Strip a trailing '\r' from CRLF line endings.
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        // Trim leading whitespace.
        fl::size s = 0;
        while (s < line.size() &&
               (line[s] == ' ' || line[s] == '\t')) {
            ++s;
        }
        // Trim trailing whitespace.
        fl::size e = line.size();
        while (e > s &&
               (line[e - 1] == ' ' || line[e - 1] == '\t')) {
            --e;
        }
        line = line.substr(s, e - s);

        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            continue;
        }
        return url(line);
    }
    return url();
}

/// Parse a `.lnk` file into URL + metadata (forward-compat).
///
/// Accepts the same format as `parse_lnk()` and additionally scans
/// non-comment `key=value` lines that follow the primary URL. Recognized
/// keys:
///   - `sha256=<hex>`   — stored in `LnkMetadata::sha256`
///   - `fallback=<url>` — stored in `LnkMetadata::fallback`
/// Unknown keys are silently ignored (forward-compat).
///
/// v1: the returned metadata is parsed but NOT enforced by the runtime.
/// `sha256` is not verified and `fallback` is not retried. These fields
/// are reserved for future integrity/retry features so existing `.lnk`
/// files remain valid when richer behavior lands.
inline LnkMetadata parse_lnk_with_metadata(fl::string_view content) FL_NOEXCEPT {
    LnkMetadata out;
    bool gotPrimary = false;
    fl::size pos = 0;
    while (pos < content.size()) {
        fl::size eol = content.find('\n', pos);
        fl::size lineEnd = (eol == fl::string_view::npos) ? content.size() : eol;
        fl::string_view line = content.substr(pos, lineEnd - pos);
        pos = (eol == fl::string_view::npos) ? content.size() : eol + 1;

        if (!line.empty() && line[line.size() - 1] == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        fl::size s = 0;
        while (s < line.size() &&
               (line[s] == ' ' || line[s] == '\t')) {
            ++s;
        }
        fl::size e = line.size();
        while (e > s &&
               (line[e - 1] == ' ' || line[e - 1] == '\t')) {
            --e;
        }
        line = line.substr(s, e - s);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (!gotPrimary) {
            out.primary = url(line);
            gotPrimary = true;
            continue;
        }

        // Subsequent lines: look for "key=value" metadata.
        fl::size eq = line.find('=');
        if (eq == fl::string_view::npos) {
            // Unknown non-kv line, ignore (forward-compat).
            continue;
        }
        fl::string_view key = line.substr(0, eq);
        fl::string_view value = line.substr(eq + 1);
        if (key == "sha256") {
            out.sha256 = fl::string(value.data(), value.size());
        } else if (key == "fallback") {
            out.fallback = url(value);
        }
        // else: unknown key, ignore.
    }
    return out;
}

} // namespace fl
