#include "fl/json_compact.h"
#include "fl/string.h"  // fl::strlen

namespace fl {

fl::string compactJsonString(const char* jsonStr) {
    if (!jsonStr) {
        return fl::string();
    }
    
    fl::string result;
    result.reserve(strlen(jsonStr)); // Reserve space to minimize reallocations
    
    const char* ptr = jsonStr;
    bool inString = false;
    char lastChar = '\0';
    
    while (*ptr) {
        char c = *ptr;
        
        // Track string state
        if (c == '"' && lastChar != '\\') {
            inString = !inString;
        }
        
        // Check if the character is a whitespace character and we're not inside a string
        if ((c == ' ' || c == '\t' || c == '\n' || c == '\r') && !inString) {
            // Skip whitespace outside of strings
        } else {
            // Keep the character
            result.write(&c, 1);
        }
        
        lastChar = c;
        ptr++;
    }
    
    return result;
}

} // namespace fl
