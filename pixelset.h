#ifndef __INC_PIXELSET_H
#define __INC_PIXELSET_H

/// Represents a set of CRGB led objects.  Provides the [] array operator, and works like a normal array in that case.
/// This should be kept in sync with the set of functions provided by CRGB as well as functions in colorutils.
class CPixelSet {
public:
  const int8_t  dir;
  const int   len;
  CRGB * const leds;
  CRGB * const end_pos;

public:

  inline CPixelSet(const CPixelSet & other) : leds(other.leds), len(other.len), dir(other.dir), end_pos(other.end_pos) {}
  inline CPixelSet(CRGB *_leds, int _len) : leds(_leds), len(_len), dir(_len < 0 ? -1 : 1), end_pos(_leds + _len) {}
  inline CPixelSet(CRGB *_leds, int _start, int _end) : leds(_leds), dir(((_end-_start)<0) ? -1 : 1), len((_end - _start) + dir), end_pos(_leds + len) {}

  /// what's the size of this set?
  int size() { return abs(len); }

  /// is this set reversed?
  bool reversed() { return len < 0; }

  /// do these sets point to the same thing (note, this is different from the contents of the set being the same)
  bool operator==(const CPixelSet rhs) const { return leds == rhs.leds && len == rhs.len && dir == rhs.dir; }

  /// do these sets point to the different things (note, this is different from the contents of the set being the same)
  bool operator!=(const CPixelSet rhs) const { return leds != rhs.leds || len != rhs.len || dir != rhs.dir; }


  /// access a single element in this set, just like an array operator
  inline CRGB & operator[](int x) const { if(dir & 0x80) { return leds[-x]; } else { return leds[x]; } }

  /// Access an inclusive subset of the leds in this set.  Note that start can be greater than end, which will
  /// result in a reverse ordering for many functions (useful for mirroring)
  /// @param start the first element from this set for the new subset
  /// @param end the last element for the new subset
  inline CPixelSet operator()(int start, int end) { return CPixelSet(leds+start, start, end); }

  /// Access an inclusive subset of the leds in this set, starting from the first.
  /// @param end the last element for the new subset
  /// Not sure i want this? inline CPixelSet operator()(int end) { return CPixelSet(leds, 0, end); }

  /// Return the reverse ordering of this set
  inline CPixelSet operator-() { return CPixelSet(leds + len - dir, len - dir, 0); }

  /// Return a pointer to the first element in this set
  inline operator CRGB* () const { return leds; }

  /// Assign the passed in color to all elements in this set
  /// @param color the new color for the elements in the set
  inline CPixelSet & operator=(const CRGB & color) {
    for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) = color; }
    return *this;
  }

  void dump() const {
    Serial.print("len: "); Serial.print(len); Serial.print(", dir:"); Serial.print((int)dir);
    Serial.print(", range:"); Serial.print((uint32_t)leds); Serial.print("-"); Serial.print((uint32_t)end_pos);
    Serial.print(", diff:"); Serial.print((int32_t)(end_pos - leds));
 }
  /// Copy the contents of the passed in set to our set.  Note if one set is smaller than the other, only the
  /// smallest number of items will be copied over.
  inline CPixelSet & operator=(const CPixelSet & rhs) {
    for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) {
      (*pixel) = (*rhspixel);
    }
    return *this;
  }

  // modification/scaling operators
  inline CPixelSet & addToRGB(uint8_t inc) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) += inc; } return *this; }
  inline CPixelSet & operator+=(CPixelSet & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) += (*rhspixel); } return *this; }

  inline CPixelSet & subFromRGB(uint8_t inc) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) -= inc; } return *this; }
  inline CPixelSet & operator-=(CPixelSet & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) -= (*rhspixel); } return *this; }

  inline CPixelSet & operator++() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)++; } return *this; }
  inline CPixelSet & operator++(int DUMMY_ARG) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)++; } return *this; }

  inline CPixelSet & operator--() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)--; } return *this; }
  inline CPixelSet & operator--(int DUMMY_ARG) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)--; } return *this; }

  inline CPixelSet & operator/=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) /= d; } return *this; }
  inline CPixelSet & operator>>=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) >>= d; } return *this; }
  inline CPixelSet & operator*=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) *= d; } return *this; }

  inline CPixelSet & nscale8_video(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8_video(scaledown); } return *this;}
  inline CPixelSet & operator%=(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8_video(scaledown); } return *this; }
  inline CPixelSet & fadeLightBy(uint8_t fadefactor) { return nscale8_video(255 - fadefactor); }

  inline CPixelSet & nscale8(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8(scaledown); } return *this; }
  inline CPixelSet & nscale8(CRGB & scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8(scaledown); } return *this; }
  inline CPixelSet & nscale8(CPixelSet & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel).nscale8((*rhspixel)); } return *this; }

  inline CPixelSet & fadeToBlackBy(uint8_t fade) { return nscale8(255 - fade); }

  inline CPixelSet & operator|=(const CRGB & rhs) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) |= rhs; } return *this; }
  inline CPixelSet & operator|=(const CPixelSet & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) |= (*rhspixel); } return *this; }
  inline CPixelSet & operator|=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) |= d; } return *this; }

  inline CPixelSet & operator&=(const CRGB & rhs) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) &= rhs; } return *this; }
  inline CPixelSet & operator&=(const CPixelSet & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) &= (*rhspixel); } return *this; }
  inline CPixelSet & operator&=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) &= d; } return *this; }

  inline operator bool() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { if((*pixel)) return true; } return false; }

  // Color util functions
  inline CPixelSet & fill_solid(const CRGB & color) { if(dir>0) { ::fill_solid(leds, len, color); } else { ::fill_solid(leds + len + 1, -len, color); } return *this; }
  inline CPixelSet & fill_solid(const CHSV & color) { if(dir>0) { ::fill_solid(leds, len, color); } else { ::fill_solid(leds + len + 1, -len, color); } return *this; }

  inline CPixelSet & fill_rainbow(uint8_t initialhue, uint8_t deltahue=5) {
    if(dir >= 0) {
      ::fill_rainbow(leds,len,initialhue,deltahue);
    } else {
      ::fill_rainbow(leds+len+1,-len,initialhue,deltahue);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient(const CHSV & startcolor, const CHSV & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds,len,startcolor, endcolor, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, (-len), endcolor, startcolor, directionCode);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV &  c3, TGradientDirectionCode directionCode = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds, len, c1, c2, c3, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, -len, c3, c2, c1, directionCode);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV & c3, const CHSV & c4, TGradientDirectionCode directionCode = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient(leds, len, c1, c2, c3, c4, directionCode);
    } else {
      ::fill_gradient(leds + len + 1, -len, c4, c3, c2, c1, directionCode);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient_RGB(const CRGB & startcolor, const CRGB & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds,len,startcolor, endcolor);
    } else {
      ::fill_gradient_RGB(leds + len + 1, (-len), endcolor, startcolor);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient_RGB(const CRGB & c1, const CRGB & c2, const CRGB &  c3) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds, len, c1, c2, c3);
    } else {
      ::fill_gradient_RGB(leds + len + 1, -len, c3, c2, c1);
    }
    return *this;
  }

  inline CPixelSet & fill_gradient_RGB(const CRGB & c1, const CRGB & c2, const CRGB & c3, const CRGB & c4) {
    if(dir >= 0) {
      ::fill_gradient_RGB(leds, len, c1, c2, c3, c4);
    } else {
      ::fill_gradient_RGB(leds + len + 1, -len, c4, c3, c2, c1);
    }
    return *this;
  }

  inline CPixelSet & nblend(const CRGB & overlay, fract8 amountOfOverlay) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { ::nblend((*pixel), overlay, amountOfOverlay); } return *this; }
  inline CPixelSet & nblend(const CPixelSet & rhs, fract8 amountOfOverlay) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { ::nblend((*pixel), (*rhspixel), amountOfOverlay); } return *this; }

  // Note: only bringing in a 1d blur, not sure 2d blur makes sense when looking at sub arrays
  inline CPixelSet & blur1d(fract8 blur_amount) {
    if(dir >= 0) {
      ::blur1d(leds, len, blur_amount);
    } else {
      ::blur1d(leds + len + 1, -len, blur_amount);
    }
    return *this;
  }

  inline CPixelSet & napplyGamma_video(float gamma) {
    if(dir >= 0) {
      ::napplyGamma_video(leds, len, gamma);
    } else {
      ::napplyGamma_video(leds + len + 1, -len, gamma);
    }
    return *this;
  }

  inline CPixelSet & napplyGamma_video(float gammaR, float gammaG, float gammaB) {
    if(dir >= 0) {
      ::napplyGamma_video(leds, len, gammaR, gammaG, gammaB);
    } else {
      ::napplyGamma_video(leds + len + 1, -len, gammaR, gammaG, gammaB);
    }
    return *this;
  }

  // TODO: Make this a fully specified/proper iterator
  template <class T>
  class pixelset_iterator_base {
    T * leds;
    const int8_t dir;
  public:
    __attribute__((always_inline)) inline pixelset_iterator_base(const pixelset_iterator_base & rhs) : leds(rhs.leds), dir(rhs.dir) {}
    __attribute__((always_inline)) inline pixelset_iterator_base(T * _leds, const char _dir) : leds(_leds), dir(_dir) {}

    __attribute__((always_inline)) inline pixelset_iterator_base& operator++() { leds += dir; return *this; }
    __attribute__((always_inline)) inline pixelset_iterator_base operator++(int) { pixelset_iterator_base tmp(*this); leds += dir; return tmp; }

    __attribute__((always_inline)) inline bool operator==(pixelset_iterator_base & other) const { return leds == other.leds; } // && set==other.set; }
    __attribute__((always_inline)) inline bool operator!=(pixelset_iterator_base & other) const { return leds != other.leds; } // || set != other.set; }

    __attribute__((always_inline)) inline CRGB& operator*() const { return *leds; }
  };

  typedef pixelset_iterator_base<CRGB> iterator;
  typedef pixelset_iterator_base<const CRGB> const_iterator;

  iterator begin() { return iterator(leds, dir); }
  iterator end() { return iterator(end_pos, dir); }

  iterator begin() const { return iterator(leds, dir); }
  iterator end() const { return iterator(end_pos, dir); }

  const_iterator cbegin() const { return const_iterator(leds, dir); }
  const_iterator cend() const { return const_iterator(end_pos, dir); }
};

__attribute__((always_inline))
inline CRGB *operator+(const CPixelSet & pixels, int offset) { return (CRGB*)pixels + offset; }

typedef CPixelSet CRGBSet;

template<int SIZE>
class CRGBArray : public CPixelSet {
  CRGB rawleds[SIZE];
public:
  CRGBArray() : CPixelSet(rawleds, SIZE) {}
};

#endif
