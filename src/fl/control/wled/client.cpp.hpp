// ok no header
#include "fl/fx/wled/client.h"
#include "fl/log/log.h"
#include "fl/log/log.h"

namespace fl {

// WLEDClient implementation

WLEDClient::WLEDClient(fl::shared_ptr<IFastLED> controller)
    : mController(controller), mBrightness(255), mOn(false) {
    if (!mController) {
        FL_WARN_F("WLEDClient: constructed with null controller");
    }
}

void WLEDClient::setBrightness(u8 brightness) {
    mBrightness = brightness;
    FL_DBG_F("WLEDClient: setBrightness(%s)", static_cast<int>(mBrightness));

    // Apply brightness to controller if we're on
    if (mOn && mController) {
        mController->setBrightness(mBrightness);
    }
}

void WLEDClient::setOn(bool on) {
    mOn = on;
    FL_DBG_F("WLEDClient: setOn(%s)", (mOn ? "true" : "false"));

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

void WLEDClient::clear(bool writeToStrip) {
    FL_DBG_F("WLEDClient: clear(writeToStrip=%s)", (writeToStrip ? "true" : "false"));

    if (!mController) {
        return;
    }

    mController->clear(writeToStrip);
}

void WLEDClient::update() {
    FL_DBG_F("WLEDClient: update()");

    if (!mController) {
        return;
    }

    mController->show();
}

fl::span<CRGB> WLEDClient::getLEDs() {
    if (!mController) {
        return fl::span<CRGB>();
    }

    return mController->getLEDs();
}

size_t WLEDClient::getNumLEDs() const {
    if (!mController) {
        return 0;
    }

    return mController->getNumLEDs();
}

void WLEDClient::setSegment(size_t start, size_t end) {
    FL_DBG_F("WLEDClient: setSegment(%s, %s)", start, end);

    if (!mController) {
        return;
    }

    mController->setSegment(start, end);
}

void WLEDClient::clearSegment() {
    FL_DBG_F("WLEDClient: clearSegment()");

    if (!mController) {
        return;
    }

    mController->clearSegment();
}

void WLEDClient::setCorrection(CRGB correction) {
    FL_DBG_F("WLEDClient: setCorrection(r=%s, g=%s, b=%s)", static_cast<int>(correction.r), static_cast<int>(correction.g), static_cast<int>(correction.b));

    if (!mController) {
        return;
    }

    mController->setCorrection(correction);
}

void WLEDClient::setTemperature(CRGB temperature) {
    FL_DBG_F("WLEDClient: setTemperature(r=%s, g=%s, b=%s)", static_cast<int>(temperature.r), static_cast<int>(temperature.g), static_cast<int>(temperature.b));

    if (!mController) {
        return;
    }

    mController->setTemperature(temperature);
}

void WLEDClient::setMaxRefreshRate(u16 fps) {
    FL_DBG_F("WLEDClient: setMaxRefreshRate(%s)", fps);

    if (!mController) {
        return;
    }

    mController->setMaxRefreshRate(fps);
}

u16 WLEDClient::getMaxRefreshRate() const {
    if (!mController) {
        return 0;
    }

    return mController->getMaxRefreshRate();
}

} // namespace fl
