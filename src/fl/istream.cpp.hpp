#include "istream.h"
#include "fl/math.h"
#include "fl/compiler_control.h"
//#include <stddef.h>
// <cstdlib> not available on AVR platforms like Arduino UNO
// We implement custom integer parsing functions instead

#include "fl/math_macros.h"

namespace fl {

namespace {
    // Helper function to check if a character is a digit
    inline bool isDigit(char c) {
        return c >= '0' && c <= '9';
    }
    
    // Helper function to check if a character is whitespace
    inline bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }
    
    // Custom integer parsing function for signed 32-bit integers
    bool parse_i32(const char* str, fl::i32& result) {
        if (!str) return false;
        
        // Skip leading whitespace
        while (*str && isSpace(*str)) {
            str++;
        }
        
        if (*str == '\0') return false;
        
        bool negative = false;
        if (*str == '-') {
            negative = true;
            str++;
        } else if (*str == '+') {
            str++;
        }
        
        if (*str == '\0' || !isDigit(*str)) return false;
        
        fl::u32 value = 0;
        const fl::u32 max_div_10 = 214748364U; // INT32_MAX / 10
        const fl::u32 max_mod_10 = 7U;         // INT32_MAX % 10
        const fl::u32 max_neg_div_10 = 214748364U; // -INT32_MIN / 10  
        const fl::u32 max_neg_mod_10 = 8U;         // -INT32_MIN % 10
        
        while (*str && isDigit(*str)) {
            fl::u32 digit = *str - '0';
            
            // Check for overflow
            if (negative) {
                if (value > max_neg_div_10 || (value == max_neg_div_10 && digit > max_neg_mod_10)) {
                    return false; // Overflow
                }
            } else {
                if (value > max_div_10 || (value == max_div_10 && digit > max_mod_10)) {
                    return false; // Overflow
                }
            }
            
            value = value * 10 + digit;
            str++;
        }
        
        // Check if we stopped at a non-digit character (should be end of string for valid parse)
        if (*str != '\0') return false;
        
        if (negative) {
            result = -static_cast<fl::i32>(value);
        } else {
            result = static_cast<fl::i32>(value);
        }
        
        return true;
    }
    
    // Custom integer parsing function for unsigned 32-bit integers
    bool parse_u32(const char* str, fl::u32& result) {
        if (!str) return false;
        
        // Skip leading whitespace
        while (*str && isSpace(*str)) {
            str++;
        }
        
        if (*str == '\0') return false;
        
        // Optional '+' sign (no '-' for unsigned)
        if (*str == '+') {
            str++;
        } else if (*str == '-') {
            return false; // Negative not allowed for unsigned
        }
        
        if (*str == '\0' || !isDigit(*str)) return false;
        
        fl::u32 value = 0;
        const fl::u32 max_div_10 = 429496729U; // UINT32_MAX / 10
        const fl::u32 max_mod_10 = 5U;         // UINT32_MAX % 10
        
        while (*str && isDigit(*str)) {
            fl::u32 digit = *str - '0';
            
            // Check for overflow
            if (value > max_div_10 || (value == max_div_10 && digit > max_mod_10)) {
                return false; // Overflow
            }
            
            value = value * 10 + digit;
            str++;
        }
        
        // Check if we stopped at a non-digit character (should be end of string for valid parse)
        if (*str != '\0') return false;
        
        result = value;
        return true;
    }
}

FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
// Global cin instance (stub that conditionally delegates)
istream cin;

// Function to get singleton instance of istream_real (for better linker elimination)
istream_real& cin_real() {
    // Local static instance - only constructed when first called
    // This allows the linker to eliminate it if never referenced
    static istream_real instance;
    return instance;
}

bool istream_real::readLine() {
    // If we have no more data available and no buffered data, we're at EOF
    if (pos_ >= buffer_len_ && fl::available() == 0) {
        return false;
    }
    
    // Read characters until newline or no more data
    buffer_len_ = 0;
    while (fl::available() > 0 && buffer_len_ < BUFFER_SIZE - 1) {
        int c = fl::read();
        if (c == -1) break;
        if (c == '\n') break;
        if (c == '\r') continue; // Skip carriage return
        buffer_[buffer_len_++] = static_cast<char>(c);
    }
    
    // Null terminate the buffer
    buffer_[buffer_len_] = '\0';
    pos_ = 0;
    return true;
}

void istream_real::skipWhitespace() {
    while (pos_ < buffer_len_ && 
           (buffer_[pos_] == ' ' || buffer_[pos_] == '\t' || 
            buffer_[pos_] == '\n' || buffer_[pos_] == '\r')) {
        pos_++;
    }
    
    // If we've consumed all buffer and there's more input, read more
    if (pos_ >= buffer_len_ && fl::available() > 0) {
        if (readLine()) {
            skipWhitespace();
        }
    }
}

bool istream_real::readToken(string& token) {
    skipWhitespace();
    
    if (pos_ >= buffer_len_ && fl::available() == 0) {
        failed_ = true;
        return false;
    }
    
    // If buffer is empty but data is available, read a line
    if (pos_ >= buffer_len_ && fl::available() > 0) {
        if (!readLine()) {
            failed_ = true;
            return false;
        }
        skipWhitespace();
    }
    
    // Read until whitespace or end of buffer
    token.clear();
    while (pos_ < buffer_len_ && 
           buffer_[pos_] != ' ' && buffer_[pos_] != '\t' && 
           buffer_[pos_] != '\n' && buffer_[pos_] != '\r') {
        // Explicitly append as a character string to avoid fl::u8->number conversion
        char ch[2] = {buffer_[pos_], '\0'};
        token.append(ch, 1);
        pos_++;
    }
    
    return !token.empty();
}

istream_real& istream_real::operator>>(string& str) {
    if (!readToken(str)) {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(char& c) {
    skipWhitespace();
    
    if (pos_ >= buffer_len_ && fl::available() > 0) {
        if (!readLine()) {
            failed_ = true;
            return *this;
        }
        skipWhitespace();
    }
    
    if (pos_ < buffer_len_) {
        c = buffer_[pos_];
        pos_++;
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(fl::i8& n) {
    string token;
    if (readToken(token)) {
        fl::i32 temp;
        if (parse_i32(token.c_str(), temp) && temp >= -128 && temp <= 127) {
            n = static_cast<fl::i8>(temp);
        } else {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(fl::u8& n) {
    string token;
    if (readToken(token)) {
        fl::u32 temp;
        if (parse_u32(token.c_str(), temp) && temp <= 255) {
            n = static_cast<fl::u8>(temp);
        } else {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(fl::i16& n) {
    string token;
    if (readToken(token)) {
        fl::i32 temp;
        if (parse_i32(token.c_str(), temp) && temp >= -32768 && temp <= 32767) {
            n = static_cast<fl::i16>(temp);
        } else {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

// u16 operator>> removed - now handled by template in header

istream_real& istream_real::operator>>(fl::i32& n) {
    string token;
    if (readToken(token)) {
        if (!parse_i32(token.c_str(), n)) {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(fl::u32& n) {
    string token;
    if (readToken(token)) {
        if (!parse_u32(token.c_str(), n)) {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(float& f) {
    string token;
    if (readToken(token)) {
        // Use the existing toFloat() method
        f = token.toFloat();
        // Check if parsing was successful by checking for valid float
        // toFloat() returns 0.0f for invalid input, but we need to distinguish 
        // between actual 0.0f and parse failure
        if (ALMOST_EQUAL_FLOAT(f, 0.0f) && token != "0" && token != "0.0" && token != "0." && token.find("0") != 0) {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

istream_real& istream_real::operator>>(double& d) {
    string token;
    if (readToken(token)) {
        // Use the existing toFloat() method
        float f = token.toFloat();
        d = static_cast<double>(f);
        // Check if parsing was successful (same logic as float)
        if (ALMOST_EQUAL_FLOAT(f, 0.0f) && token != "0" && token != "0.0" && token != "0." && token.find("0") != 0) {
            failed_ = true;
        }
    } else {
        failed_ = true;
    }
    return *this;
}

// fl::size operator>> removed - now handled by template in header

istream_real& istream_real::getline(string& str) {
    str.clear();
    
    // Read from current buffer position to end
    while (pos_ < buffer_len_) {
        if (buffer_[pos_] == '\n') {
            pos_++; // Consume the newline
            return *this;
        }
        // Explicitly append as a character string to avoid fl::u8->number conversion
        char ch[2] = {buffer_[pos_], '\0'};
        str.append(ch, 1);
        pos_++;
    }
    
    // If we need more data, read a new line
    if (fl::available() > 0) {
        // Read more characters until newline
        while (fl::available() > 0) {
            int c = fl::read();
            if (c == -1) break;
            if (c == '\n') break;
            if (c == '\r') continue; // Skip carriage return
            // Explicitly append as a character string to avoid fl::u8->number conversion
            char ch[2] = {static_cast<char>(c), '\0'};
            str.append(ch, 1);
        }
    }
    
    return *this;
}

int istream_real::get() {
    if (pos_ >= buffer_len_ && fl::available() > 0) {
        if (!readLine()) {
            return -1;
        }
    }
    
    if (pos_ < buffer_len_) {
        return static_cast<int>(static_cast<unsigned char>(buffer_[pos_++]));
    }
    
    // Try to read directly from input if buffer is empty
    return fl::read();
}

istream_real& istream_real::putback(char c) {
    if (pos_ > 0) {
        pos_--;
        buffer_[pos_] = c;
    } else {
        // Insert at beginning of buffer - shift existing data
        if (buffer_len_ < BUFFER_SIZE - 1) {
            for (fl::size i = buffer_len_; i > 0; --i) {
                buffer_[i] = buffer_[i-1];
            }
            buffer_[0] = c;
            buffer_len_++;
            buffer_[buffer_len_] = '\0';
        }
    }
    return *this;
}

int istream_real::peek() {
    if (pos_ >= buffer_len_ && fl::available() > 0) {
        if (!readLine()) {
            return -1;
        }
    }
    
    if (pos_ < buffer_len_) {
        return static_cast<int>(static_cast<unsigned char>(buffer_[pos_]));
    }
    
    return -1;
}

} // namespace fl
