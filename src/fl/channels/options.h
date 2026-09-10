#pragma once

#include "led_sysdefs.h"  // IWYU pragma: keep
#include "pixeltypes.h"  // IWYU pragma: keep
#include "color.h"
#include "dither_mode.h"
#include "fl/channels/bus.h"
#include "rgbw.h"  // IWYU pragma: keep
#include "fl/gfx/rgbww.h"  // IWYU pragma: keep
#include "fl/stl/optional.h"
#include "fl/stl/variant.h"
#include "fl/channels/color_profile.h"
#include "fl/channels/channel_events.h"
#include "fl/log/log.h"

namespace fl {

// Declared rather than included. Every path that binds a profile has to
// install these -- there are four, and three of them do not go through
// setColorProfile: Channel::create<Profile>,
// ChannelOptions::withColorProfile<Profile> and
// CLEDController::bindStaticEmitterProfile all set mStaticProfile directly.
// Missing one leaves that channel silently on the legacy path, which is the
// exact bug this whole change exists to fix.
//
// Declared rather than included. `pipeline_binding.h` pulls the whole gfx
// pipeline in behind it, and reaching those headers from here puts them
// ahead of whatever brings `EmitterProfile` into scope unqualified --
// `device_solve.h`, `gamut_map.h` and `white_allocation.h` all stop
// compiling. One declaration is all this file needs, and keeping it to one
// also keeps `options.h` cheap for every translation unit that includes it.
void installColorPipelineHooks() FL_NO_EXCEPT;


#ifndef FL_COLOR_PROFILE_RUNTIME
#define FL_COLOR_PROFILE_RUNTIME (!FL_PLATFORM_HAS_TINY_MEMORY)
#endif

/// Optional channel configuration parameters
/// All fields have sensible defaults and can be overridden as needed.
///
/// **Driver selection** (#2459). `mBus` is the single typed selector:
///
///   - `Bus::AUTO` (default) — let `ChannelManager` pick by priority dispatch.
///   - Any other value — pin this channel to the named driver via
///     `ChannelManager::findDriverByName(busName(mBus))`. If the named driver
///     isn't registered, `Channel::showPixels` emits a one-shot `FL_ERROR`
///     with the resolution hint (`fl::enableDrivers<fl::Bus::X>()` or
///     `FastLED.enableAllDrivers()`) and falls back to AUTO/priority.
///
/// **White-channel configuration** (#2558). `mWhiteCfg` is a 3-way variant:
///
///   - `fl::Empty` (default) — plain 3-channel RGB output, no white extraction.
///     Replaces the legacy `Rgbw{rgbw_mode=kRGBWInvalid}` sentinel.
///   - `Rgbw` — 4-channel RGBW output. Mode + CCT + optional shared
///     colorimetric profile handle inside the Rgbw value.
///   - `Rgbww` — 5-channel RGB + warm-W + cool-W output. Mode + two CCTs +
///     optional profile pointer inside the Rgbww value.
///
/// `setRgbw()` / `getRgbw()` on CLEDController remain backward-compatible —
/// they wrap the variant. New 5-channel callers use `setRgbww()`/`getRgbww()`.
///
/// **Custom / third-party / mock drivers** whose names aren't in the
/// `fl::Bus` enum should be bound either by priority (clear competing
/// drivers via `clearAllDrivers()` and let the mock win priority dispatch)
/// or via `ChannelManager::setExclusiveDriverByName(name)` for process-wide
/// binding. There is no string-typed affinity field on `ChannelOptions`.
struct ChannelOptions {
    CRGB mCorrection = UncorrectedColor;
    CRGB mTemperature = UncorrectedTemperature;
    fl::u8 mDitherMode = BINARY_DITHER;
    /// White-channel selection (variant): Empty = plain RGB, Rgbw = 4-channel,
    /// Rgbww = 5-channel. Default-constructed = Empty.
    fl::variant<fl::Empty, Rgbw, Rgbww> mWhiteCfg;
    Bus mBus = Bus::AUTO;              // Typed driver selection
    fl::u8 mBusWhich = 0;              // Instance selector for portable buses
    fl::optional<float> mGamma;        // Gamma correction (nullopt = use default 2.8)
#if FL_COLOR_PROFILE_RUNTIME
    ColorProfileBinding mColorProfile;
#endif

    bool setColorProfile(const EmitterProfile& profile) FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return setColorProfile(profile, SourceProfile::linearSrgb(), true);
#else
        FL_UNUSED(profile);
        return false;
#endif
    }
    bool setColorProfile(const EmitterProfile& profile,
                         SourceProfile source,
                         GamutPolicy gamut = GamutPolicy::ChromaCompress) FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return setColorProfile(profile, source, false, gamut);
#else
        FL_UNUSED(profile); FL_UNUSED(source); FL_UNUSED(gamut);
        return false;
#endif
    }
    bool setColorProfile(const EmitterProfile& profile, SourceProfile source,
                         bool global_source, GamutPolicy gamut = GamutPolicy::ChromaCompress) FL_NO_EXCEPT {
#if !FL_COLOR_PROFILE_RUNTIME
        FL_UNUSED(profile); FL_UNUSED(source); FL_UNUSED(global_source); FL_UNUSED(gamut);
        return false;
#else
        // Captured before mStaticProfile is cleared below, so the warning
        // condition still sees whether a profile was bound on entry.
        const bool had_profile = hasColorProfile();
        mColorProfile.mStaticProfile = nullptr;
        if (profile.native_code_depth == 0 || profile.native_code_depth > 16 ||
            !validChromaticity(profile.xy_r) || !validChromaticity(profile.xy_g) ||
            !validChromaticity(profile.xy_b) || !validPrimaries(source.primaries) ||
            !finitePositive(profile.lum_r) || !finitePositive(profile.lum_g) || !finitePositive(profile.lum_b) ||
            (profile.response_lut_size != 0 &&
             (profile.response_lut_r == nullptr || profile.response_lut_g == nullptr || profile.response_lut_b == nullptr))) {
            clearColorProfile();
            return false;
        }
        if (!monotonic(profile.response_lut_r, profile.response_lut_size) ||
            !monotonic(profile.response_lut_g, profile.response_lut_size) ||
            !monotonic(profile.response_lut_b, profile.response_lut_size)) {
            clearColorProfile();
            return false;
        }
        // Warn only once validation has passed. Warning earlier consumed
        // mWarnedLegacyCleared on a profile that was then rejected, and
        // clearColorProfile() leaves mCorrection/mTemperature alone -- so the
        // next *valid* profile cleared the caller's legacy settings silently.
        if (!had_profile &&
            (mCorrection != UncorrectedColor || mTemperature != UncorrectedTemperature) &&
            !mWarnedLegacyCleared) {
            FL_WARN_F("Color profile clears legacy correction/temperature");
            ChannelEvents::instance().onColorProfileWarning(
                ColorProfileEvent{-1, {}, ColorProfileWarning::LegacyClearedByProfile});
            mWarnedLegacyCleared = true;
        }
        // The one call that makes the colour pipeline reachable. Everything
        // downstream goes through function pointers these install, so a
        // program that never gets here never references the pipeline and the
        // linker drops it -- about 3.5 KB of flash for sketches that do not
        // ask for colour management.
        installColorPipelineHooks();
        mColorProfile.mStorage = fl::make_shared<ColorProfileStorage>(profile);
        mColorProfile.mSource = source;
        mColorProfile.mGamut = gamut;
        mColorProfile.mRequested = true;
        mColorProfile.mUseGlobalSourceDefault = global_source;
        mCorrection = UncorrectedColor;
        mTemperature = UncorrectedTemperature;
        mGamma.reset();
        mDitherMode = DISABLE_DITHER;
        return true;
#endif
    }
    void requestColorManagement(SourceProfile source = SourceProfile::linearSrgb()) FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        clearColorProfile();
        mColorProfile.mSource = source;
        mColorProfile.mRequested = true;
#else
        FL_UNUSED(source);
#endif
    }
    void clearColorProfile() FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        mColorProfile.mStorage.reset();
        mColorProfile.mStaticProfile = nullptr;
        mColorProfile.mRequested = false;
        // Also drop the global-source opt-in. Without this, a profile bound
        // by setColorProfile() leaves the flag set, and a following
        // requestColorManagement(explicitSource) has its source silently
        // replaced by defaultSourceProfile() in Channel::create().
        // setColorProfile() re-establishes the flag itself when it succeeds.
        mColorProfile.mUseGlobalSourceDefault = false;
#endif
    }
    bool hasColorProfile() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return mColorProfile.active();
#else
        return false;
#endif
    }
    // No isColorManaged() here on purpose. It used to exist as a hardcoded
    // `false` with no callers anywhere, and options cannot answer the
    // question: they carry the *request*, while the transform is built by
    // Channel::reconcileColorProfile and can fail for a binding these options
    // accepted. Asking a request object whether rendering is installed can
    // only ever return a plausible-looking guess. Channel::isColorManaged()
    // is the one that knows (#4328).
    const EmitterProfile* emitterProfile() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return mColorProfile.profile();
#else
        return nullptr;
#endif
    }
    bool setTargetWhite(Chromaticity white) FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        if (!validChromaticity(white)) return false;
        mTargetWhite = white;
        mHasTargetWhite = true;
        return true;
#else
        FL_UNUSED(white);
        return false;
#endif
    }
    Chromaticity targetWhite() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return mTargetWhite;
#else
        return Chromaticity();
#endif
    }
    bool hasTargetWhite() const FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        return mHasTargetWhite;
#else
        return false;
#endif
    }

    template<const EmitterProfile& Profile>
    static ChannelOptions withColorProfile() FL_NO_EXCEPT {
        ChannelOptions options;
#if FL_COLOR_PROFILE_RUNTIME
        installColorPipelineHooks();
        options.mColorProfile.mStaticProfile = &Profile;
        options.mColorProfile.mRequested = true;
#endif
        return options;
    }
    void setLegacyCorrection(CRGB correction) FL_NO_EXCEPT { warnProfileCleared(); clearColorProfile(); mCorrection = correction; }
    void setLegacyTemperature(CRGB temperature) FL_NO_EXCEPT { warnProfileCleared(); clearColorProfile(); mTemperature = temperature; }

private:
    static bool validChromaticity(const float xy[2]) FL_NO_EXCEPT {
        return xy[0] > 0.0f && xy[0] < 1.0f && xy[1] > 0.0f && xy[1] < 1.0f &&
               xy[0] + xy[1] <= 1.0f;
    }
    static bool validChromaticity(Chromaticity xy) FL_NO_EXCEPT {
        return xy.x > 0.0f && xy.x < 1.0f && xy.y > 0.0f && xy.y < 1.0f &&
               xy.x + xy.y <= 1.0f;
    }
    static bool validPrimaries(const RgbPrimaries& primaries) FL_NO_EXCEPT {
        return validChromaticity(primaries.red) && validChromaticity(primaries.green) &&
               validChromaticity(primaries.blue) && validChromaticity(primaries.white);
    }
    static bool monotonic(const u16* values, u16 size) FL_NO_EXCEPT {
        for (u16 i = 1; i < size; ++i) {
            if (values[i] < values[i - 1]) return false;
        }
        return true;
    }
    static bool finitePositive(float value) FL_NO_EXCEPT {
        return value > 0.0f && value < 3.4028234e38f;
    }
    void warnProfileCleared() FL_NO_EXCEPT {
#if FL_COLOR_PROFILE_RUNTIME
        if (hasColorProfile() && !mWarnedProfileCleared) {
            FL_WARN_F("Legacy correction/temperature clears color profile");
            ChannelEvents::instance().onColorProfileWarning(
                ColorProfileEvent{-1, {}, ColorProfileWarning::ProfileClearedByLegacy});
            mWarnedProfileCleared = true;
        }
#endif
    }

public:

    /// @return The active Rgbw if mWhiteCfg holds one, else RgbwInvalid::value().
    /// Backward-compat shim for code paths that pre-date the variant migration.
    Rgbw rgbw() const FL_NO_EXCEPT {
        if (auto* p = mWhiteCfg.ptr<Rgbw>()) return *p;
        return RgbwInvalid::value();
    }

    /// @return The active Rgbww if mWhiteCfg holds one, else RgbwwInvalid::value().
    Rgbww rgbww() const FL_NO_EXCEPT {
        if (auto* p = mWhiteCfg.ptr<Rgbww>()) return *p;
        return RgbwwInvalid::value();
    }

    /// True if this channel emits 4-channel RGBW. Requires both the Rgbw
    /// variant alternative AND an active mode — a stored
    /// `RgbwInvalid::value()` does not count, mirroring the legacy
    /// `Rgbw::active()` semantics.
    bool isRgbw() const FL_NO_EXCEPT {
        auto* p = mWhiteCfg.ptr<Rgbw>();
        return p != nullptr && p->active();
    }
    /// True if this channel emits 5-channel RGBWW. Same active-flag check as
    /// isRgbw() — a stored `RgbwwInvalid::value()` does not count.
    bool isRgbww() const FL_NO_EXCEPT {
        auto* p = mWhiteCfg.ptr<Rgbww>();
        return p != nullptr && p->active();
    }

private:
#if FL_COLOR_PROFILE_RUNTIME
    Chromaticity mTargetWhite;
    bool mHasTargetWhite = false;
    bool mWarnedProfileCleared = false;
    bool mWarnedLegacyCleared = false;
#endif
};

} // namespace fl
