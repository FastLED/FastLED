#ifndef __INC_PIXELS_H
#define __INC_PIXELS_H

#include <stdint.h>
#include "lib8tion.h"


struct CHSV {
	union {
		struct {
		    union {
		        uint8_t hue;
		        uint8_t h; };
		    union {
		        uint8_t saturation;
		        uint8_t sat;
		        uint8_t s; };
		    union {
		        uint8_t value;
		        uint8_t val;
		        uint8_t v; };
		};
		uint8_t raw[3];
	};
};


struct CRGB {
	union {
		struct {
            union {
                uint8_t r;
                uint8_t red;
            };
            union {
                uint8_t g;
                uint8_t green;
            };
            union {
                uint8_t b;
                uint8_t blue;
            };
        };
		uint8_t raw[3];
	};

	inline uint8_t& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return raw[x];
    }

    // default values are UNINITIALIZED
	inline CRGB() __attribute__((always_inline))
    {
    }
    
    // allow construction from R, G, B
    inline CRGB( uint8_t ir, uint8_t ig, uint8_t ib)  __attribute__((always_inline))
        : r(ir), g(ig), b(ib)
    {
    }
    
    // allow copy construction
	inline CRGB(const CRGB& rhs) __attribute__((always_inline))
    {
        r = rhs.r;
        g = rhs.g;
        b = rhs.b;
    }
    
    // allow assignment from one RGB struct to another
	inline CRGB& operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        r = rhs.r;
        g = rhs.g;
        b = rhs.b;
        return *this;
    }    

    // allow assignment from R, G, and B
	inline CRGB& setRGB (uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline))
    {
        r = nr;
        g = ng;
        b = nb;
        return *this;
    }
    
#if 0
    // allow assignment from H, S, and V
	inline CRGB& setHSV (uint8_t nh, uint8_t ns, uint8_t nv) __attribute__((always_inline))
    {
        CHSV hsv;
        hsv.hue = nh;
        hsv.sat = ns;
        hsv.val = nv;
        hsv2rgb( hsv, *this);
        return *this;
    }
    
    // allow assignment from H, S, and V
	inline CRGB& setRainbowHSV (uint8_t nh, uint8_t ns, uint8_t nv) __attribute__((always_inline))
    {
        CHSV hsv;
        hsv.hue = nh;
        hsv.sat = ns;
        hsv.val = nv;
        rainbow2rgb( hsv, *this);
        return *this;
    }
#endif
    
    // add one RGB to another, saturating at 0xFF for each channel
    inline CRGB& operator+= (const CRGB& rhs )  __attribute__((always_inline))
    {
        r = qadd8( r, rhs.r);
        g = qadd8( g, rhs.g);
        b = qadd8( b, rhs.b);
        return *this;
    }
    
    // add a contstant to each channel, saturating at 0xFF
    inline CRGB& operator+= (uint8_t d )  __attribute__((always_inline))
    {
        r = qadd8( r, d);
        g = qadd8( g, d);
        b = qadd8( b, d);
        return *this;
    }
    
    // subtract one RGB from another, saturating at 0x00 for each channel
    inline CRGB& operator-= (const CRGB& rhs )  __attribute__((always_inline))
    {
        r = qsub8( r, rhs.r);
        g = qsub8( g, rhs.g);
        b = qsub8( b, rhs.b);
        return *this;
    }
    
    // subtract a constant from each channel, saturating at 0x00
    inline CRGB& operator-= (uint8_t d )  __attribute__((always_inline))
    {
        r = qsub8( r, d);
        g = qsub8( g, d);
        b = qsub8( b, d);
        return *this;
    }
    
    // subtract a constant of '1' from each channel, saturating at 0x00
    inline CRGB& operator-- ()  __attribute__((always_inline))
    {
        *(this) -= 1;
        return *this;
    }
    
    // subtract a constant of '1' from each channel, saturating at 0x00
    inline CRGB operator-- (int DUMMY_ARG)  __attribute__((always_inline))
    {
        CRGB retval(*this);
        --(*this);
        return retval;
    }

    // add a constant of '1' from each channel, saturating at 0xFF
    inline CRGB& operator++ ()  __attribute__((always_inline))
    {
        *(this) += 1;
        return *this;
    }
    
    // add a constant of '1' from each channel, saturating at 0xFF
    inline CRGB operator++ (int DUMMY_ARG)  __attribute__((always_inline))
    {
        CRGB retval(*this);
        ++(*this);
        return retval;
    }

    // divide each of the channels by a constant
    inline CRGB& operator/= (uint8_t d )  __attribute__((always_inline))
    {
        r /= d;
        g /= d;
        b /= d;
        return *this;
    }
    
    // multiply each of the channels by a constant,
    // saturating each channel at 0xFF
    inline CRGB& operator*= (uint8_t d )  __attribute__((always_inline))
    {
        r = qmul8( r, d);
        g = qmul8( g, d);
        b = qmul8( b, d);
        return *this;
    }

    // scale down a RGB to N 256ths of it's current brightness, using
    // 'video' dimming rules, which means that unless the scale factor is ZERO
    // each channel is guaranteed NOT to dim down to zero.  If it's already
    // nonzero, it'll stay nonzero, even if that means the hue shifts a little
    // at low brightness levels.
    inline CRGB& nscale8_video (uint8_t scaledown )  __attribute__((always_inline))
    {
        nscale8x3_video( r, g, b, scaledown);
        return *this;
    }
    
    // %= is a synonym for nscale8_video.  Think of it is scaling down
    // by "a percentage"
    inline CRGB& operator%= (uint8_t scaledown )  __attribute__((always_inline))
    {
        nscale8x3_video( r, g, b, scaledown);
        return *this;
    }

    // scale down a RGB to N 256ths of it's current brightness, using
    // 'plain math' dimming rules, which means that if the low light levels
    // may dim all the way to 100% black.
    inline CRGB& nscale8 (uint8_t scaledown )  __attribute__((always_inline))
    {
        nscale8x3( r, g, b, scaledown);
        return *this;
    }

    // "or" operator brings each channel up to the higher of the two values
    inline CRGB& operator|= (const CRGB& rhs )  __attribute__((always_inline))
    {
        if( rhs.r > r) r = rhs.r;
        if( rhs.g > g) g = rhs.g;
        if( rhs.b > b) b = rhs.b;
        return *this;
    }
    inline CRGB& operator|= (uint8_t d )  __attribute__((always_inline))
    {
        if( d > r) r = d;
        if( d > g) g = d;
        if( d > b) b = d;
        return *this;
    }
    
    // "and" operator brings each channel down to the lower of the two values
    inline CRGB& operator&= (const CRGB& rhs )  __attribute__((always_inline))
    {
        if( rhs.r < r) r = rhs.r;
        if( rhs.g < g) g = rhs.g;
        if( rhs.b < b) b = rhs.b;
        return *this;
    }
    inline CRGB& operator&= (uint8_t d )  __attribute__((always_inline))
    {
        if( d < r) r = d;
        if( d < g) g = d;
        if( d < b) b = d;
        return *this;
    }
    
    // this allows testing a CRGB for zero-ness
    inline operator bool() const __attribute__((always_inline))
    {
        return r || g || b;
    }
    
    // invert each channel
    inline CRGB operator- ()  __attribute__((always_inline))
    {
        CRGB retval;
        retval.r = 255 - r;
        retval.g = 255 - g;
        retval.b = 255 - b;
        return retval;
    }
    
    
    inline uint8_t getLuma ( )  {
        //Y' = 0.2126 R' + 0.7152 G' + 0.0722 B'
        //     54            183       18 (!)
        
        uint8_t luma = scale8_LEAVING_R1_DIRTY( r, 54) + \
        scale8_LEAVING_R1_DIRTY( g, 183) + \
        scale8_LEAVING_R1_DIRTY( b, 18);
        cleanup_R1();
        return luma;
    }
    
    inline uint8_t getAverageLight( )  {
        const uint8_t eightysix = 86;
        uint8_t avg = scale8_LEAVING_R1_DIRTY( r, eightysix) + \
        scale8_LEAVING_R1_DIRTY( g, eightysix) + \
        scale8_LEAVING_R1_DIRTY( b, eightysix);
        cleanup_R1();
        return avg;
    }
};


inline __attribute__((always_inline)) bool operator== (const CRGB& lhs, const CRGB& rhs)
{
    return (lhs.r == rhs.r) && (lhs.g == rhs.g) && (lhs.b == rhs.b);
}

inline __attribute__((always_inline)) bool operator!= (const CRGB& lhs, const CRGB& rhs)
{
    return !(lhs == rhs);
}

inline __attribute__((always_inline)) bool operator< (const CRGB& lhs, const CRGB& rhs)
{
    uint16_t sl, sr;
    sl = lhs.r + lhs.g + lhs.b;
    sr = rhs.r + rhs.g + rhs.b;
    return sl < sr;
}

inline __attribute__((always_inline)) bool operator> (const CRGB& lhs, const CRGB& rhs)
{
    uint16_t sl, sr;
    sl = lhs.r + lhs.g + lhs.b;
    sr = rhs.r + rhs.g + rhs.b;
    return sl > sr;
}

inline __attribute__((always_inline)) bool operator>= (const CRGB& lhs, const CRGB& rhs)
{
    uint16_t sl, sr;
    sl = lhs.r + lhs.g + lhs.b;
    sr = rhs.r + rhs.g + rhs.b;
    return sl >= sr;
}

inline __attribute__((always_inline)) bool operator<= (const CRGB& lhs, const CRGB& rhs)
{
    uint16_t sl, sr;
    sl = lhs.r + lhs.g + lhs.b;
    sr = rhs.r + rhs.g + rhs.b;
    return sl <= sr;
}


__attribute__((always_inline))
inline CRGB operator+( const CRGB& p1, const CRGB& p2)
{
    return CRGB( qadd8( p1.r, p2.r),
                 qadd8( p1.g, p2.g),
                 qadd8( p1.b, p2.b));
}

__attribute__((always_inline))
inline CRGB operator-( const CRGB& p1, const CRGB& p2)
{
    return CRGB( qsub8( p1.r, p2.r),
                 qsub8( p1.g, p2.g),
                 qsub8( p1.b, p2.b));
}

__attribute__((always_inline))
inline CRGB operator*( const CRGB& p1, uint8_t d)
{
    return CRGB( qmul8( p1.r, d),
                 qmul8( p1.g, d),
                 qmul8( p1.b, d));
}

__attribute__((always_inline))
inline CRGB operator/( const CRGB& p1, uint8_t d)
{
    return CRGB( p1.r/d, p1.g/d, p1.b/d);
}

    
__attribute__((always_inline))
inline CRGB operator&( const CRGB& p1, const CRGB& p2)
{
    return CRGB( p1.r < p2.r ? p1.r : p2.r,
                 p1.g < p2.g ? p1.g : p2.g,
                 p1.b < p2.b ? p1.b : p2.b);
}
    
__attribute__((always_inline))
inline CRGB operator|( const CRGB& p1, const CRGB& p2)
{
    return CRGB( p1.r > p2.r ? p1.r : p2.r,
                 p1.g > p2.g ? p1.g : p2.g,
                 p1.b > p2.b ? p1.b : p2.b);
}

__attribute__((always_inline))
inline CRGB operator%( const CRGB& p1, uint8_t d)
{
    CRGB retval( p1);
    retval.nscale8_video( d);
    return retval;
}



#ifdef SUPPORT_ARGB
struct CARGB {
	union {
		struct {
            union {
                uint8_t a;
                uint8_t alpha;
            };
            union {
                uint8_t r;
                uint8_t red;
            };
            union {
                uint8_t g;
                uint8_t green;
            };
            union {
                uint8_t b;
                uint8_t blue;
            };
        };
		uint8_t raw[4];
		uint32_t all32;
	};
    
    // TODO: port various operators from CRGB to CARGB
};

#endif


// Define RGB orderings
enum EOrder {
	RGB=0012,
	RBG=0021,
	GRB=0102,
	GBR=0120,
	BRG=0201,
	BGR=0210
};


#endif
