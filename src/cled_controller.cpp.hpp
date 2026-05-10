/// @file cled_controller.cpp
/// base definitions used by led controllers for writing out led data

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "cled_controller.h"

#include "fl/stl/cstring.h"
#include "fl/system/sketch_macros.h"


CLEDController::~CLEDController() {
#if SKETCH_HAS_LARGE_MEMORY
    // Remove from draw list on destruction to prevent dangling pointers
    // Note: Not enabled on memory-constrained platforms (AVR, ESP8266, etc.)
    // because the virtual destructor adds ~600 bytes on AVR and pulling in
    // removeFromDrawList() adds additional overhead
    removeFromDrawList();
#endif
}

/// Create an led controller object, add it to the chain of controllers
CLEDController::CLEDController() : mLeds(), mSettings() {
    addToList();
}

CLEDController::CLEDController(RegistrationMode mode) : mLeds(), mSettings() {
    if (mode == RegistrationMode::AutoRegister) {
        addToList();
    }
}

void CLEDController::addToList() {
    // Don't add if already in list
    #if SKETCH_HAS_LARGE_MEMORY // Mostly for AVR, the isInList() check adds memory overhead on these tight platforms.
    if (isInList()) {
        return;
    }
    #endif

    // Add to linked list
    if(mPHead==nullptr) { mPHead = this; }
    if(mPTail != nullptr) { mPTail->mPNext = this; }
    mPTail = this;
}

bool CLEDController::isInList() const {
    CLEDController* curr = mPHead;
    while (curr != nullptr) {
        if (curr == this) {
            return true;
        }
        curr = curr->mPNext;
    }
    return false;
}

void CLEDController::clearLedDataInternal(int nLeds) {
    // On common code that runs on avr, every byte counts.
    fl::u16 n = nLeds >= 0 ? static_cast<fl::u16>(nLeds) : static_cast<fl::u16>(mLeds.size());
    if (mLeds.data()) {
        fl::memset((void*)mLeds.data(), 0, sizeof(CRGB) * n);
    }

}

void CLEDController::removeFromList(CLEDController* controller) {
    if (controller == nullptr) {
        return;
    }

    // Remove the specified controller from the linked list
    CLEDController* prev = nullptr;
    CLEDController* curr = mPHead;

    // Find the controller in the linked list
    while (curr != nullptr) {
        if (curr == controller) {
            // Found it - remove from list
            if (prev == nullptr) {
                // Removing head
                mPHead = controller->mPNext;
                if (mPHead == nullptr) {
                    // List is now empty
                    mPTail = nullptr;
                }
            } else {
                // Removing from middle or end
                prev->mPNext = controller->mPNext;
                if (controller->mPNext == nullptr) {
                    // Removing tail
                    mPTail = prev;
                }
            }

            // Clear the controller's next pointer
            controller->mPNext = nullptr;
            break;
        }
        prev = curr;
        curr = curr->mPNext;
    }
}

ColorAdjustment CLEDController::getAdjustmentData(fl::u8 brightness) {
    // *premixed = getAdjustment(brightness);
    // if (color_correction) {
    //     *color_correction = getAdjustment(255);
    // }
    #if FASTLED_HD_COLOR_MIXING
    ColorAdjustment out = {this->getAdjustment(brightness), this->getAdjustment(255), brightness};
    #else
    ColorAdjustment out = {getAdjustment(brightness)};
    #endif
    return out;
}
