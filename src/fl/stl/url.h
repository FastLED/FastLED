#pragma once

/// @file url.h
/// @brief Lightweight URL parser for embedded environments.
/// Parses once on construction; accessors return fl::string_view into the
/// owned fl::string — zero-copy, trivially cheap.

#include "fl/int.h"
#include "fl/stl/string.h"
#include "fl/string_view.h"

namespace fl {

class url {
  public:
    url() : mValid(false), mRepaired(false) { zeroOffsets(); }

    explicit url(const char *url)
        : mUrl(url), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    explicit url(const fl::string &u)
        : mUrl(u), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    explicit url(fl::string_view url)
        : mUrl(url.data(), url.size()), mValid(false), mRepaired(false) {
        zeroOffsets();
        parse();
    }

    bool isValid() const { return mValid; }
    explicit operator bool() const { return mValid; }

    /// True if the URL was missing a scheme and "https://" was assumed.
    bool wasRepaired() const { return mRepaired; }

    // ---- Component accessors (return views into mUrl) ----
    fl::string_view scheme() const { return view(mScheme); }
    fl::string_view userinfo() const { return view(mUserinfo); }
    fl::string_view host() const { return view(mHost); }
    fl::string_view port_str() const { return view(mPort); }
    fl::string_view path() const { return view(mPath); }
    fl::string_view query() const { return view(mQuery); }
    fl::string_view fragment() const { return view(mFragment); }
    fl::string_view authority() const { return view(mAuthority); }

    /// Numeric port. Returns the explicit port if present, otherwise the
    /// well-known default for the scheme (80 for http, 443 for https, etc.).
    fl::u16 port() const {
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
    fl::string_view str() const {
        return fl::string_view(mUrl.c_str(), mUrl.size());
    }
    const fl::string &string() const { return mUrl; }
    const char *c_str() const { return mUrl.c_str(); }

    // ---- Comparison ----
    bool operator==(const url &o) const { return mUrl == o.mUrl; }
    bool operator!=(const url &o) const { return !(mUrl == o.mUrl); }

  private:
    // A (offset, length) pair stored as uint16_t.
    struct Span {
        fl::u16 off;
        fl::u16 len;
    };

    fl::string_view view(const Span &s) const {
        if (s.len == 0)
            return fl::string_view();
        return fl::string_view(mUrl.c_str() + s.off, s.len);
    }

    void zeroOffsets() {
        mScheme = {0, 0};
        mUserinfo = {0, 0};
        mHost = {0, 0};
        mPort = {0, 0};
        mPath = {0, 0};
        mQuery = {0, 0};
        mFragment = {0, 0};
        mAuthority = {0, 0};
    }

    static Span makeSpan(fl::size off, fl::size len) {
        Span s;
        s.off = static_cast<fl::u16>(off);
        s.len = static_cast<fl::u16>(len);
        return s;
    }

    fl::u16 defaultPort() const {
        fl::string_view s = scheme();
        if (s == "https" || s == "wss")
            return 443;
        if (s == "http" || s == "ws")
            return 80;
        if (s == "ftp")
            return 21;
        return 0;
    }

    void parse() {
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

    void parseAuthority(fl::string_view auth, fl::size baseOff) {
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

} // namespace fl
