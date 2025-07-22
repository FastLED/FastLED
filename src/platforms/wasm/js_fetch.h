#pragma once

#include "fl/string.h"
#include "fl/function.h"

namespace fl {

/// Forward declaration for fetch request
class FetchRequest;

/// Simple fetch response callback type
using FetchResponseCallback = fl::function<void(const fl::string&)>;

/// Fetch request object for fluent API
class FetchRequest {
private:
    fl::string mUrl;
    
public:
    explicit FetchRequest(const fl::string& url) : mUrl(url) {}
    
    /// Execute the fetch request and call the response callback
    /// @param callback Function to call with the response content
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
