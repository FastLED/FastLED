#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/http_types.h"
#include "fl/algorithm.h"
#include "fl/string.h"
#include "fl/sstream.h"

namespace fl {

// ========== HttpHeaders Implementation ==========

void HttpHeaders::set(const fl::string& name, const fl::string& value) {
    // Remove existing headers with same name
    remove(name);
    // Add new header
    mHeaders.emplace_back(name, value);
}

void HttpHeaders::add(const fl::string& name, const fl::string& value) {
    mHeaders.emplace_back(name, value);
}

fl::optional<fl::string> HttpHeaders::get(const fl::string& name) const {
    for (const auto& header : mHeaders) {
        if (header_name_equals(header.first, name)) {
            return header.second;
        }
    }
    return fl::nullopt;
}

fl::vector<fl::string> HttpHeaders::get_all(const fl::string& name) const {
    fl::vector<fl::string> values;
    for (const auto& header : mHeaders) {
        if (header_name_equals(header.first, name)) {
            values.push_back(header.second);
        }
    }
    return values;
}

bool HttpHeaders::has(const fl::string& name) const {
    for (const auto& header : mHeaders) {
        if (header_name_equals(header.first, name)) {
            return true;
        }
    }
    return false;
}

void HttpHeaders::remove(const fl::string& name) {
    mHeaders.erase(
        fl::remove_if(mHeaders.begin(), mHeaders.end(),
            [this, &name](const HeaderPair& header) {
                return header_name_equals(header.first, name);
            }),
        mHeaders.end()
    );
}

void HttpHeaders::clear() {
    mHeaders.clear();
}

bool HttpHeaders::header_name_equals(const fl::string& a, const fl::string& b) const {
    return to_lower(a) == to_lower(b);
}

fl::string HttpHeaders::to_lower(const fl::string& str) const {
    fl::string result = str;
    for (fl::size i = 0; i < result.size(); ++i) {
        char& c = result[i];
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

// ========== HTTP Method Functions ==========

fl::string to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE:  return "DELETE";
        case HttpMethod::HEAD:    return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH:   return "PATCH";
        case HttpMethod::TRACE:   return "TRACE";
        case HttpMethod::CONNECT: return "CONNECT";
        default:                  return "GET";
    }
}

fl::optional<HttpMethod> parse_http_method(const fl::string& method) {
    fl::string upper_method = method;
    for (char& c : upper_method) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
    
    if (upper_method == "GET")     return HttpMethod::GET;
    if (upper_method == "POST")    return HttpMethod::POST;
    if (upper_method == "PUT")     return HttpMethod::PUT;
    if (upper_method == "DELETE")  return HttpMethod::DELETE;
    if (upper_method == "HEAD")    return HttpMethod::HEAD;
    if (upper_method == "OPTIONS") return HttpMethod::OPTIONS;
    if (upper_method == "PATCH")   return HttpMethod::PATCH;
    if (upper_method == "TRACE")   return HttpMethod::TRACE;
    if (upper_method == "CONNECT") return HttpMethod::CONNECT;
    
    return fl::nullopt;
}

// ========== HTTP Version Functions ==========

fl::string to_string(HttpVersion version) {
    switch (version) {
        case HttpVersion::HTTP_1_0: return "HTTP/1.0";
        case HttpVersion::HTTP_1_1: return "HTTP/1.1";
        case HttpVersion::HTTP_2_0: return "HTTP/2.0";
        default:                    return "HTTP/1.1";
    }
}

// ========== Request Implementation ==========

Request::Request() 
    : mMethod(HttpMethod::GET)
    , mUrl("/")
    , mVersion(HttpVersion::HTTP_1_1) {
}

Request::Request(HttpMethod method, const fl::string& url)
    : mMethod(method)
    , mUrl(url)
    , mVersion(HttpVersion::HTTP_1_1) {
}

Request::Request(const fl::string& method, const fl::string& url)
    : mUrl(url)
    , mVersion(HttpVersion::HTTP_1_1) {
    auto parsed_method = parse_http_method(method);
    mMethod = parsed_method ? *parsed_method : HttpMethod::GET;
}

void Request::set_method(const fl::string& method) {
    auto parsed_method = parse_http_method(method);
    mMethod = parsed_method ? *parsed_method : HttpMethod::GET;
}

void Request::set_body(fl::span<const fl::u8> data) {
    mBody.assign(data.begin(), data.end());
}

void Request::set_body(const fl::string& text) {
    const char* text_data = text.c_str();
    const fl::u8* u8_data = reinterpret_cast<const fl::u8*>(text_data);
    mBody.assign(u8_data, u8_data + text.size());
}

void Request::set_body(const fl::vector<fl::u8>& data) {
    mBody = data;
}

fl::span<const fl::u8> Request::get_body_span() const {
    return fl::span<const fl::u8>(mBody.data(), mBody.size());
}

fl::string Request::get_body_text() const {
    return fl::string(reinterpret_cast<const char*>(mBody.data()), mBody.size());
}

void Request::set_content_type(const fl::string& content_type) {
    set_header("Content-Type", content_type);
}

fl::optional<fl::string> Request::get_content_type() const {
    return get_header("Content-Type");
}

void Request::set_user_agent(const fl::string& user_agent) {
    set_header("User-Agent", user_agent);
}

fl::optional<fl::string> Request::get_user_agent() const {
    return get_header("User-Agent");
}

void Request::set_accept(const fl::string& accept) {
    set_header("Accept", accept);
}

fl::optional<fl::string> Request::get_accept() const {
    return get_header("Accept");
}

void Request::set_authorization(const fl::string& auth) {
    set_header("Authorization", auth);
}

fl::optional<fl::string> Request::get_authorization() const {
    return get_header("Authorization");
}

bool Request::is_valid() const {
    if (mUrl.empty()) return false;
    if (mMethod == HttpMethod::POST || mMethod == HttpMethod::PUT || mMethod == HttpMethod::PATCH) {
        // POST/PUT/PATCH should have Content-Type if they have a body
        if (has_body() && !get_content_type()) {
            return false;
        }
    }
    return true;
}

fl::string Request::get_validation_error() const {
    if (mUrl.empty()) return "URL cannot be empty";
    if (mMethod == HttpMethod::POST || mMethod == HttpMethod::PUT || mMethod == HttpMethod::PATCH) {
        if (has_body() && !get_content_type()) {
            return "Content-Type required for requests with body";
        }
    }
    return "";
}

// ========== HTTP Status Code Functions ==========

bool is_informational(HttpStatusCode code) {
    return static_cast<fl::u16>(code) >= 100 && static_cast<fl::u16>(code) < 200;
}

bool is_success(HttpStatusCode code) {
    return static_cast<fl::u16>(code) >= 200 && static_cast<fl::u16>(code) < 300;
}

bool is_redirection(HttpStatusCode code) {
    return static_cast<fl::u16>(code) >= 300 && static_cast<fl::u16>(code) < 400;
}

bool is_client_error(HttpStatusCode code) {
    return static_cast<fl::u16>(code) >= 400 && static_cast<fl::u16>(code) < 500;
}

bool is_server_error(HttpStatusCode code) {
    return static_cast<fl::u16>(code) >= 500 && static_cast<fl::u16>(code) < 600;
}

bool is_error(HttpStatusCode code) {
    return is_client_error(code) || is_server_error(code);
}

fl::string to_string(HttpStatusCode code) {
    fl::string result;
    fl::u16 code_int = static_cast<fl::u16>(code);
    
    // Simple integer to string conversion
    if (code_int == 0) {
        result = "0";
    } else {
        fl::vector<char> digits;
        fl::u16 temp = code_int;
        while (temp > 0) {
            digits.push_back('0' + (temp % 10));
            temp /= 10;
        }
        // Reverse digits
        for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
            result += *it;
        }
    }
    
    return result;
}

fl::string get_status_text(HttpStatusCode code) {
    switch (code) {
        // 1xx Informational
        case HttpStatusCode::CONTINUE: return "Continue";
        case HttpStatusCode::SWITCHING_PROTOCOLS: return "Switching Protocols";
        
        // 2xx Success
        case HttpStatusCode::OK: return "OK";
        case HttpStatusCode::CREATED: return "Created";
        case HttpStatusCode::ACCEPTED: return "Accepted";
        case HttpStatusCode::NO_CONTENT: return "No Content";
        case HttpStatusCode::PARTIAL_CONTENT: return "Partial Content";
        
        // 3xx Redirection
        case HttpStatusCode::MULTIPLE_CHOICES: return "Multiple Choices";
        case HttpStatusCode::MOVED_PERMANENTLY: return "Moved Permanently";
        case HttpStatusCode::FOUND: return "Found";
        case HttpStatusCode::SEE_OTHER: return "See Other";
        case HttpStatusCode::NOT_MODIFIED: return "Not Modified";
        case HttpStatusCode::TEMPORARY_REDIRECT: return "Temporary Redirect";
        case HttpStatusCode::PERMANENT_REDIRECT: return "Permanent Redirect";
        
        // 4xx Client Error
        case HttpStatusCode::BAD_REQUEST: return "Bad Request";
        case HttpStatusCode::UNAUTHORIZED: return "Unauthorized";
        case HttpStatusCode::FORBIDDEN: return "Forbidden";
        case HttpStatusCode::NOT_FOUND: return "Not Found";
        case HttpStatusCode::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatusCode::NOT_ACCEPTABLE: return "Not Acceptable";
        case HttpStatusCode::REQUEST_TIMEOUT: return "Request Timeout";
        case HttpStatusCode::CONFLICT: return "Conflict";
        case HttpStatusCode::GONE: return "Gone";
        case HttpStatusCode::LENGTH_REQUIRED: return "Length Required";
        case HttpStatusCode::PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HttpStatusCode::URI_TOO_LONG: return "URI Too Long";
        case HttpStatusCode::UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case HttpStatusCode::RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
        case HttpStatusCode::EXPECTATION_FAILED: return "Expectation Failed";
        case HttpStatusCode::UPGRADE_REQUIRED: return "Upgrade Required";
        case HttpStatusCode::TOO_MANY_REQUESTS: return "Too Many Requests";
        
        // 5xx Server Error
        case HttpStatusCode::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatusCode::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatusCode::BAD_GATEWAY: return "Bad Gateway";
        case HttpStatusCode::SERVICE_UNAVAILABLE: return "Service Unavailable";
        case HttpStatusCode::GATEWAY_TIMEOUT: return "Gateway Timeout";
        case HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
        
        default: return "Unknown Status";
    }
}

// ========== Response Implementation ==========

Response::Response()
    : mStatusCode(HttpStatusCode::OK)
    , mVersion(HttpVersion::HTTP_1_1) {
}

Response::Response(HttpStatusCode status_code)
    : mStatusCode(status_code)
    , mVersion(HttpVersion::HTTP_1_1) {
}

Response::Response(fl::u16 status_code)
    : mStatusCode(static_cast<HttpStatusCode>(status_code))
    , mVersion(HttpVersion::HTTP_1_1) {
}

void Response::set_body(fl::span<const fl::u8> data) {
    mBody.assign(data.begin(), data.end());
}

void Response::set_body(const fl::string& text) {
    const char* text_data = text.c_str();
    const fl::u8* u8_data = reinterpret_cast<const fl::u8*>(text_data);
    mBody.assign(u8_data, u8_data + text.size());
}

void Response::set_body(const fl::vector<fl::u8>& data) {
    mBody = data;
}

fl::span<const fl::u8> Response::get_body_span() const {
    return fl::span<const fl::u8>(mBody.data(), mBody.size());
}

fl::string Response::get_body_text() const {
    return fl::string(reinterpret_cast<const char*>(mBody.data()), mBody.size());
}

fl::optional<fl::string> Response::get_content_type() const {
    return get_header("Content-Type");
}

fl::optional<fl::size> Response::get_content_length() const {
    auto header = get_header("Content-Length");
    if (header) {
        // Simple string to integer conversion
        fl::size result = 0;
        for (char c : *header) {
            if (c >= '0' && c <= '9') {
                result = result * 10 + (c - '0');
            } else {
                return fl::nullopt; // Invalid number
            }
        }
        return result;
    }
    return fl::nullopt;
}

fl::optional<fl::string> Response::get_server() const {
    return get_header("Server");
}

fl::optional<fl::string> Response::get_date() const {
    return get_header("Date");
}

fl::optional<fl::string> Response::get_location() const {
    return get_header("Location");
}

bool Response::is_valid() const {
    fl::u16 code = static_cast<fl::u16>(mStatusCode);
    return code >= 100 && code < 600;
}

fl::string Response::get_validation_error() const {
    fl::u16 code = static_cast<fl::u16>(mStatusCode);
    if (code < 100 || code >= 600) {
        return "Invalid HTTP status code";
    }
    return "";
}

// ========== URL Parsing Implementation ==========

int UrlComponents::get_default_port(const fl::string& scheme) {
    if (scheme == "http") return 80;
    if (scheme == "https") return 443;
    if (scheme == "ftp") return 21;
    if (scheme == "ftps") return 990;
    return 80; // Default to HTTP
}

fl::string UrlComponents::to_string() const {
    fl::string result = scheme + "://";
    
    if (!userinfo.empty()) {
        result += userinfo + "@";
    }
    
    result += host;
    
    if (!port_str.empty() && port != get_default_port(scheme)) {
        result += ":" + port_str;
    }
    
    result += path;
    
    if (!query.empty()) {
        result += "?" + query;
    }
    
    if (!fragment.empty()) {
        result += "#" + fragment;
    }
    
    return result;
}

bool UrlComponents::is_valid() const {
    return !scheme.empty() && !host.empty() && !path.empty() && port > 0 && port <= 65535;
}

fl::optional<UrlComponents> parse_url(const fl::string& url) {
    if (url.empty()) {
        return fl::nullopt;
    }
    
    UrlComponents components;
    fl::string remaining = url;
    
    // Parse scheme
    fl::size scheme_pos = remaining.find("://");
    if (scheme_pos == fl::string::npos) {
        return fl::nullopt; // Invalid URL - no scheme
    }
    
    components.scheme = remaining.substr(0, scheme_pos);
    remaining = remaining.substr(scheme_pos + 3, remaining.length());
    
    // Parse fragment first (appears at end)
    fl::size fragment_pos = remaining.find('#');
    if (fragment_pos != fl::string::npos) {
        components.fragment = remaining.substr(fragment_pos + 1, remaining.length());
        remaining = remaining.substr(0, fragment_pos);
    }
    
    // Parse query
    fl::size query_pos = remaining.find('?');
    if (query_pos != fl::string::npos) {
        components.query = remaining.substr(query_pos + 1, remaining.length());
        remaining = remaining.substr(0, query_pos);
    }
    
    // Parse path
    fl::size path_pos = remaining.find('/');
    if (path_pos != fl::string::npos) {
        components.path = remaining.substr(path_pos, remaining.length());
        remaining = remaining.substr(0, path_pos);
    } else {
        components.path = "/"; // Default path
    }
    
    // Parse userinfo
    fl::size at_pos = remaining.find('@');
    if (at_pos != fl::string::npos) {
        components.userinfo = remaining.substr(0, at_pos);
        remaining = remaining.substr(at_pos + 1, remaining.length());
    }
    
    // Parse host and port
    fl::size port_pos = remaining.find(':');
    if (port_pos != fl::string::npos) {
        components.host = remaining.substr(0, port_pos);
        components.port_str = remaining.substr(port_pos + 1, remaining.length());
        
        // Convert port string to integer
        components.port = 0;
        for (char c : components.port_str) {
            if (c >= '0' && c <= '9') {
                components.port = components.port * 10 + (c - '0');
            } else {
                return fl::nullopt; // Invalid port
            }
        }
    } else {
        components.host = remaining;
        components.port = UrlComponents::get_default_port(components.scheme);
    }
    
    // Validate components
    if (!components.is_valid()) {
        return fl::nullopt;
    }
    
    return components;
}

// ========== URL Encoding/Decoding ==========

fl::string url_encode(const fl::string& str) {
    fl::string result;
    result.reserve(str.length() * 2); // Reserve space for worst case
    
    for (char c : str) {
        if ((c >= 'A' && c <= 'Z') || 
            (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            result += '%';
            // Convert to hex
            fl::u8 byte = static_cast<fl::u8>(c);
            fl::u8 high = (byte >> 4) & 0x0F;
            fl::u8 low = byte & 0x0F;
            
            result += (high < 10) ? ('0' + high) : ('A' + high - 10);
            result += (low < 10) ? ('0' + low) : ('A' + low - 10);
        }
    }
    
    return result;
}

fl::string url_decode(const fl::string& str) {
    fl::string result;
    result.reserve(str.length());
    
    for (fl::size i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '%' && i + 2 < str.length()) {
            char hex1 = str[i + 1];
            char hex2 = str[i + 2];
            
            // Convert hex to byte
            fl::u8 value = 0;
            if (hex1 >= '0' && hex1 <= '9') value = (hex1 - '0') << 4;
            else if (hex1 >= 'A' && hex1 <= 'F') value = (hex1 - 'A' + 10) << 4;
            else if (hex1 >= 'a' && hex1 <= 'f') value = (hex1 - 'a' + 10) << 4;
            else continue; // Invalid hex
            
            if (hex2 >= '0' && hex2 <= '9') value |= (hex2 - '0');
            else if (hex2 >= 'A' && hex2 <= 'F') value |= (hex2 - 'A' + 10);
            else if (hex2 >= 'a' && hex2 <= 'f') value |= (hex2 - 'a' + 10);
            else continue; // Invalid hex
            
            result += static_cast<char>(value);
            i += 2; // Skip the two hex digits
        } else if (c == '+') {
            result += ' '; // Plus becomes space in form encoding
        } else {
            result += c;
        }
    }
    
    return result;
}

// ========== Query String Utilities ==========

fl::string build_query_string(const fl::vector<fl::pair<fl::string, fl::string>>& params) {
    if (params.empty()) {
        return "";
    }
    
    fl::string result;
    bool first = true;
    
    for (const auto& param : params) {
        if (!first) {
            result += "&";
        }
        first = false;
        
        result += url_encode(param.first);
        result += "=";
        result += url_encode(param.second);
    }
    
    return result;
}

fl::vector<fl::pair<fl::string, fl::string>> parse_query_string(const fl::string& query) {
    fl::vector<fl::pair<fl::string, fl::string>> params;
    
    if (query.empty()) {
        return params;
    }
    
    fl::size start = 0;
    fl::size pos = 0;
    
    while (pos <= query.length()) {
        if (pos == query.length() || query[pos] == '&') {
            if (pos > start) {
                fl::string param = query.substr(start, pos);
                fl::size eq_pos = param.find('=');
                
                if (eq_pos != fl::string::npos) {
                    fl::string key = url_decode(param.substr(0, eq_pos));
                    fl::string value = url_decode(param.substr(eq_pos + 1, param.length()));
                    params.emplace_back(key, value);
                } else {
                    // Key without value
                    fl::string key = url_decode(param);
                    params.emplace_back(key, "");
                }
            }
            start = pos + 1;
        }
        ++pos;
    }
    
    return params;
}

// ========== Basic Authentication ==========

fl::string encode_basic_auth(const fl::string& username, const fl::string& password) {
    fl::string credentials = username + ":" + password;
    
    // Simple base64 encoding implementation
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    fl::string result;
    fl::size i = 0;
    
    while (i < credentials.length()) {
        fl::u32 octet_a = i < credentials.length() ? static_cast<fl::u8>(credentials[i++]) : 0;
        fl::u32 octet_b = i < credentials.length() ? static_cast<fl::u8>(credentials[i++]) : 0;
        fl::u32 octet_c = i < credentials.length() ? static_cast<fl::u8>(credentials[i++]) : 0;
        
        fl::u32 triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        result += base64_chars[(triple >> 3 * 6) & 0x3F];
        result += base64_chars[(triple >> 2 * 6) & 0x3F];
        result += base64_chars[(triple >> 1 * 6) & 0x3F];
        result += base64_chars[(triple >> 0 * 6) & 0x3F];
    }
    
    // Add padding
    fl::size padding = (3 - credentials.length() % 3) % 3;
    for (fl::size p = 0; p < padding; ++p) {
        result[result.length() - 1 - p] = '=';
    }
    
    return "Basic " + result;
}

// ========== HTTP Date Functions ==========

fl::string format_http_date(fl::u64 timestamp) {
    // Simple HTTP date formatting (RFC 7231)
    // For embedded systems, we'll use a simplified format
    // Real implementation would use proper time formatting
    (void)timestamp; // Suppress unused parameter warning
    return "Mon, 01 Jan 2024 00:00:00 GMT"; // Placeholder
}

fl::optional<fl::u64> parse_http_date(const fl::string& date_str) {
    // Simple HTTP date parsing
    // Real implementation would parse RFC 7231 format
    (void)date_str; // Suppress unused parameter warning
    return fl::nullopt; // Placeholder
}

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
