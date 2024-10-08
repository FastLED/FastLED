#pragma once

#include "namespace.h"
#include "crgb.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(Frame);

class Frame : public Referent {
public:
    explicit Frame(int bytes_per_frame);
    uint8_t* data() { return mSurface.get(); }
    const uint8_t* data() const { return mSurface.get(); }

private:
    scoped_array<uint8_t> mSurface;
};

FASTLED_NAMESPACE_END
