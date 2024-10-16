#pragma once

#include <sstream>
#include <map>
#include <string>


class JsonDictEncoder {
private:
    std::ostringstream oss;
    std::string finalStr;
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
            finalStr = oss.str();
            oss.clear();
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
        return finalStr.c_str();
    }
};

class JsonStringValueDecoder {
public:
    static bool parseJson(const char* input, std::map<std::string, std::string>* destination) {
        if (!input || !destination) return false;

        destination->clear();
        std::string jsonStr(input);
        size_t pos = 0;
        size_t end = jsonStr.length();

        // Skip leading whitespace
        pos = jsonStr.find_first_not_of(" \t\n\r", pos);
        if (pos == std::string::npos || jsonStr[pos] != '{') return false;

        ++pos; // Skip opening brace
        
        // Handle empty object
        pos = jsonStr.find_first_not_of(" \t\n\r", pos);
        if (pos == std::string::npos) return false;
        if (jsonStr[pos] == '}') return true;

        while (pos < end) {
            // Find key
            pos = jsonStr.find('"', pos);
            if (pos == std::string::npos) return false;
            size_t keyEnd = jsonStr.find('"', pos + 1);
            if (keyEnd == std::string::npos) return false;

            // Parse key
            std::string key = jsonStr.substr(pos + 1, keyEnd - pos - 1);

            // Find colon
            pos = jsonStr.find(':', keyEnd + 1);
            if (pos == std::string::npos) return false;

            // Find value
            pos = jsonStr.find_first_not_of(" \t\n\r", pos + 1);
            if (pos == std::string::npos) return false;

            std::string value;
            if (jsonStr[pos] == '"') {
                size_t valueEnd = jsonStr.find('"', pos + 1);
                if (valueEnd == std::string::npos) return false;
                value = jsonStr.substr(pos + 1, valueEnd - pos - 1);
                pos = valueEnd + 1;
            } else {
                size_t valueEnd = jsonStr.find_first_of(",}", pos);
                if (valueEnd == std::string::npos) return false;
                value = jsonStr.substr(pos, valueEnd - pos);
                // Trim trailing whitespace from value
                value.erase(value.find_last_not_of(" \t\n\r") + 1);
                pos = valueEnd;
            }

            (*destination)[key] = value;

            // Check for comma or end of object
            pos = jsonStr.find_first_not_of(" \t\n\r", pos);
            if (pos == std::string::npos) return false;
            
            if (jsonStr[pos] == '}') {
                // Check if this is the last character (ignoring whitespace)
                return jsonStr.find_first_not_of(" \t\n\r", pos + 1) == std::string::npos;
            } else if (jsonStr[pos] == ',') {
                ++pos; // Skip comma
                // Allow trailing comma
                pos = jsonStr.find_first_not_of(" \t\n\r", pos);
                if (pos == std::string::npos) return false;
                if (jsonStr[pos] == '}') {
                    return jsonStr.find_first_not_of(" \t\n\r", pos + 1) == std::string::npos;
                }
            } else {
                return false;
            }
        }

        return false; // We should never reach here if the JSON is valid
    }
};

class JsonIdValueDecoder {
public:
    static bool parseJson(const char* input, std::map<int, std::string>* destination) {
        if (!input || !destination) return false;

        destination->clear();
        std::string jsonStr(input);
        size_t pos = 0;
        size_t end = jsonStr.length();

        // Remove leading and trailing whitespace and braces
        while (pos < end && (jsonStr[pos] == ' ' || jsonStr[pos] == '{')) ++pos;
        while (end > pos && (jsonStr[end-1] == ' ' || jsonStr[end-1] == '}')) --end;

        while (pos < end) {
            // Find key
            size_t keyStart = jsonStr.find('"', pos);
            size_t keyEnd = jsonStr.find('"', keyStart + 1);
            if (keyStart == std::string::npos || keyEnd == std::string::npos) return false;

            // Parse key
            int key;
            try {
                key = std::stoi(jsonStr.substr(keyStart + 1, keyEnd - keyStart - 1));
            } catch (const std::exception&) {
                return false;
            }

            // Find value
            size_t valueStart = jsonStr.find(':', keyEnd) + 1;
            size_t valueEnd = jsonStr.find(',', valueStart);
            if (valueEnd == std::string::npos) valueEnd = end;

            // Parse value
            std::string value = jsonStr.substr(valueStart, valueEnd - valueStart);
            // Trim whitespace and quotes from value
            value.erase(0, value.find_first_not_of(" \t\n\r\""));
            value.erase(value.find_last_not_of(" \t\n\r\"") + 1);

            (*destination)[key] = value;

            pos = valueEnd + 1;
            // Skip any trailing commas and whitespace
            while (pos < end && (jsonStr[pos] == ',' || jsonStr[pos] == ' ' || jsonStr[pos] == '\n' || jsonStr[pos] == '\r')) ++pos;
        }

        return true;
    }
};
