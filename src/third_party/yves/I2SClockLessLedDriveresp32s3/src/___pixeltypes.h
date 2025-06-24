#include "fl/stdint.h"
#include <string.h>

#ifdef USE_FASTLED
#include "FastLED.h"
#endif
// #include "Arduino.h"

#define _OUT_OF_BOUND -12

namespace fl {

#ifdef COLOR_RGBW

struct Pixel {
    union {
        uint8_t raw[4];
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t white;
        };
    };

    inline Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
        __attribute__((always_inline))
        : red(r), green(g), blue(b), white(w) {
        // brigthness =0xE0 |(br&31);
    }

    inline Pixel(uint8_t r, uint8_t g, uint8_t b) __attribute__((always_inline))
        : red(r), green(g), blue(b) {
        white = MIN(red, green);
        white = MIN(white, blue);
        red = red - white;
        green = green - white;
        blue = blue - white;
    }

    inline Pixel() __attribute__((always_inline)) {}

#ifdef USE_FASTLED
    inline Pixel &operator=(const CRGB &rhs) __attribute__((always_inline)) {

        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        white = MIN(red, green);
        white = MIN(white, blue);
        red = red - white;
        green = green - white;
        blue = blue - white;
        return *this;
    }
#endif

    inline Pixel(const Pixel &rhs) __attribute__((always_inline)) {
        // brigthness=rhs.brigthness;
        red = rhs.red;
        green = rhs.green;
        blue = rhs.blue;
        white = rhs.white;
    }
    inline Pixel &operator=(const uint32_t colorcode)
        __attribute__((always_inline)) {
        // rgb colorg;
        red = (colorcode >> 24) & 0xFF;
        green = (colorcode >> 16) & 0xFF;
        blue = (colorcode >> 8) & 0xFF;
        white = colorcode & 0xFF;
        return *this;
    }
};
#else

struct Pixel {
    union {
        uint8_t raw[3];
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
        };
    };

    inline Pixel(uint8_t r, uint8_t g, uint8_t b) __attribute__((always_inline))
        : red(r), green(g), blue(b) {
        // brigthness =0xE0 |(br&31);
    }

    inline Pixel() __attribute__((always_inline)) {}

#ifdef USE_FASTLED
    inline Pixel &operator=(const CRGB &rhs) __attribute__((always_inline)) {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
    inline Pixel &operator=(CRGB &rhs) __attribute__((always_inline)) {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
    inline Pixel(const CRGB &rhs) __attribute__((always_inline)) {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
    }
#endif

    inline Pixel(const Pixel &rhs) __attribute__((always_inline)) {
        // brigthness=rhs.brigthness;
        red = rhs.red;
        green = rhs.green;
        blue = rhs.blue;
    }
    inline Pixel &operator=(const uint32_t colorcode)
        __attribute__((always_inline)) {
        // rgb colorg;
        red = (colorcode >> 16) & 0xFF;
        green = (colorcode >> 8) & 0xFF;
        blue = (colorcode >> 0) & 0xFF;
        return *this;
    }
    inline __attribute__((always_inline)) bool operator==(const Pixel &rhs) {
        return (red == rhs.red) && (green == rhs.green) && (blue == rhs.blue);
    }
    inline __attribute__((always_inline)) bool operator!=(const Pixel &rhs) {
        return !((red == rhs.red) && (green == rhs.green) &&
                 (blue == rhs.blue));
    }
};

#endif

enum class leddirection { FORWARD, BACKWARD, MAP };

class Pixels {
  public:
    inline Pixels() __attribute__((always_inline)) {}
    inline Pixels(const Pixels &rhs) __attribute__((always_inline)) {
        _size = rhs._size;
        _direction = rhs._direction;
        _num_strips = rhs._num_strips;
        for (int i = 0; i < _num_strips; i++) {
            _sizes[i] = rhs._sizes[i];
        }
        ledpointer = rhs.ledpointer;
        mapFunction = rhs.mapFunction;

        // parent=rhs.parent;
    }
    Pixels(int size, Pixel *ledpoi) {
        Pixels(size, ledpoi, leddirection::FORWARD);
    }

    Pixels(int size, Pixel *ledpoi, leddirection direction) {
        __Pixels(size, ledpoi, direction, this);
    }

    void __Pixels(int size, Pixel *ledpoi, leddirection direction,
                  Pixels *pib) {
        pib->_size = size;
        pib->ledpointer = ledpoi;
        pib->_num_strips = 0;
        pib->_direction = direction;
        //  pib->nb_child=0;
    }

    Pixels(int num_led_per_strip, int num_strips) {
        int sizes[16];
        for (int i = 0; i < num_strips; i++) {
            sizes[i] = num_led_per_strip;
        }
        __Pixels(sizes, num_strips, leddirection::FORWARD, this);
    }

    Pixels(int *sizes, int num_strips) {
        __Pixels(sizes, num_strips, leddirection::FORWARD, this);
    }

    Pixels(int *sizes, int num_strips, leddirection direction) {
        __Pixels(sizes, num_strips, direction, this);
    }
    void __Pixels(int *sizes, int num_strips, leddirection direction,
                  Pixels *pib) {
        int size = 0;
        for (int i = 0; i < num_strips; i++) {
            size += sizes[i];
            pib->_sizes[i] = sizes[i];
        }

        pib->_num_strips = num_strips;

        ledpointer = (Pixel *)calloc(size, sizeof(Pixel));
        if (ledpointer == NULL) {
            pib->_size = 0;
        } else {
            pib->_size = size;
        }
        pib->_direction = direction;
    }
    Pixel &operator[](int i) {
        switch (_direction) {

        case (leddirection::FORWARD):

            return *(ledpointer + i % _size);
            break;

        case (leddirection::BACKWARD):

            return *(ledpointer + (_size - i % (_size)-1));
            break;

        case (leddirection::MAP):
            if (mapFunction) {
                int offset = mapFunction(i, arguments);
                // printf("%d %d\n",i,offset);
                if (offset == _OUT_OF_BOUND) {
                    return offPixel;
                } else
                    return *(ledpointer + (mapFunction(i, arguments) % _size));
            }

            else
                return *(ledpointer);
            break;
        default:
            return *(ledpointer);
            break;
        }
    }

    void copy(Pixels ori) { copy(ori, leddirection::FORWARD); }

    void copy(Pixels ori, leddirection dir) {
        leddirection ledd = _direction;
        if (_direction == leddirection::MAP)
            ledd = leddirection::FORWARD;
        for (int i = 0; i < ori._size; i++) {
            if (ledd == dir) {
                (*this)[i] = ori[i];
            } else {
                (*this)[i] = ori[ori._size - i % (ori._size) - 1];
            }
        }
    }

    Pixels getStrip(int num_strip, leddirection direction) {
        if (_num_strips == 0 or _num_strips < num_strip) {

            int d[0];
            return Pixels(d, 1, direction);
        } else {
            uint32_t off = 0;
            for (int i = 0; i < num_strip % _num_strips; i++) {
                off += _sizes[i];
            }

            return Pixels(_sizes[num_strip], ledpointer + off, direction);
        }
    }

    Pixels getStrip(int num_strip) {
        return getStrip(num_strip, leddirection::FORWARD);
    }

    int *getLengths() { return _sizes; }

    int getNumStrip() { return _num_strips; }
    uint8_t *getPixels() { return (uint8_t *)ledpointer; }
    void clear() {
        // memset(ledpointer,0,_size*sizeof(Pixel));
    }

    Pixels createSubset(int start, int length) {
        return createSubset(start, length, leddirection::FORWARD);
    }

    Pixels createSubset(int start, leddirection direction) {
        if (start < 0)
            start = 0;
        return Pixels(_size, ledpointer + start, direction);
    }

    Pixels createSubset(int start, int length, leddirection direction) {
        if (start < 0)
            start = 0;
        if (length <= 0)
            length = 1;
        return Pixels(length, ledpointer + start, direction);
    }
    /*
        Pixels getParent()
        {
            return *parent;
        }

        Pixels * getChild(int i)
        {

            return children[i%nb_child];
        }
        */
    inline void setMapFunction(int (*fptr)(int i, void *args), void *args,
                               int size) {
        mapFunction = fptr;
        if (arguments == NULL)
            arguments = (void *)malloc(sizeof(size));
        memcpy(arguments, args, size);
    }

  private:
    Pixel *ledpointer;
    size_t _size = 0;
    int _sizes[16];
    int _num_strips = 0;
    leddirection _direction;
    // int nb_child;
    //  Pixels *parent;
    void *arguments;
    // Pixels **children;
    int (*mapFunction)(int i, void *args);
    /*
     * this is the pixel to retuen when out of bound
     */
    Pixel offPixel;
};

} // namespace fl
