#pragma once

#include <sstream>
#include <string>

class JsonDictEncoder {
private:
    std::ostringstream oss;
    bool first = true;
    bool begun = false;
    bool ended = false;

    template<typename T>
    void appendJsonField(const char* name, const T& value) {
        oss << "\"" << name << "\":" << value;
    }

    void appendJsonField(const char* name, const std::string& value) {
        oss << "\"" << name << "\":\"" << value << "\"";
    }

    void appendJsonField(const char* name, const char* value) {
        oss << "\"" << name << "\":\"" << value << "\"";
    }

public:
    JsonDictEncoder() = default;

    void begin() {
        if (!begun) {
            oss << "{";
            begun = true;
        }
    }

    void end() {
        if (begun && !ended) {
            oss << "}";
            begun = false;
            ended = true;
        }
    }

    template<typename T>
    void addField(const char* name, const T& value) {
        if (!begun) {
            begin();
        }
        if (!first) {
            oss << ",";
        }
        appendJsonField(name, value);
        first = false;
    }

    const char* str() {
        end();
        return oss.str().c_str();
    }
};