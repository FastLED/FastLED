// allow-include-after-namespace

#pragma once

// Forward declarations to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
#ifndef FL_IO_H_INCLUDED
namespace fl {
    int available();
    int read();
#ifdef FASTLED_TESTING
    template<typename T> class function;  // Forward declare
    void clear_io_handlers();
    void inject_available_handler(const function<int()>& handler);
    void inject_read_handler(const function<int()>& handler);
#endif
}
#endif

#include "fl/str.h"
#include "fl/type_traits.h"
#include "fl/sketch_macros.h"

#include "fl/int.h"

namespace fl {

class istream_real {
private:
    static const fl::size BUFFER_SIZE = 256;
    char buffer_[BUFFER_SIZE];
    fl::size buffer_len_ = 0;
    fl::size pos_ = 0;
    bool failed_ = false;
    
    // Helper to read a line from input
    bool readLine();
    
    // Helper to skip whitespace
    void skipWhitespace();
    
    // Helper to read until whitespace or end
    bool readToken(string& token);
    
public:
    istream_real() = default;
    
    // Check if stream is in good state
    bool good() const { return !failed_; }
    bool fail() const { return failed_; }
    bool eof() const { return pos_ >= buffer_len_ && fl::available() == 0; }
    
    // Clear error state
    void clear() { failed_ = false; }
    
    // Stream input operators
    istream_real& operator>>(string& str);
    istream_real& operator>>(char& c);
    istream_real& operator>>(fl::i8& n);
    istream_real& operator>>(fl::u8& n);
    istream_real& operator>>(fl::i16& n);
    istream_real& operator>>(fl::i32& n);
    istream_real& operator>>(fl::u32& n);
    istream_real& operator>>(float& f);
    istream_real& operator>>(double& d);
    
    // Unified handler for fl:: namespace size-like unsigned integer types to avoid conflicts
    // This only handles fl::size and fl::u16 from the fl:: namespace
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::size>::value ||
        fl::is_same<T, fl::u16>::value,
        istream_real&
    >::type operator>>(T& n);
    
    // Get a line from input
    istream_real& getline(string& str);
    
    // Get next character
    int get();
    
    // Put back a character
    istream_real& putback(char c);
    
    // Peek at next character without consuming it
    int peek();
};

// Function to get singleton instance of istream_real (for better linker elimination)
istream_real& cin_real();

// Stub istream class that conditionally delegates to istream_real
class istream {
private:
#if SKETCH_HAS_LOTS_OF_MEMORY
    istream_real real_stream_;
#endif
    
public:
    istream() = default;
    
    // Check if stream is in good state
    bool good() const { 
#if SKETCH_HAS_LOTS_OF_MEMORY
        return real_stream_.good();
#else
        return true; // Always good on memory-constrained platforms
#endif
    }
    
    bool fail() const { 
#if SKETCH_HAS_LOTS_OF_MEMORY
        return real_stream_.fail();
#else
        return false; // Never fail on memory-constrained platforms
#endif
    }
    
    bool eof() const { 
#if SKETCH_HAS_LOTS_OF_MEMORY
        return real_stream_.eof();
#else
        return true; // Always EOF on memory-constrained platforms
#endif
    }
    
    // Clear error state
    void clear() { 
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_.clear();
#endif
    }
    
    // Stream input operators
    istream& operator>>(string& str) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> str;
#else
        // No-op on memory-constrained platforms
        str.clear();
#endif
        return *this;
    }
    
    istream& operator>>(char& c) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> c;
#else
        // No-op on memory-constrained platforms
        c = '\0';
#endif
        return *this;
    }
    
    istream& operator>>(fl::i8& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    istream& operator>>(fl::u8& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    istream& operator>>(fl::i16& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    istream& operator>>(fl::i32& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    istream& operator>>(fl::u32& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    istream& operator>>(float& f) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> f;
#else
        // No-op on memory-constrained platforms
        f = 0.0f;
#endif
        return *this;
    }
    
    istream& operator>>(double& d) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> d;
#else
        // No-op on memory-constrained platforms
        d = 0.0;
#endif
        return *this;
    }
    
    // Unified handler for fl:: namespace size-like unsigned integer types to avoid conflicts
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::size>::value ||
        fl::is_same<T, fl::u16>::value,
        istream&
    >::type operator>>(T& n) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_ >> n;
#else
        // No-op on memory-constrained platforms
        n = 0;
#endif
        return *this;
    }
    
    // Get a line from input
    istream& getline(string& str) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_.getline(str);
#else
        // No-op on memory-constrained platforms
        str.clear();
#endif
        return *this;
    }
    
    // Get next character
    int get() {
#if SKETCH_HAS_LOTS_OF_MEMORY
        return real_stream_.get();
#else
        // No-op on memory-constrained platforms
        return -1;
#endif
    }
    
    // Put back a character
    istream& putback(char c) {
#if SKETCH_HAS_LOTS_OF_MEMORY
        real_stream_.putback(c);
#endif
        return *this;
    }
    
    // Peek at next character without consuming it
    int peek() {
#if SKETCH_HAS_LOTS_OF_MEMORY
        return real_stream_.peek();
#else
        // No-op on memory-constrained platforms
        return -1;
#endif
    }
};

// Global cin instance for input (now uses the stub)
extern istream cin;

// Template implementation for istream_real
template<typename T>
typename fl::enable_if<
    fl::is_same<T, fl::size>::value ||
    fl::is_same<T, fl::u16>::value,
    istream_real&
>::type istream_real::operator>>(T& n) {
    // Use existing fl::u32 parsing logic for both fl::size and fl::u16
    // since they're both unsigned integer types that fit in fl::u32
    fl::u32 temp;
    (*this) >> temp;
    n = static_cast<T>(temp);
    return *this;
}

} // namespace fl
