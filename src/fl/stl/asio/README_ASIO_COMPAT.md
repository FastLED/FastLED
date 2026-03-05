# FastLED ↔ Asio Compatibility Layer

FastLED's networking types are designed to be portable to/from Boost.Asio code.
This document maps Asio concepts to their FastLED equivalents.

## Type Mapping

| Asio | FastLED | Header |
|------|---------|--------|
| `boost::asio::ip::tcp::socket` | `fl::asio::ip::tcp::socket` | `fl/stl/asio/ip/tcp.h` |
| `boost::asio::ip::tcp::acceptor` | `fl::asio::ip::tcp::acceptor` | `fl/stl/asio/ip/tcp.h` |
| `boost::asio::ip::tcp::endpoint` | `fl::asio::ip::tcp::endpoint` | `fl/stl/asio/ip/tcp.h` |
| `boost::asio::mutable_buffer` | `fl::span<u8>` | `fl/stl/span.h` |
| `boost::asio::const_buffer` | `fl::span<const u8>` | `fl/stl/span.h` |
| `boost::system::error_code` | `fl::asio::error_code` | `fl/stl/asio/error_code.h` |
| `boost::system::errc` | `fl::asio::errc` | `fl/stl/asio/error_code.h` |

## Operation Mapping

| Asio | FastLED |
|------|---------|
| `io_context::run()` | `AsyncManager::update_all()` |
| `io_context::poll()` | `async_run()` |
| `socket.connect(ep)` | `socket.connect(ep)` |
| `socket.close()` | `socket.close()` |
| `socket.shutdown()` | `socket.shutdown()` |
| `read_some(buf, ec)` | `socket.read_some(buf, ec)` |
| `write_some(buf, ec)` | `socket.write_some(buf, ec)` |
| `async_read(s, buf, handler)` | `s.async_read_some(buf, handler)` |
| `async_write(s, buf, handler)` | `s.async_write_some(buf, handler)` |
| `async_connect(s, ep, handler)` | `s.async_connect(ep, handler)` |
| `acceptor.open(ep)` | `acceptor.open(port)` |
| `acceptor.listen(backlog)` | `acceptor.listen(backlog)` |
| `acceptor.accept(peer)` | `acceptor.accept(peer)` |

## Completion Handler Signatures

| Asio Handler Concept | FastLED Equivalent |
|---------------------|-------------------|
| `void(error_code, size_t)` (ReadHandler/WriteHandler) | `fl::asio::io_handler` |
| `void(error_code)` (ConnectHandler/AcceptHandler) | `fl::asio::connect_handler` |

## Error Handling

```cpp
// Asio style — works identically in FastLED
fl::asio::error_code ec;
size_t n = sock.read_some(buffer, ec);
if (ec) {
    // handle error
}

// Check specific errors
if (ec.code == fl::asio::errc::would_block) { ... }
if (ec.code == fl::asio::errc::eof) { ... }

// Convert to/from fl::Error
fl::Error err = ec.to_error();
fl::asio::error_code ec2 = fl::asio::error_code::from_error(err);
```

## What's NOT Ported (By Design)

- **No `io_context`** — FastLED's `AsyncManager` + `Scheduler` fill this role
- **No completion tokens** — Template metaprogramming too heavy for embedded
- **No strand** — FastLED is single-threaded per task
- **No SSL/TLS** — Out of scope
- **No UDP** — TCP only (for now)
- **No scatter/gather I/O** — Single-buffer operations only
