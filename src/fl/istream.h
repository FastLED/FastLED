#pragma once

#include "fl/io.h"
#include "fl/str.h"

#ifndef FASTLED_STRSTREAM_USES_SIZE_T
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
#define FASTLED_STRSTREAM_USES_SIZE_T 0
#else
#define FASTLED_STRSTREAM_USES_SIZE_T 1
#endif
#endif

namespace fl {

class istream {
private:
    static const size_t BUFFER_SIZE = 256;
    char buffer_[BUFFER_SIZE];
    size_t buffer_len_ = 0;
    size_t pos_ = 0;
    bool failed_ = false;
    
    // Helper to read a line from input
    bool readLine();
    
    // Helper to skip whitespace
    void skipWhitespace();
    
    // Helper to read until whitespace or end
    bool readToken(string& token);
    
public:
    istream() = default;
    
    // Check if stream is in good state
    bool good() const { return !failed_; }
    bool fail() const { return failed_; }
    bool eof() const { return pos_ >= buffer_len_ && fl::available() == 0; }
    
    // Clear error state
    void clear() { failed_ = false; }
    
    // Stream input operators
    istream& operator>>(string& str);
    istream& operator>>(char& c);
    istream& operator>>(int8_t& n);
    istream& operator>>(uint8_t& n);
    istream& operator>>(int16_t& n);
    istream& operator>>(uint16_t& n);
    istream& operator>>(int32_t& n);
    istream& operator>>(uint32_t& n);
    istream& operator>>(float& f);
    istream& operator>>(double& d);
    
#if FASTLED_STRSTREAM_USES_SIZE_T
    istream& operator>>(size_t& n);
#endif
    
    // Get a line from input
    istream& getline(string& str);
    
    // Get next character
    int get();
    
    // Put back a character
    istream& putback(char c);
    
    // Peek at next character without consuming it
    int peek();
};

// Global cin instance for input
extern istream cin;

} // namespace fl
