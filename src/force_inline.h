#pragma once

// Please note that on Attiny85 etc... this compiler is like
// honey badger when it comes to inline. It doesn't care what
// you declare as forced inline as it will completely ignore whatever
// you tell it to do and do what it wants instead to compact code
// size to the absolute minimum that it can. And this is a good thing
// because the size reduction used by this compiler is world class...
// and it needs to be this way or things won't fit in the extremely
// tiny memory size that it has available.
#define FASTLED_FORCE_INLINE __attribute__((always_inline)) inline
