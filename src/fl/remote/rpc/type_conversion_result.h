#pragma once

#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

// =============================================================================
// TypeConversionResult - Warning/Error tracking for type conversions
// =============================================================================

class TypeConversionResult {
public:
    TypeConversionResult() FL_NO_EXCEPT : mHasError(false) {}

    static TypeConversionResult success() FL_NO_EXCEPT {
        return TypeConversionResult();
    }

    static TypeConversionResult warning(const fl::string& msg) FL_NO_EXCEPT {
        TypeConversionResult result;
        result.mWarnings.push_back(msg);
        return result;
    }

    static TypeConversionResult error(const fl::string& msg) FL_NO_EXCEPT {
        TypeConversionResult result;
        result.mHasError = true;
        result.mErrorMessage = msg;
        return result;
    }

    bool ok() const FL_NO_EXCEPT { return !mHasError; }
    bool hasWarning() const FL_NO_EXCEPT { return !mWarnings.empty(); }
    bool hasError() const FL_NO_EXCEPT { return mHasError; }

    const fl::vector<fl::string>& warnings() const FL_NO_EXCEPT { return mWarnings; }
    const fl::string& errorMessage() const FL_NO_EXCEPT { return mErrorMessage; }

    void addWarning(const fl::string& msg) FL_NO_EXCEPT {
        mWarnings.push_back(msg);
    }

    void setError(const fl::string& msg) FL_NO_EXCEPT {
        mHasError = true;
        mErrorMessage = msg;
    }

    // Merge another result into this one
    void merge(const TypeConversionResult& other) FL_NO_EXCEPT {
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
