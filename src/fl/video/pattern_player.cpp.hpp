#include "fl/video/pattern_player.h"
#include "fl/stl/chrono.h"

namespace fl {
namespace video {

PatternPlayer::PatternPlayer(const char* filepath)
    : mFilepath(filepath),
      mHasScreenMap(false),
      mLastDrawTime(0),
      mFps(30.0f) {
}

PatternPlayer::~PatternPlayer() FL_NO_EXCEPT {
    end();
}

bool PatternPlayer::begin(int cs_pin, int numLeds, float fps) {
    if (!mFs.beginSd(cs_pin)) {
        return false;
    }
    return begin(mFs, numLeds, fps);
}

bool PatternPlayer::begin(FileSystem& fs, int numLeds, float fps) {
    mHasScreenMap = false;
    if (fps <= 0.0f) {
        return false;
    }
    mFs = fs;
    mFps = fps;
    mVideo = mFs.openVideo(mFilepath.c_str(), numLeds, fps, 2);
    if (!mVideo) {
        return false;
    }
    if (mVideo.hasEmbeddedScreenMap()) {
        const fl::string& json = mVideo.embeddedScreenMapJson();
        fl::string parseErr;
        if (ScreenMap::ParseJson(json.c_str(), "strip1", &mScreenMap, &parseErr)) {
            mHasScreenMap = true;
            return true;
        }
    }
    fl::string sidecarPath = mFilepath;
    size_t lastSlash = sidecarPath.find_last_of('/');
    if (lastSlash != fl::string::npos) {
        sidecarPath = sidecarPath.substr(0, lastSlash + 1) + "screenmap.json";
    } else {
        sidecarPath = "screenmap.json";
    }
    if (mFs.readScreenMap(sidecarPath.c_str(), "strip1", &mScreenMap)) {
        mHasScreenMap = true;
    }
    return true;
}

void PatternPlayer::end() {
    if (mVideo) {
        mVideo.end();
    }
    mFs.end();
    mHasScreenMap = false;
}

bool PatternPlayer::hasNewFrame() const {
    if (!mVideo) {
        return false;
    }
    return (fl::millis() - mLastDrawTime) >= (1000.0f / mFps);
}

void PatternPlayer::drawFrame(fl::span<CRGB> leds) {
    if (mVideo) {
        mLastDrawTime = fl::millis();
        mVideo.draw(mLastDrawTime, leds);
    }
}

const ScreenMap& PatternPlayer::getScreenMap() const {
    return mScreenMap;
}

bool PatternPlayer::hasScreenMap() const {
    return mHasScreenMap;
}

} // namespace video
} // namespace fl
