#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/types.h"
#include "fl/net/http/transport.h"
#include "fl/function.h"
#include "fl/shared_ptr.h"
#include "fl/unique_ptr.h"
#include "fl/mutex.h"
#include "fl/vector.h"
#include "fl/pair.h"
#include "fl/future.h"

namespace fl {

// ========== Simple HTTP Functions (Level 1) ==========

/// Simple HTTP GET request
/// @param url the URL to request
/// @returns future that resolves to response
fl::future<Response> http_get(const fl::string& url);

/// Simple HTTP POST request with data
/// @param url the URL to post to
/// @param data the data to send
/// @param content_type the content type (default: application/octet-stream)
/// @returns future that resolves to response
fl::future<Response> http_post(const fl::string& url, 
                               fl::span<const fl::u8> data,
                               const fl::string& content_type = "application/octet-stream");

/// Simple HTTP POST request with text
/// @param url the URL to post to  
/// @param text the text data to send
/// @param content_type the content type (default: text/plain)
/// @returns future that resolves to response
fl::future<Response> http_post(const fl::string& url,
                               const fl::string& text,
                               const fl::string& content_type = "text/plain");

/// Simple HTTP POST request with JSON
/// @param url the URL to post to
/// @param json the JSON data to send
/// @returns future that resolves to response
fl::future<Response> http_post_json(const fl::string& url, const fl::string& json);

/// Simple HTTP PUT request
fl::future<Response> http_put(const fl::string& url, 
                              fl::span<const fl::u8> data,
                              const fl::string& content_type = "application/octet-stream");

/// Simple HTTP DELETE request
fl::future<Response> http_delete(const fl::string& url);

// ========== HTTP Client Class (Level 2) ==========

class RequestBuilder; // Forward declaration

/// HTTP Client with configuration and session management
class HttpClient {
public:
    /// Client configuration options
    struct Config {
        fl::u32 timeout_ms;                          ///< Request timeout
        fl::u32 connect_timeout_ms;                   ///< Connection timeout
        fl::size max_redirects;                          ///< Maximum redirect follows
        bool follow_redirects;                        ///< Whether to follow redirects
        fl::string user_agent;              ///< User agent string
        fl::vector<fl::pair<fl::string, fl::string>> default_headers; ///< Default headers
        bool verify_ssl;                              ///< Verify SSL certificates
        fl::string ca_bundle_path;                           ///< CA bundle for SSL verification
        fl::size max_response_size;               ///< Max response size (10MB)
        fl::size buffer_size;                         ///< Internal buffer size
        bool enable_compression;                      ///< Accept gzip compression
        bool enable_keepalive;                        ///< Use HTTP keep-alive
        fl::u32 keepalive_timeout_ms;                ///< Keep-alive timeout
        
        // Constructor with defaults
        Config() 
            : timeout_ms(10000)
            , connect_timeout_ms(5000)
            , max_redirects(5)
            , follow_redirects(true)
            , user_agent("FastLED/1.0")
            , verify_ssl(true)
            , max_response_size(10485760)
            , buffer_size(8192)
            , enable_compression(true)
            , enable_keepalive(true)
            , keepalive_timeout_ms(30000) {}
    };
    
    /// Create HTTP client with default configuration
    HttpClient() = default;
    
    /// Create HTTP client with custom configuration
    explicit HttpClient(const Config& config);
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::shared_ptr<Transport> transport, const Config& config = {});
    
    /// Destructor
    ~HttpClient();
    
    // ========== SIMPLE REQUEST METHODS ==========
    
    /// HTTP GET request
    /// @param url the URL to request
    /// @returns future that resolves to response
    fl::future<Response> get(const fl::string& url);
    
    /// HTTP POST request with data
    fl::future<Response> post(const fl::string& url, fl::span<const fl::u8> data,
                              const fl::string& content_type = "application/octet-stream");
    
    /// HTTP POST request with text
    fl::future<Response> post(const fl::string& url, const fl::string& text,
                              const fl::string& content_type = "text/plain");
    
    /// HTTP PUT request
    fl::future<Response> put(const fl::string& url, fl::span<const fl::u8> data,
                             const fl::string& content_type = "application/octet-stream");
    
    /// HTTP DELETE request  
    fl::future<Response> delete_(const fl::string& url);  // delete is keyword
    
    /// HTTP HEAD request
    fl::future<Response> head(const fl::string& url);
    
    /// HTTP OPTIONS request
    fl::future<Response> options(const fl::string& url);
    
    /// HTTP PATCH request
    fl::future<Response> patch(const fl::string& url, fl::span<const fl::u8> data,
                               const fl::string& content_type = "application/octet-stream");
    
    // ========== ADVANCED REQUEST METHODS ==========
    
    /// Send custom request
    fl::future<Response> send(const Request& request);
    
    /// Send request asynchronously (returns future immediately)
    fl::future<Response> send_async(const Request& request);
    
    /// Download file to local storage
    fl::future<bool> download_file(const fl::string& url, const fl::string& local_path);
    
    /// Upload file from local storage
    fl::future<Response> upload_file(const fl::string& url, const fl::string& file_path,
                                     const fl::string& field_name = "file");
    
    /// Stream download with custom processor
    fl::future<bool> stream_download(const fl::string& url, 
                                    fl::function<bool(fl::span<const fl::u8>)> data_processor);
    
    /// Stream upload with custom provider
    fl::future<Response> stream_upload(const fl::string& url,
                                      fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider,
                                      const fl::string& content_type = "application/octet-stream");
    
    // ========== CONFIGURATION ==========
    
    /// Set request timeout
    void set_timeout(fl::u32 timeout_ms);
    fl::u32 get_timeout() const;
    
    /// Set connection timeout
    void set_connect_timeout(fl::u32 timeout_ms);
    fl::u32 get_connect_timeout() const;
    
    /// Configure redirect handling
    void set_follow_redirects(bool follow);
    void set_max_redirects(fl::size max_redirects);
    bool get_follow_redirects() const;
    fl::size get_max_redirects() const;
    
    /// Set user agent
    void set_user_agent(const fl::string& user_agent);
    const fl::string& get_user_agent() const;
    
    /// Header management
    void set_header(const fl::string& name, const fl::string& value);
    void set_headers(const fl::vector<fl::pair<fl::string, fl::string>>& headers);
    void remove_header(const fl::string& name);
    void clear_headers();
    fl::span<const fl::pair<fl::string, fl::string>> get_headers() const;
    
    /// SSL/TLS configuration
    void set_verify_ssl(bool verify);
    void set_ca_bundle_path(const fl::string& path);
    void set_client_certificate(const fl::string& cert_path, const fl::string& key_path);
    bool get_verify_ssl() const;
    
    /// Response size limits
    void set_max_response_size(fl::size max_size);
    fl::size get_max_response_size() const;
    
    /// Buffer configuration
    void set_buffer_size(fl::size buffer_size);
    fl::size get_buffer_size() const;
    
    /// Compression settings
    void set_enable_compression(bool enable);
    bool get_enable_compression() const;
    
    /// Keep-alive settings
    void set_enable_keepalive(bool enable);
    void set_keepalive_timeout(fl::u32 timeout_ms);
    bool get_enable_keepalive() const;
    fl::u32 get_keepalive_timeout() const;
    
    // ========== AUTHENTICATION ==========
    
    /// Set basic authentication
    void set_basic_auth(const fl::string& username, const fl::string& password);
    
    /// Set bearer token authentication
    void set_bearer_token(const fl::string& token);
    
    /// Set API key authentication
    void set_api_key(const fl::string& key, const fl::string& header_name = "X-API-Key");
    
    /// Set custom authorization header
    void set_authorization(const fl::string& auth_header);
    
    /// Clear authentication
    void clear_auth();
    
    // ========== SESSION MANAGEMENT ==========
    
    /// Cookie management
    void set_cookie(const fl::string& name, const fl::string& value);
    void set_cookies(const fl::vector<fl::pair<fl::string, fl::string>>& cookies);
    void clear_cookies();
    fl::span<const fl::pair<fl::string, fl::string>> get_cookies() const;
    
    /// Session persistence
    void enable_cookie_jar(bool enable = true);
    bool is_cookie_jar_enabled() const;
    
    // ========== REQUEST BUILDING ==========
    
    /// Create request builder for advanced requests
    RequestBuilder request(const fl::string& method, const fl::string& url);
    RequestBuilder get_request(const fl::string& url);
    RequestBuilder post_request(const fl::string& url);
    RequestBuilder put_request(const fl::string& url);
    RequestBuilder delete_request(const fl::string& url);
    RequestBuilder head_request(const fl::string& url);
    RequestBuilder options_request(const fl::string& url);
    RequestBuilder patch_request(const fl::string& url);
    
    // ========== STATISTICS ==========
    
    /// Client statistics
    struct Stats {
        fl::size total_requests;
        fl::size successful_requests;
        fl::size failed_requests;
        fl::size redirects_followed;
        fl::size bytes_sent;
        fl::size bytes_received;
        fl::u32 average_response_time_ms;
        fl::u32 last_request_time_ms;
    };
    Stats get_stats() const;
    void reset_stats();
    
    /// Connection statistics
    fl::size get_active_connections() const;
    fl::size get_total_connections() const;
    
    // ========== ERROR HANDLING ==========
    
    /// Get last error information
    enum class ErrorCode {
        SUCCESS,
        NETWORK_ERROR,
        TIMEOUT,
        SSL_ERROR,
        INVALID_URL,
        INVALID_RESPONSE,
        TOO_MANY_REDIRECTS,
        RESPONSE_TOO_LARGE,
        UNKNOWN_ERROR
    };
    
    ErrorCode get_last_error() const;
    fl::string get_last_error_message() const;
    
    /// Error handling callback
    using ErrorHandler = fl::function<void(ErrorCode, const fl::string&)>;
    void set_error_handler(ErrorHandler handler);
    
    // ========== FACTORY METHODS ==========
    
    /// Create client with specific transport type
    static fl::shared_ptr<HttpClient> create_with_tcp_transport();
    static fl::shared_ptr<HttpClient> create_with_tls_transport();
    static fl::shared_ptr<HttpClient> create_with_transport(fl::shared_ptr<Transport> transport);
    
    /// Create clients for common use cases
    static fl::shared_ptr<HttpClient> create_simple_client();
    static fl::shared_ptr<HttpClient> create_secure_client(bool verify_ssl = true);
    static fl::shared_ptr<HttpClient> create_api_client(const fl::string& base_url, const fl::string& api_key);
    
private:
    fl::shared_ptr<Transport> mTransport;
    Config mConfig;
    fl::vector<fl::pair<fl::string, fl::string>> mDefaultHeaders;
    fl::vector<fl::pair<fl::string, fl::string>> mCookies;
    bool mCookieJarEnabled = false;
    
    // Authentication state
    fl::optional<fl::string> mAuthHeader;
    
    // Statistics
    mutable fl::mutex mStatsMutex;
    Stats mStats;
    
    // Error handling
    ErrorCode mLastError = ErrorCode::SUCCESS;
    fl::string mLastErrorMessage;
    ErrorHandler mErrorHandler;
    
    // Internal implementation
    fl::future<Response> send_internal(const Request& request);
    Request build_request(const fl::string& method, const fl::string& url) const;
    void apply_config_to_request(Request& request) const;
    void apply_authentication(Request& request) const;
    void apply_cookies(Request& request) const;
    void process_response_cookies(const Response& response);
    fl::future<Response> handle_redirects(const Request& request, const Response& response);
    void update_stats(const Request& request, const fl::optional<Response>& response, fl::u32 duration_ms);
    void set_error(ErrorCode error, const fl::string& message);
    
    // Friends
    friend class RequestBuilder;
};

// ========== Request Builder (Level 3) ==========

/// Request builder for constructing complex HTTP requests
class RequestBuilder {
public:
    /// Create request builder
    RequestBuilder(const fl::string& method, const fl::string& url, HttpClient* client = nullptr);
    
    /// HTTP method and URL
    RequestBuilder& method(const fl::string& method);
    RequestBuilder& url(const fl::string& url);
    
    /// Headers
    RequestBuilder& header(const fl::string& name, const fl::string& value);
    RequestBuilder& headers(const fl::vector<fl::pair<fl::string, fl::string>>& headers);
    RequestBuilder& user_agent(const fl::string& agent);
    RequestBuilder& content_type(const fl::string& type);
    RequestBuilder& accept(const fl::string& types);
    RequestBuilder& authorization(const fl::string& auth);
    
    /// Query parameters
    RequestBuilder& query(const fl::string& name, const fl::string& value);
    RequestBuilder& query(const fl::vector<fl::pair<fl::string, fl::string>>& params);
    
    /// Request body
    RequestBuilder& body(fl::span<const fl::u8> data);
    RequestBuilder& body(const fl::string& text);
    RequestBuilder& json(const fl::string& json_data);
    RequestBuilder& form_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields);
    RequestBuilder& multipart_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields);
    RequestBuilder& file_upload(const fl::string& field_name, const fl::string& file_path);
    
    /// Authentication shortcuts
    RequestBuilder& basic_auth(const fl::string& username, const fl::string& password);
    RequestBuilder& bearer_token(const fl::string& token);
    RequestBuilder& api_key(const fl::string& key, const fl::string& header_name = "X-API-Key");
    
    /// Request options
    RequestBuilder& timeout(fl::u32 timeout_ms);
    RequestBuilder& follow_redirects(bool follow = true);
    RequestBuilder& max_redirects(fl::size max_redirects);
    RequestBuilder& verify_ssl(bool verify = true);
    
    /// Build and send request
    Request build() const;
    fl::future<Response> send();
    fl::future<Response> send_async();
    
    /// Streaming operations
    fl::future<bool> download_to_file(const fl::string& file_path);
    fl::future<bool> stream_download(fl::function<bool(fl::span<const fl::u8>)> data_processor);
    fl::future<Response> stream_upload(fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider);
    
private:
    HttpClient* mClient;
    fl::string mMethod;
    fl::string mUrl;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::vector<fl::pair<fl::string, fl::string>> mQueryParams;
    fl::vector<fl::u8> mBody;
    fl::optional<fl::string> mBodyContentType;
    fl::u32 mTimeout = 0;  // 0 = use client default
    fl::optional<bool> mFollowRedirects;
    fl::optional<fl::size> mMaxRedirects;
    fl::optional<bool> mVerifySSL;
    
    // Internal helpers
    fl::string build_query_string() const;
    fl::string build_form_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields) const;
    fl::string build_multipart_data(const fl::vector<fl::pair<fl::string, fl::string>>& fields) const;
    fl::string encode_basic_auth(const fl::string& username, const fl::string& password) const;
    fl::string url_encode(const fl::string& str) const;
};

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
