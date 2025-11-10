/// @file cled_controller.cpp
/// base definitions used by led controllers for writing out led data

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "cled_controller.h"

#include "fl/cstring.h"


CLEDController::~CLEDController() = default;

/// Create an led controller object, add it to the chain of controllers
CLEDController::CLEDController() : m_Data(nullptr), mSettings(), m_nLeds(0) {
    m_pNext = nullptr;
    if(m_pHead==nullptr) { m_pHead = this; }
    if(m_pTail != nullptr) { m_pTail->m_pNext = this; }
    m_pTail = this;
}



void CLEDController::clearLedDataInternal(int nLeds) {
    if(m_Data) {
        nLeds = (nLeds < 0) ? m_nLeds : nLeds;
        nLeds = (nLeds > m_nLeds) ? m_nLeds : nLeds;
        fl::memset((void*)m_Data, 0, sizeof(CRGB) * nLeds);
    }

}

void CLEDController::removeFromList(CLEDController* controller) {
    if (controller == nullptr) {
        return;
    }

    // Remove the specified controller from the linked list
    CLEDController* prev = nullptr;
    CLEDController* curr = m_pHead;

    // Find the controller in the linked list
    while (curr != nullptr) {
        if (curr == controller) {
            // Found it - remove from list
            if (prev == nullptr) {
                // Removing head
                m_pHead = controller->m_pNext;
                if (m_pHead == nullptr) {
                    // List is now empty
                    m_pTail = nullptr;
                }
            } else {
                // Removing from middle or end
                prev->m_pNext = controller->m_pNext;
                if (controller->m_pNext == nullptr) {
                    // Removing tail
                    m_pTail = prev;
                }
            }

            // Clear the controller's next pointer
            controller->m_pNext = nullptr;
            break;
        }
        prev = curr;
        curr = curr->m_pNext;
    }
}

ColorAdjustment CLEDController::getAdjustmentData(uint8_t brightness) {
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



