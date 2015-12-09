#ifndef __INC_PIXELSET_H
#define __INC_PIXELSET_H

#define FOREACH_RHS for(int i=0,j=0; i!=len && j != rhs.len; i+=dir,j+=rhs.dir)
#define FOREACH for(int i=0; i != len; i += dir)

/// Represents a set of CRGB led objects.  Provides the [] array operator, and works like a normal array in that case.
/// This should be kept in sync with the set of functions provided by CRGB as well as functions in colorutils.
class CRGBSet {
  CRGB *leds;
  int   len;
  char  dir;

public:

  inline CRGBSet(const CRGBSet & other) : leds(other.leds), len(other.len), dir(other.dir) {}
  inline CRGBSet(CRGB *_leds, int _len) : leds(_leds), len(_len), dir(1) {}
#ifdef ARM
  inline CRGBSet(CRGB *_leds, int _start, int _end) : leds(_leds), len(_end - _start), dir(0x01 | len>>31) { len += dir; }
#else
  inline CRGBSet(CRGB *_leds, int _start, int _end) : leds(_leds), len(_end - _start) { dir = (len<0) ? -1 : 1; len += dir; }
#endif

  /// what's the size of this set?
  int size() { return abs(len); }

  /// is this set reversed?
  bool reversed() { return len < 0; }

  /// access a single element in this set, just like an array operator
  inline CRGB & operator[](int x) { if(dir) { return leds[x]; } else { return leds[-x]; } }

  /// Access an inclusive subset of the leds in this set.  Note that start can be greater than end, which will
  /// result in a reverse ordering for many functions (useful for mirroring)
  /// @param start the first element from this set for the new subset
  /// @param end the last element for the new subset
  inline CRGBSet operator()(int start, int end) { return CRGBSet(leds+start, start, end); }

  /// Access an inclusive subset of the leds in this set, starting from the first.
  /// @param end the last element for the new subset
  /// Not sure i want this? inline CRGBSet operator()(int end) { return CRGBSet(leds, 0, end); }

  /// Return the reverse ordering of this set
  inline CRGBSet operator-() { return CRGBSet(leds + len - dir, len - dir, 0); }

  /// Return a pointer to the first element in this set
  inline operator CRGB* () const { return leds; }

  /// Assign the passed in color to all elements in this set
  /// @param color the new color for the elements in the set
  inline CRGBSet & operator=(const CRGB & color) { FOREACH { leds[i] = color; } return *this; }

  /// Copy the contents of the passed in set to our set.  Note if one set is smaller than the other, only the
  /// smallest number of items will be copied over.
  inline CRGBSet & operator=(const CRGBSet & rhs) { FOREACH_RHS { leds[i] = rhs.leds[j]; } return *this; }

  // modification/scaling operators
  inline CRGBSet & addToRGB(uint8_t inc) { FOREACH { leds[i] += inc; } return *this; }
  inline CRGBSet & operator+=(CRGBSet & rhs) { FOREACH_RHS { leds[i] += rhs.leds[j]; } return *this; }

  inline CRGBSet & subFromRGB(uint8_t inc) { FOREACH { leds[i] -= inc; } return *this; }
  inline CRGBSet & operator-=(CRGBSet & rhs) { FOREACH_RHS { leds[i] -= rhs.leds[j]; } return *this; }

  inline CRGBSet & operator++() { FOREACH { leds[i]++; } return *this; }
  inline CRGBSet & operator++(int DUMMY_ARG) { FOREACH { leds[i]++; } return *this; }

  inline CRGBSet & operator--() { FOREACH { leds[i]--; } return *this; }
  inline CRGBSet & operator--(int DUMMY_ARG) { FOREACH { leds[i]--; } return *this; }

  inline CRGBSet & operator/=(uint8_t d) { FOREACH { leds[i] /= d; } return *this; }
  inline CRGBSet & operator>>=(uint8_t d) { FOREACH { leds[i] >>= d; } return *this; }
  inline CRGBSet & operator*=(uint8_t d) { FOREACH { leds[i] *= d; } return *this; }

  inline CRGBSet & nscale8_video(uint8_t scaledown) { FOREACH { leds[i].nscale8_video(scaledown); } return *this;}
  inline CRGBSet & operator%=(uint8_t scaledown) { FOREACH { leds[i].nscale8_video(scaledown); } return *this; }
  inline CRGBSet & fadeLightBy(uint8_t fadefactor) { return nscale8_video(255 - fadefactor); }

  inline CRGBSet & nscale8(uint8_t scaledown) { FOREACH { leds[i].nscale8(scaledown); } return *this; }
  inline CRGBSet & nscale8(CRGB & scaledown) { FOREACH { leds[i].nscale8(scaledown); } return *this; }
  inline CRGBSet & nscale8(CRGBSet & rhs) { FOREACH_RHS { leds[i].nscale8(leds[j]); } return *this; }

  inline CRGBSet & fadeToBlackBy(uint8_t fade) { return nscale8(255 - fade); }

  inline CRGBSet & operator|=(const CRGB & rhs) { FOREACH { leds[i] |= rhs; } return *this; }
  inline CRGBSet & operator|=(const CRGBSet & rhs) { FOREACH_RHS { leds[i] |= rhs.leds[j]; } return *this; }
  inline CRGBSet & operator|=(uint8_t d) { FOREACH { leds[i] |= d; } return *this; }

  inline CRGBSet & operator&=(const CRGB & rhs) { FOREACH { leds[i] &= rhs; } return *this; }
  inline CRGBSet & operator&=(const CRGBSet & rhs) { FOREACH_RHS { leds[i] &= rhs.leds[j]; } return *this; }
  inline CRGBSet & operator&=(uint8_t d) { FOREACH { leds[i] &= d; } return *this; }

  inline operator bool() { FOREACH { if(leds[i]) return true; } return false; }

  // Color util functions
  inline CRGBSet & fill_solid(const CRGB & color) { if(dir) { ::fill_solid(leds, len, color); } else { ::fill_solid(leds + len + 1, -len, color); } return *this; }
  inline CRGBSet & fill_solid(const CHSV & color) { if(dir) { ::fill_solid(leds, len, color); } else { ::fill_solid(leds + len + 1, -len, color); } return *this; }

  inline CRGBSet & fill_rainbow(uint8_t initialhue, uint8_t deltahue=5) {
    if(dir >= 0) {
      ::fill_rainbow(leds,len,initialhue,deltahue);
    } else {
      ::fill_rainbow(leds+len+1,-len,initialhue,deltahue);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient(const CHSV & startcolor, const CHSV & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds,len,startcolor, endcolor, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, (-len), endcolor, startcolor, directionCode);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV &  c3, TGradientDirectionCode directionCode = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds, len, c1, c2, c3, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, -len, c3, c2, c1, directionCode);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV & c3, const CHSV & c4, TGradientDirectionCode directionCode = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds, len, c1, c2, c3, c4, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, -len, c4, c3, c2, c1, directionCode);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient_RGB(const CRGB & startcolor, const CRGB & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds,len,startcolor, endcolor);
    } else {
      ::fill_gradient_RGB(leds + len + 1, (-len), endcolor, startcolor);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient_RGB(const CRGB & c1, const CRGB & c2, const CRGB &  c3) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds, len, c1, c2, c3);
    } else {
      ::fill_gradient_RGB(leds + len + 1, -len, c3, c2, c1);
    }
    return *this;
  }

  inline CRGBSet & fill_gradient_RGB(const CRGB & c1, const CRGB & c2, const CRGB & c3, const CRGB & c4) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds, len, c1, c2, c3, c4);
    } else {
      ::fill_gradient_RGB(leds + len + 1, -len, c4, c3, c2, c1);
    }
    return *this;
  }

  inline CRGBSet & nblend(const CRGB & overlay, fract8 amountOfOverlay) { FOREACH { ::nblend(leds[i], overlay, amountOfOverlay); } return *this; }
  inline CRGBSet & nblend(const CRGBSet & rhs, fract8 amountOfOverlay) { FOREACH_RHS { ::nblend(leds[i], rhs.leds[j], amountOfOverlay); } return *this; }

  // Note: only bringing in a 1d blur, not sure 2d blur makes sense when looking at sub arrays
  inline CRGBSet & blur1d(fract8 blur_amount) {
    if(dir >= 0) {
      ::blur1d(leds, len, blur_amount);
    } else {
      ::blur1d(leds + len + 1, -len, blur_amount);
    }
    return *this;
  }

  inline CRGBSet & napplyGamma_video(float gamma) {
    if(dir >= 0) {
      ::napplyGamma_video(leds, len, gamma);
    } else {
      ::napplyGamma_video(leds + len + 1, -len, gamma);
    }
    return *this;
  }

  inline CRGBSet & napplyGamma_video(float gammaR, float gammaG, float gammaB) {
    if(dir >= 0) {
      ::napplyGamma_video(leds, len, gammaR, gammaG, gammaB);
    } else {
      ::napplyGamma_video(leds + len + 1, -len, gammaR, gammaG, gammaB);
    }
    return *this;
  }
};

#endif
