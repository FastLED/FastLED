#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/stdint.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "fl/span.h"
#include "fl/optional.h"
#include "fl/pair.h"

namespace fl {

/// HTTP headers collection with case-insensitive lookup
class HttpHeaders {
public:
    using HeaderPair = fl::pair<fl::string, fl::string>;
    using HeaderVector = fl::vector<HeaderPair>;
    
    /// Default constructor
    HttpHeaders() = default;
    
    /// Constructor from vector of header pairs
    explicit HttpHeaders(const HeaderVector& headers) : mHeaders(headers) {}
    
    /// Set header value (replaces existing)
    void set(const fl::string& name, const fl::string& value);
    
    /// Add header value (allows duplicates)
    void add(const fl::string& name, const fl::string& value);
    
    /// Get header value (case-insensitive)
    fl::optional<fl::string> get(const fl::string& name) const;
    
    /// Get all values for header (case-insensitive)
    fl::vector<fl::string> get_all(const fl::string& name) const;
    
    /// Check if header exists (case-insensitive)
    bool has(const fl::string& name) const;
    
    /// Remove header (case-insensitive)
    void remove(const fl::string& name);
    
    /// Clear all headers
    void clear();
    
    /// Get all headers
    const HeaderVector& get_all() const { return mHeaders; }
    
    /// Header count
    fl::size size() const { return mHeaders.size(); }
    bool empty() const { return mHeaders.empty(); }
    
    /// Iterator support
    HeaderVector::iterator begin() { return mHeaders.begin(); }
    HeaderVector::iterator end() { return mHeaders.end(); }
    HeaderVector::const_iterator begin() const { return mHeaders.begin(); }
    HeaderVector::const_iterator end() const { return mHeaders.end(); }
    
private:
    HeaderVector mHeaders;
    
    /// Case-insensitive header name comparison
    bool header_name_equals(const fl::string& a, const fl::string& b) const;
    fl::string to_lower(const fl::string& str) const;
};

/// HTTP request method enumeration
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT
};

/// Convert HTTP method to string
fl::string to_string(HttpMethod method);

/// Parse HTTP method from string
fl::optional<HttpMethod> parse_http_method(const fl::string& method);

/// HTTP version enumeration
enum class HttpVersion {
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2_0
};

/// Convert HTTP version to string
fl::string to_string(HttpVersion version);

/// HTTP request class
class Request {
public:
    /// Default constructor (GET /)
    Request();
    
    /// Constructor with method and URL
    Request(HttpMethod method, const fl::string& url);
    Request(const fl::string& method, const fl::string& url);
    
    /// Method access
    void set_method(HttpMethod method) { mMethod = method; }
    void set_method(const fl::string& method);
    HttpMethod get_method() const { return mMethod; }
    fl::string get_method_string() const { return to_string(mMethod); }
    
    /// URL access
    void set_url(const fl::string& url) { mUrl = url; }
    const fl::string& get_url() const { return mUrl; }
    
    /// HTTP version access
    void set_version(HttpVersion version) { mVersion = version; }
    HttpVersion get_version() const { return mVersion; }
    
    /// Headers access
    HttpHeaders& headers() { return mHeaders; }
    const HttpHeaders& headers() const { return mHeaders; }
    void set_header(const fl::string& name, const fl::string& value) { mHeaders.set(name, value); }
    fl::optional<fl::string> get_header(const fl::string& name) const { return mHeaders.get(name); }
    
    /// Body access
    void set_body(fl::span<const fl::u8> data);
    void set_body(const fl::string& text);
    void set_body(const fl::vector<fl::u8>& data);
    const fl::vector<fl::u8>& get_body() const { return mBody; }
    fl::span<const fl::u8> get_body_span() const;
    fl::string get_body_text() const;
    fl::size get_body_size() const { return mBody.size(); }
    bool has_body() const { return !mBody.empty(); }
    void clear_body() { mBody.clear(); }
    
    /// Content type convenience methods
    void set_content_type(const fl::string& content_type);
    fl::optional<fl::string> get_content_type() const;
    
    /// Content length (automatically managed)
    fl::size get_content_length() const { return mBody.size(); }
    
    /// User agent convenience
    void set_user_agent(const fl::string& user_agent);
    fl::optional<fl::string> get_user_agent() const;
    
    /// Accept header convenience
    void set_accept(const fl::string& accept);
    fl::optional<fl::string> get_accept() const;
    
    /// Authorization convenience
    void set_authorization(const fl::string& auth);
    fl::optional<fl::string> get_authorization() const;
    
    /// Validate request
    bool is_valid() const;
    fl::string get_validation_error() const;
    
private:
    HttpMethod mMethod;
    fl::string mUrl;
    HttpVersion mVersion;
    HttpHeaders mHeaders;
    fl::vector<fl::u8> mBody;
};

/// HTTP status code enumeration
enum class HttpStatusCode : fl::u16 {
    // 1xx Informational
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    
    // 2xx Success
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    PARTIAL_CONTENT = 206,
    
    // 3xx Redirection
    MULTIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,
    
    // 4xx Client Error
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PAYLOAD_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    UPGRADE_REQUIRED = 426,
    TOO_MANY_REQUESTS = 429,
    
    // 5xx Server Error
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505
};

/// Get status code category
bool is_informational(HttpStatusCode code);
bool is_success(HttpStatusCode code);
bool is_redirection(HttpStatusCode code);
bool is_client_error(HttpStatusCode code);
bool is_server_error(HttpStatusCode code);
bool is_error(HttpStatusCode code);

/// Convert status code to string
fl::string to_string(HttpStatusCode code);
fl::string get_status_text(HttpStatusCode code);

/// HTTP response class
class Response {
public:
    /// Default constructor (200 OK)
    Response();
    
    /// Constructor with status code
    explicit Response(HttpStatusCode status_code);
    Response(fl::u16 status_code);
    
    /// Status code access
    void set_status_code(HttpStatusCode code) { mStatusCode = code; }
    void set_status_code(fl::u16 code) { mStatusCode = static_cast<HttpStatusCode>(code); }
    HttpStatusCode get_status_code() const { return mStatusCode; }
    fl::u16 get_status_code_int() const { return static_cast<fl::u16>(mStatusCode); }
    fl::string get_status_text() const { return ::fl::get_status_text(mStatusCode); }
    
    /// Status code category helpers
    bool is_success() const { return ::fl::is_success(mStatusCode); }
    bool is_redirection() const { return ::fl::is_redirection(mStatusCode); }
    bool is_client_error() const { return ::fl::is_client_error(mStatusCode); }
    bool is_server_error() const { return ::fl::is_server_error(mStatusCode); }
    bool is_error() const { return ::fl::is_error(mStatusCode); }
    
    /// HTTP version access
    void set_version(HttpVersion version) { mVersion = version; }
    HttpVersion get_version() const { return mVersion; }
    
    /// Headers access
    HttpHeaders& headers() { return mHeaders; }
    const HttpHeaders& headers() const { return mHeaders; }
    void set_header(const fl::string& name, const fl::string& value) { mHeaders.set(name, value); }
    fl::optional<fl::string> get_header(const fl::string& name) const { return mHeaders.get(name); }
    
    /// Body access
    void set_body(fl::span<const fl::u8> data);
    void set_body(const fl::string& text);
    void set_body(const fl::vector<fl::u8>& data);
    const fl::vector<fl::u8>& get_body() const { return mBody; }
    fl::span<const fl::u8> get_body_span() const;
    fl::string get_body_text() const;
    fl::size get_body_size() const { return mBody.size(); }
    bool has_body() const { return !mBody.empty(); }
    void clear_body() { mBody.clear(); }
    
    /// Content type convenience methods
    fl::optional<fl::string> get_content_type() const;
    
    /// Content length convenience
    fl::optional<fl::size> get_content_length() const;
    
    /// Server convenience
    fl::optional<fl::string> get_server() const;
    
    /// Date convenience
    fl::optional<fl::string> get_date() const;
    
    /// Location convenience (for redirects)
    fl::optional<fl::string> get_location() const;
    
    /// Response validation
    bool is_valid() const;
    fl::string get_validation_error() const;
    
private:
    HttpStatusCode mStatusCode;
    HttpVersion mVersion;
    HttpHeaders mHeaders;
    fl::vector<fl::u8> mBody;
};

/// URL parsing result
struct UrlComponents {
    fl::string scheme;      ///< http, https, etc.
    fl::string host;        ///< hostname or IP address
    fl::string port_str;    ///< port as string (empty if default)
    int port;               ///< port number (80 for http, 443 for https by default)
    fl::string path;        ///< path component (starts with /)
    fl::string query;       ///< query string (without ?)
    fl::string fragment;    ///< fragment (without #)
    fl::string userinfo;    ///< user:password@ component
    
    /// Reconstruct full URL
    fl::string to_string() const;
    
    /// Check if valid
    bool is_valid() const;
    
    /// Get default port for scheme
    static int get_default_port(const fl::string& scheme);
};

/// Parse URL into components
fl::optional<UrlComponents> parse_url(const fl::string& url);

/// URL encoding/decoding utilities
fl::string url_encode(const fl::string& str);
fl::string url_decode(const fl::string& str);

/// Query string utilities
fl::string build_query_string(const fl::vector<fl::pair<fl::string, fl::string>>& params);
fl::vector<fl::pair<fl::string, fl::string>> parse_query_string(const fl::string& query);

/// Basic authentication encoding
fl::string encode_basic_auth(const fl::string& username, const fl::string& password);

/// HTTP date formatting
fl::string format_http_date(fl::u64 timestamp);
fl::optional<fl::u64> parse_http_date(const fl::string& date_str);

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
