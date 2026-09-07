#pragma once

#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

using colorimetric_response::EmitterProfile;  // ok bare using: public P2 profile API

/// One immutable allocation for every borrowed member in EmitterProfile.
/// The profile's C-compatible pointers are rebound only to members here.
struct ColorProfileStorage {
    EmitterProfile mProfile;
    fl::vector<u16> mResponseRed;
    fl::vector<u16> mResponseGreen;
    fl::vector<u16> mResponseBlue;
    fl::string mId;
    fl::string mProvenanceKind;
    fl::string mReportId;

    explicit ColorProfileStorage(const EmitterProfile& source) FL_NO_EXCEPT : mProfile(source),
        mId(source.id == nullptr ? "" : source.id),
        mProvenanceKind(source.provenance_kind == nullptr ? "" : source.provenance_kind),
        mReportId(source.report_id == nullptr ? "" : source.report_id) {
        if (source.response_lut_size != 0) {
            mResponseRed.assign(source.response_lut_r, source.response_lut_r + source.response_lut_size);
            mResponseGreen.assign(source.response_lut_g, source.response_lut_g + source.response_lut_size);
            mResponseBlue.assign(source.response_lut_b, source.response_lut_b + source.response_lut_size);
            mProfile.response_lut_r = mResponseRed.data();
            mProfile.response_lut_g = mResponseGreen.data();
            mProfile.response_lut_b = mResponseBlue.data();
        }
        mProfile.id = mId.empty() ? nullptr : mId.c_str();
        mProfile.provenance_kind = mProvenanceKind.empty() ? nullptr : mProvenanceKind.c_str();
        mProfile.report_id = mReportId.empty() ? nullptr : mReportId.c_str();
    }

    ColorProfileStorage(const ColorProfileStorage&) FL_NO_EXCEPT = delete;
    ColorProfileStorage(ColorProfileStorage&&) FL_NO_EXCEPT = delete;
    ColorProfileStorage& operator=(const ColorProfileStorage&) FL_NO_EXCEPT = delete;
    ColorProfileStorage& operator=(ColorProfileStorage&&) FL_NO_EXCEPT = delete;
};

/// Immutable profile binding carried by a channel.
struct ColorProfileBinding {
    fl::shared_ptr<const ColorProfileStorage> mStorage;
    SourceProfile mSource = SourceProfile::linearSrgb();
    GamutPolicy mGamut = GamutPolicy::ChromaCompress;
    bool mRequested = false;
    bool mUseGlobalSourceDefault = false;
    const EmitterProfile* mStaticProfile = nullptr;

    bool active() const FL_NO_EXCEPT { return static_cast<bool>(mStorage) || mStaticProfile != nullptr; }
    const EmitterProfile* profile() const FL_NO_EXCEPT { return mStaticProfile != nullptr ? mStaticProfile : (mStorage ? &mStorage->mProfile : nullptr); }
};

namespace detail {
// Defined in color_profile.cpp.hpp, not here. A function-local static in a
// header gives every DLL that includes it its own copy, so on Windows the
// test module set one instance of these while Channel::create() read
// another; `SourceProfile` also has a non-trivial constructor, which the
// header rule prohibits outright.
bool& colorProfileStrictMode() FL_NO_EXCEPT;
SourceProfile& defaultSourceProfile() FL_NO_EXCEPT;
}  // namespace detail

}  // namespace fl
