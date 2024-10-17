#pragma once

#include <map>
#include <string>
#include "third_party/arduinojson/json.h"


class JsonStringValueDecoder {
public:
    static bool parseJson(const char* input, std::map<std::string, std::string>* destination) {
        if (!input || !destination) return false;

        ArduinoJson::DynamicJsonDocument doc(1024);
        ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, input);

        if (error) return false;

        destination->clear();
        for (ArduinoJson::JsonPair kv : doc.as<ArduinoJson::JsonObject>()) {
            (*destination)[kv.key().c_str()] = kv.value().as<std::string>();
        }

        return true;
    }
};

class JsonIdValueDecoder {
public:
    static bool parseJson(const char* input, std::map<int, std::string>* destination) {
        if (!input || !destination) return false;

        ArduinoJson::DynamicJsonDocument doc(1024);
        ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, input);

        if (error) return false;

        destination->clear();
        for (ArduinoJson::JsonPair kv : doc.as<ArduinoJson::JsonObject>()) {
            int key = atoi(kv.key().c_str());
            (*destination)[key] = kv.value().as<std::string>();
        }

        return true;
    }
};
