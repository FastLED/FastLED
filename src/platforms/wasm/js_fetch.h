#pragma once

#include "fl/string.h"
#include "fl/function.h"
#include "fl/optional.h"
#include "fl/vector.h"
#include "fl/hash_map.h"

namespace fl {

/// Forward declaration for fetch request
class FetchRequest;

/// Simple HTTP response for fetch API
class response {
public:
    /// Default constructor
    response() : mStatusCode(200), mStatusText("OK") {}
    
    /// Constructor with status code and status text
    response(int status_code, const fl::string& status_text) 
        : mStatusCode(status_code), mStatusText(status_text) {}
    
    /// Status code access
    int status() const { return mStatusCode; }
    void set_status(int status_code) { mStatusCode = status_code; }
    
    /// Status text access
    const fl::string& status_text() const { return mStatusText; }
    void set_status_text(const fl::string& status_text) { mStatusText = status_text; }
    
    /// Success check (2xx status codes)
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    
    /// Body content access
    const fl::string& text() const { return mBody; }
    void set_text(const fl::string& body) { mBody = body; }
    
    /// Headers access
    void set_header(const fl::string& name, const fl::string& value) {
        mHeaders[name] = value;
    }
    
    fl::optional<fl::string> get_header(const fl::string& name) const {
        auto it = mHeaders.find(name);
        if (it != mHeaders.end()) {
            return fl::make_optional(it->second);
        }
        return fl::nullopt;
    }
    
    /// Content type convenience
    fl::optional<fl::string> content_type() const {
        return get_header("content-type");
    }
    
private:
    int mStatusCode;
    fl::string mStatusText;
    fl::string mBody;
    fl::hash_map<fl::string, fl::string> mHeaders;
};

/// Simple fetch response callback type
using FetchResponseCallback = fl::function<void(const response&)>;

/// Fetch request object for fluent API
class FetchRequest {
private:
    fl::string mUrl;
    
public:
    explicit FetchRequest(const fl::string& url) : mUrl(url) {}
    
    /// Execute the fetch request and call the response callback
    /// @param callback Function to call with the response object
    void response(const FetchResponseCallback& callback);
};

/// Main fetch object for fluent API
class Fetch {
public:
    /// Create a GET request
    /// @param url The URL to fetch
    /// @returns FetchRequest object for chaining
    FetchRequest get(const fl::string& url) {
        return FetchRequest(url);
    }
};

/// Global fetch object for easy access
extern Fetch fetch;

} // namespace fl 
