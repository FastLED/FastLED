# Remote Stream I/O Investigation

## Current State

### Problems
1. **Manual I/O boilerplate** - Users must manually:
   - Read from stream (Serial, Network, etc.)
   - Parse JSON
   - Call processRpc()
   - Build response JSON
   - Write response back
   - Call tick() separately
   - Call getResults() and manually print them

2. **No unified stream handling** - Every example reimplements the same pattern:
   ```cpp
   if (Serial.available()) {
       fl::string jsonRpc = readSerialJson();
       // 40+ lines of manual parsing/response handling
   }
   remote.tick(millis());
   auto results = remote.getResults();
   for (const auto& r : results) {
       fl::Remote::printJson(r.to_json());
   }
   ```

3. **Inconsistent output** - Immediate responses vs scheduled results handled differently

### Current Usage Pattern
```cpp
// Remote.ino example (113-183)
void loop() {
    remote.tick(millis());                    // 1. Process scheduled calls

    auto results = remote.getResults();       // 2. Get scheduled results
    for (const auto& r : results) {
        fl::Remote::printJson(r.to_json());   // 3. Print scheduled results
    }

    if (Serial.available()) {                 // 4. Read input
        fl::string jsonRpc = readSerialJson();
        fl::Json doc = fl::Json::parse(jsonRpc);
        auto response = remote.processRpc(fl::Remote::RpcRequest{...});

        fl::Json response = fl::Json::object(); // 5. Build response
        if (response.ok()) {
            response.set("status", "ok");
            // ... 40 lines of response building
        }
        fl::Remote::printJson(response);      // 6. Print immediate response
    }
}
```

## Design Options

### Option A: Single update() Method
```cpp
// Setup
remote.setStream(&Serial);

// Loop
remote.update(millis());  // Does everything: tick + pull + push
```

**Pros:**
- Simplest API for users
- One call does everything

**Cons:**
- Less control over I/O timing
- Harder to test (monolithic)
- Can't separate scheduling from I/O
- May process too much in one call

### Option B: Separate pull/push Methods ⭐ **RECOMMENDED**
```cpp
// Setup (optional - can also use manual processRpc)
remote.attachStream(&Serial);

// Loop
remote.pull();             // Read available input, process RPCs, queue responses
remote.tick(millis());     // Process scheduled calls
remote.push();             // Write queued responses and scheduled results
```

**Pros:**
- Explicit and predictable
- Easy to test each phase independently
- Users can control when I/O happens
- Can skip pull/push if stream not attached
- Backward compatible with manual processRpc()

**Cons:**
- Three method calls instead of one
- Need to maintain response queue

### Option C: Callback-Based
```cpp
remote.onRequest([](Stream& stream) {
    if (stream.available()) {
        return stream.readStringUntil('\n');
    }
    return fl::string();
});

remote.onResponse([](const fl::Json& response) {
    Serial.println(response.to_string());
});
```

**Pros:**
- Maximum flexibility
- Easy to customize I/O behavior

**Cons:**
- Callback complexity
- Still requires user to implement I/O logic
- Not much simpler than current approach

## Proposed Solution: Option B

### Stream Abstraction
```cpp
// Abstract stream interface (compatible with Arduino Stream)
class RpcStream {
public:
    virtual ~RpcStream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    virtual size_t print(const char* str) = 0;
    virtual size_t println(const char* str) = 0;
};

// Adapter for Arduino Serial/Stream
template<typename TStream>
class ArduinoStreamAdapter : public RpcStream {
    TStream* mStream;
public:
    ArduinoStreamAdapter(TStream* stream) : mStream(stream) {}
    int available() override { return mStream->available(); }
    int read() override { return mStream->read(); }
    size_t write(const uint8_t* buffer, size_t size) override {
        return mStream->write(buffer, size);
    }
    size_t print(const char* str) override { return mStream->print(str); }
    size_t println(const char* str) override { return mStream->println(str); }
};
```

### Remote Class API Additions
```cpp
class Remote {
public:
    // Stream attachment (optional - maintains backward compatibility)
    void attachStream(RpcStream* stream);
    void detachStream();
    bool hasStream() const;

    // Stream-based I/O (only work if stream attached)
    size_t pull();   // Read available input, process RPCs, queue responses
    size_t push();   // Write queued responses and scheduled results

    // Convenience method (combines pull + tick + push)
    size_t update(u32 currentTimeMs);

private:
    RpcStream* mStream = nullptr;
    fl::vector<fl::Json> mOutgoingQueue;  // Queued responses to send
    fl::string mInputBuffer;              // Partial line buffer for reading
};
```

### Implementation Details

#### pull() - Read and Process Input
```cpp
size_t Remote::pull() {
    if (!mStream) return 0;

    size_t processed = 0;

    // Read available data into buffer
    while (mStream->available()) {
        int c = mStream->read();
        if (c < 0) break;

        if (c == '\n' || c == '\r') {
            if (!mInputBuffer.empty()) {
                // Process complete line
                fl::Json doc = fl::Json::parse(mInputBuffer);
                auto response = processRpc(fl::Remote::RpcRequest{
                    doc["function"] | fl::string(""),
                    doc["args"],
                    static_cast<u32>(doc["timestamp"] | 0)
                });

                // Build response JSON and queue it
                fl::Json responseJson = buildResponseJson(response);
                mOutgoingQueue.push_back(responseJson);

                mInputBuffer.clear();
                processed++;
            }
        } else {
            mInputBuffer += static_cast<char>(c);
        }
    }

    return processed;
}
```

#### push() - Write Queued Responses
```cpp
size_t Remote::push() {
    if (!mStream) return 0;

    size_t written = 0;

    // Write queued immediate responses
    while (!mOutgoingQueue.empty()) {
        const fl::Json& response = mOutgoingQueue[0];
        printJson(response);  // Uses existing printJson helper
        mOutgoingQueue.erase(mOutgoingQueue.begin());
        written++;
    }

    // Write scheduled results
    auto results = getResults();
    for (const auto& r : results) {
        printJson(r.to_json());
        written++;
    }

    return written;
}
```

#### update() - Convenience Method
```cpp
size_t Remote::update(u32 currentTimeMs) {
    size_t processed = pull();
    size_t executed = tick(currentTimeMs);
    size_t written = push();
    return processed + executed + written;
}
```

### Updated Example Usage
```cpp
void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);

    // Register functions
    remote.bind("setLed", [](int i, int r, int g, int b) {
        leds[i] = CRGB(r, g, b);
    });

    // Attach stream (optional - can still use manual processRpc)
    remote.attachStream(&Serial);
}

void loop() {
    // Option 1: Simple (single call)
    remote.update(millis());

    // Option 2: Explicit (more control)
    // remote.pull();         // Read input, queue responses
    // remote.tick(millis()); // Process scheduled calls
    // remote.push();         // Write all output

    FastLED.show();
    delay(10);
}
```

### Backward Compatibility
- Manual processRpc() still works
- Stream attachment is optional
- Existing examples continue to work unchanged
- New examples can use simpler stream-based API

## Alternative: Template-Based Stream Adapter

Instead of abstract RpcStream, use template to avoid virtual calls:

```cpp
template<typename TStream>
class Remote {
public:
    void attachStream(TStream* stream) { mStream = stream; }

    size_t pull() {
        if (!mStream) return 0;
        // Read from mStream directly (no virtual calls)
        while (mStream->available()) {
            int c = mStream->read();
            // ...
        }
    }

private:
    TStream* mStream = nullptr;
};

// Usage
fl::Remote<decltype(Serial)> remote;
remote.attachStream(&Serial);
```

**Pros:**
- No virtual call overhead
- Type-safe at compile time

**Cons:**
- Remote becomes a template class
- Can't switch streams at runtime
- More complex implementation
- Breaking change to existing code

## Recommendation

**Implement Option B with abstract RpcStream**:
1. Add `RpcStream` abstract interface
2. Add `ArduinoStreamAdapter<T>` template for Arduino compatibility
3. Add `attachStream/detachStream/hasStream` to Remote
4. Add `pull/push/update` methods to Remote
5. Maintain full backward compatibility with manual API

This provides:
- ✅ Simple API for basic use cases (`update()`)
- ✅ Explicit control for advanced use cases (`pull/tick/push`)
- ✅ Testability (can inject mock streams)
- ✅ Backward compatibility (optional feature)
- ✅ No virtual call overhead for non-stream usage
- ✅ Works with any Arduino Stream-compatible class

## Questions for Review

1. Should `pull()` auto-parse JSON or just read lines and let user control parsing?
2. Should `push()` automatically call `printJson()` or let user control formatting?
3. Should we support multiple streams (e.g., Serial + Network)?
4. Should response building be customizable (user-defined formatters)?
5. Should we buffer partial JSON lines or require complete lines per read?
6. Should `update()` exist or force users to call pull/tick/push explicitly?
