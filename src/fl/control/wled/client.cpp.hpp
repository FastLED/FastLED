// ok no header
#include "fl/fx/wled/client.h"
#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

namespace fl {

// WLEDClient implementation

WLEDClient::WLEDClient(fl::shared_ptr<IFastLED> controller)
    FL_NO_EXCEPT : mController(controller), mBrightness(255), mOn(false) {
    if (!mController) {
        FL_WARN("WLEDClient: constructed with null controller");
    }
}

void WLEDClient::setBrightness(u8 brightness) FL_NO_EXCEPT {
    mBrightness = brightness;
    FL_DBG("WLEDClient: setBrightness(" << (static_cast<int>(mBrightness)) << ")");

    // Apply brightness to controller if we're on
    if (mOn && mController) {
        mController->setBrightness(mBrightness);
    }
}

void WLEDClient::setOn(bool on) FL_NO_EXCEPT {
    mOn = on;
    FL_DBG("WLEDClient: setOn(" << ((mOn ? "true" : "false")) << ")");

    if (!mController) {
        return;
    }

    if (mOn) {
        // Turning on: apply current brightness
        mController->setBrightness(mBrightness);
    } else {
        // Turning off: set brightness to 0 (but preserve internal brightness)
        mController->setBrightness(0);
    }
}

void WLEDClient::clear(bool writeToStrip) FL_NO_EXCEPT {
    FL_DBG("WLEDClient: clear(writeToStrip=" << ((writeToStrip ? "true" : "false")) << ")");

    if (!mController) {
        return;
    }

    mController->clear(writeToStrip);
}

void WLEDClient::update() FL_NO_EXCEPT {
    FL_DBG("WLEDClient: update()");

    if (!mController) {
        return;
    }

    mController->show();
}

fl::span<CRGB> WLEDClient::getLEDs() FL_NO_EXCEPT {
    if (!mController) {
        return fl::span<CRGB>();
    }

    return mController->getLEDs();
}

size_t WLEDClient::getNumLEDs() const FL_NO_EXCEPT {
    if (!mController) {
        return 0;
    }

    return mController->getNumLEDs();
}

void WLEDClient::setSegment(size_t start, size_t end) FL_NO_EXCEPT {
    FL_DBG("WLEDClient: setSegment(" << start << ", " << end << ")");

    if (!mController) {
        return;
    }

    mController->setSegment(start, end);
}

void WLEDClient::clearSegment() FL_NO_EXCEPT {
    FL_DBG("WLEDClient: clearSegment()");

    if (!mController) {
        return;
    }

    mController->clearSegment();
}

void WLEDClient::setCorrection(CRGB correction) FL_NO_EXCEPT {
    FL_DBG("WLEDClient: setCorrection(r=" << (static_cast<int>(correction.r)) << ", g=" << (static_cast<int>(correction.g)) << ", b=" << (static_cast<int>(correction.b)) << ")");

    if (!mController) {
        return;
    }

    mController->setCorrection(correction);
}

void WLEDClient::setTemperature(CRGB temperature) FL_NO_EXCEPT {
    FL_DBG("WLEDClient: setTemperature(r=" << (static_cast<int>(temperature.r)) << ", g=" << (static_cast<int>(temperature.g)) << ", b=" << (static_cast<int>(temperature.b)) << ")");

    if (!mController) {
        return;
    }

    mController->setTemperature(temperature);
}

void WLEDClient::setMaxRefreshRate(u16 fps) FL_NO_EXCEPT {
    FL_DBG("WLEDClient: setMaxRefreshRate(" << fps << ")");

    if (!mController) {
        return;
    }

    mController->setMaxRefreshRate(fps);
}

u16 WLEDClient::getMaxRefreshRate() const FL_NO_EXCEPT {
    if (!mController) {
        return 0;
    }

    return mController->getMaxRefreshRate();
}

} // namespace fl
