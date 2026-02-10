# Remote Callback-Based I/O Design (Evaluated)

## Proposed Design: Request Source + Response Sink

### Core Concept
Remote doesn't handle strings, JSON parsing, or streams. Instead:
1. **Request Source**: User-provided function that returns next fully-parsed, validated `RpcRequest`
2. **Response Sink**: User-provided function that accepts response data for output

### Type Definitions
```cpp
namespace fl {

// Request source: Returns next validated request, or nullopt if none available
using RequestSource = fl::function<fl::optional<RpcRequest>()>;

// Response sink: Accepts response for output (user handles formatting/writing)
using ResponseSink = fl::function<void(const fl::Json& response)>;

}
```

### Remote API
```cpp
class Remote {
public:
    // Callback registration
    void setRequestSource(RequestSource source);
    void setResponseSink(ResponseSink sink);

    // Main update - pulls requests, processes, pushes responses
    size_t update(u32 currentTimeMs);

    // Optional: manual control
    size_t pull();   // Pull from source, process, queue responses
    size_t tick(u32 currentTimeMs);  // Process scheduled (existing)
    size_t push();   // Push queued responses to sink

private:
    RequestSource mRequestSource;
    ResponseSink mResponseSink;
    fl::vector<fl::Json> mOutgoingQueue;  // Responses waiting to be sent
};
```

### Implementation
```cpp
size_t Remote::update(u32 currentTimeMs) {
    size_t processed = 0;

    // 1. Pull requests from source
    if (mRequestSource) {
        while (auto request = mRequestSource()) {
            auto response = processRpc(*request);

            // Build response JSON
            fl::Json responseJson = fl::Json::object();
            if (response.ok()) {
                responseJson.set("status", "ok");
                if (response.value().has_value()) {
                    responseJson.set("result", response.value());
                }
            } else {
                responseJson.set("status", "error");
                responseJson.set("error", errorToString(response.error()));
            }

            // Queue for output
            mOutgoingQueue.push_back(responseJson);
            processed++;
        }
    }

    // 2. Process scheduled calls
    size_t executed = tick(currentTimeMs);

    // 3. Push responses to sink
    if (mResponseSink) {
        // Push immediate responses
        while (!mOutgoingQueue.empty()) {
            mResponseSink(mOutgoingQueue[0]);
            mOutgoingQueue.erase(mOutgoingQueue.begin());
        }

        // Push scheduled results
        auto results = getResults();
        for (const auto& r : results) {
            mResponseSink(r.to_json());
        }
    }

    return processed + executed;
}
```

### User Example: Serial I/O
```cpp
class SerialRpcHandler {
    fl::string inputBuffer;

public:
    // Request source: Read from Serial, parse, validate
    fl::optional<fl::Remote::RpcRequest> getNextRequest() {
        // Read available data
        while (Serial.available()) {
            int c = Serial.read();
            if (c < 0) break;

            if (c == '\n' || c == '\r') {
                if (!inputBuffer.empty()) {
                    // Parse JSON
                    fl::Json doc = fl::Json::parse(inputBuffer);
                    inputBuffer.clear();

                    // Validate and extract fields
                    if (!doc.has_value()) {
                        FL_WARN("Invalid JSON, skipping");
                        continue;  // Skip malformed JSON
                    }

                    fl::string funcName = doc["function"] | fl::string("");
                    if (funcName.empty()) {
                        FL_WARN("Missing function field, skipping");
                        continue;  // Skip invalid request
                    }

                    // Return validated request
                    return fl::Remote::RpcRequest{
                        funcName,
                        doc["args"],
                        static_cast<fl::u32>(doc["timestamp"] | 0)
                    };
                }
            } else {
                inputBuffer += static_cast<char>(c);

                // Safety: Prevent buffer overflow
                if (inputBuffer.size() > 1024) {
                    FL_ERROR("Input buffer overflow, clearing");
                    inputBuffer.clear();
                }
            }
        }

        return fl::nullopt;  // No complete request available
    }

    // Response sink: Format and write to Serial
    void sendResponse(const fl::Json& response) {
        fl::Remote::printJson(response);  // Uses existing helper
    }
};

// Setup
SerialRpcHandler handler;

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);

    remote.bind("setLed", [](int i, int r, int g, int b) {
        leds[i] = CRGB(r, g, b);
    });

    // Attach callbacks
    remote.setRequestSource([&]() { return handler.getNextRequest(); });
    remote.setResponseSink([&](const fl::Json& r) { handler.sendResponse(r); });
}

void loop() {
    remote.update(millis());  // One call does everything
    FastLED.show();
    delay(10);
}
```

## Evaluation

### ✅ Advantages

1. **Separation of Concerns**
   - Remote handles RPC logic only
   - User handles transport/parsing/formatting
   - Clear responsibility boundaries

2. **Error Isolation**
   - Malformed JSON/strings handled in user code
   - Remote only sees validated requests
   - No partial parsing state in Remote

3. **Transport Agnostic**
   - Works with Serial, Network, File, Memory, etc.
   - No stream abstraction needed
   - User controls buffering strategy

4. **Filtering & Security**
   - User can filter/validate before Remote sees request
   - Rate limiting in user code
   - Authentication/authorization at transport layer

5. **Testing**
   - Easy to inject mock request sources
   - Easy to capture responses for verification
   - No I/O mocking needed

6. **Backward Compatible**
   - Callbacks are optional
   - Manual processRpc() still works
   - Existing code unaffected

7. **Simple Remote Implementation**
   - No string parsing code
   - No stream buffering
   - Just coordination logic

### ❌ Disadvantages

1. **User Complexity**
   - User must implement request parsing
   - User must handle buffer management
   - More code in user space

2. **Duplicated Logic**
   - Every user reimplements JSON parsing
   - Every user reimplements line buffering
   - No shared utilities

3. **No Standard Pattern**
   - Each user may implement differently
   - Hard to provide canonical examples
   - Potential for bugs in user code

### 🤔 Questions to Resolve

#### 1. Should Remote provide helper utilities?
```cpp
// Option A: User does everything
remote.setRequestSource([&]() {
    // 20 lines of parsing/validation
    return request;
});

// Option B: Remote provides helpers
remote.setRequestSource([&]() {
    return fl::Remote::parseRequestFromStream(&Serial);  // Built-in helper
});
```

**Recommendation**: Provide helper functions for common cases:
- `fl::Remote::parseRequestFromStream(Stream* stream)` - handles line buffering + JSON parsing
- `fl::Remote::parseRequestFromString(const fl::string& line)` - just JSON parsing
- User can still fully customize if needed

#### 2. Should Response be a struct instead of JSON?
```cpp
// Current: JSON response
using ResponseSink = fl::function<void(const fl::Json& response)>;

// Alternative: Typed response struct
struct Response {
    enum class Status { Ok, Error, Warning };
    Status status;
    fl::Json result;           // Only present for Ok
    fl::string errorMessage;   // Only present for Error
    Error errorCode;           // Only present for Error
};
using ResponseSink = fl::function<void(const Response& response)>;
```

**Recommendation**: Use typed `Response` struct:
- More type-safe than JSON
- Easier to use in user code
- Can still convert to JSON if needed
- Clearer intent

#### 3. Should there be batch operations?
```cpp
// Option A: One at a time
using RequestSource = fl::function<fl::optional<RpcRequest>()>;

// Option B: Batch
using RequestSource = fl::function<fl::vector<RpcRequest>()>;
```

**Recommendation**: Keep one-at-a-time for simplicity. User can implement batching externally if needed.

#### 4. Should source/sink be nullable or always present?
```cpp
// Option A: Nullable (backward compat)
if (mRequestSource) {
    auto req = mRequestSource();
}

// Option B: Always present (use no-op defaults)
mRequestSource = []() { return fl::nullopt; };  // Default
```

**Recommendation**: Nullable with runtime checks. Matches "optional feature" design.

#### 5. Should update() auto-call tick()?
```cpp
// Option A: update() includes tick
size_t update(u32 currentTimeMs) {
    pull();
    tick(currentTimeMs);
    push();
}

// Option B: user calls tick separately
void loop() {
    remote.update();        // Just I/O
    remote.tick(millis());  // Scheduling separate
}
```

**Recommendation**: Option A - `update()` includes `tick()` for simplicity. Advanced users can call `pull/tick/push` separately.

## Final Recommendation

### Implement Callback Design with Helper Utilities

```cpp
class Remote {
public:
    // Response struct (typed, not raw JSON)
    struct Response {
        enum class Status { Ok, Error };
        Status status;
        fl::Json result;           // Present for Ok with return value
        fl::string errorMessage;   // Present for Error
        Error errorCode;           // Present for Error

        fl::Json to_json() const;  // Convert to JSON if needed
    };

    // Callback types
    using RequestSource = fl::function<fl::optional<RpcRequest>()>;
    using ResponseSink = fl::function<void(const Response& response)>;

    // Callback registration (optional - maintains backward compat)
    void setRequestSource(RequestSource source);
    void setResponseSink(ResponseSink sink);
    void clearCallbacks();

    // Main update (includes tick)
    size_t update(u32 currentTimeMs);

    // Manual control (for advanced users)
    size_t pull();   // Pull requests, process, queue responses
    size_t push();   // Push queued responses to sink
    // tick() already exists

    // Helper utilities for common cases
    static fl::optional<RpcRequest> parseRequestFromLine(const fl::string& line);
    static void writeResponseToStream(Stream* stream, const Response& response);

private:
    RequestSource mRequestSource;
    ResponseSink mResponseSink;
    fl::vector<Response> mOutgoingQueue;
};
```

### Minimal User Code (with helpers)
```cpp
fl::string inputBuffer;

void setup() {
    Serial.begin(115200);

    // Use helper for standard Serial I/O
    remote.setRequestSource([&]() {
        // Line buffering (user still controls this)
        while (Serial.available()) {
            int c = Serial.read();
            if (c == '\n' && !inputBuffer.empty()) {
                auto req = fl::Remote::parseRequestFromLine(inputBuffer);
                inputBuffer.clear();
                return req;  // May be nullopt if parse failed
            }
            inputBuffer += (char)c;
        }
        return fl::nullopt;
    });

    remote.setResponseSink([](const auto& r) {
        fl::Remote::writeResponseToStream(&Serial, r);
    });
}

void loop() {
    remote.update(millis());  // One call
}
```

### Custom Transport (no helpers)
```cpp
void setup() {
    // Full control for custom transport
    remote.setRequestSource([&]() {
        // Read from network, apply rate limiting, authentication, etc.
        return customNetworkParser.getNext();
    });

    remote.setResponseSink([&](const auto& response) {
        // Custom formatting, encryption, batching, etc.
        customNetworkWriter.send(response);
    });
}
```

## Summary

The **callback-based design is superior** because:
1. ✅ Remote never handles malformed strings/JSON
2. ✅ User has full control over filtering/validation
3. ✅ Transport-agnostic (Serial, Network, File, Memory)
4. ✅ Easy to test (inject mocks)
5. ✅ Backward compatible (callbacks optional)
6. ✅ Helper utilities reduce user boilerplate for common cases

The key insight: **Remote should coordinate RPC execution, not parse transport data.**

Shall I implement this design?
