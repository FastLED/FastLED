#pragma once

#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

// =============================================================================
// TypeConversionResult - Warning/Error tracking for type conversions
// =============================================================================

class TypeConversionResult {
public:
    TypeConversionResult() : mHasError(false) {}

    static TypeConversionResult success() {
        return TypeConversionResult();
    }

    static TypeConversionResult warning(const fl::string& msg) {
        TypeConversionResult result;
        result.mWarnings.push_back(msg);
        return result;
    }

    static TypeConversionResult error(const fl::string& msg) {
        TypeConversionResult result;
        result.mHasError = true;
        result.mErrorMessage = msg;
        return result;
    }

    bool ok() const { return !mHasError; }
    bool hasWarning() const { return !mWarnings.empty(); }
    bool hasError() const { return mHasError; }

    const fl::vector<fl::string>& warnings() const { return mWarnings; }
    const fl::string& errorMessage() const { return mErrorMessage; }

    void addWarning(const fl::string& msg) {
        mWarnings.push_back(msg);
    }

    void setError(const fl::string& msg) {
        mHasError = true;
        mErrorMessage = msg;
    }

    // Merge another result into this one
    void merge(const TypeConversionResult& other) {
        for (fl::size i = 0; i < other.mWarnings.size(); i++) {
            mWarnings.push_back(other.mWarnings[i]);
        }
        if (other.mHasError) {
            mHasError = true;
            mErrorMessage = other.mErrorMessage;
        }
    }

private:
    fl::vector<fl::string> mWarnings;
    fl::string mErrorMessage;
    bool mHasError;
};

} // namespace fl
