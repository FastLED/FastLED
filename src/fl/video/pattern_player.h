#pragma once

#include "fl/system/file_system.h"
#include "fl/fx/video.h"
#include "fl/math/screenmap.h"
#include "fl/stl/string.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace video {

class PatternPlayer {
public:
    PatternPlayer(const char* filepath);
    ~PatternPlayer() FL_NO_EXCEPT;

    bool begin(int cs_pin, int numLeds, float fps = 30.0f);
    bool begin(FileSystem& fs, int numLeds, float fps = 30.0f);
    void end();

    bool hasNewFrame() const;
    void drawFrame(fl::span<CRGB> leds);

    const ScreenMap& getScreenMap() const;
    bool hasScreenMap() const;

private:
    fl::string mFilepath;
    FileSystem mFs;
    Video mVideo;
    ScreenMap mScreenMap;
    bool mHasScreenMap;
    fl::u32 mLastDrawTime;
    float mFps;
};

} // namespace video

using PatternPlayer = video::PatternPlayer;

} // namespace fl
