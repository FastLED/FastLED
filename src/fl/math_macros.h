#pragma once

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef ABS
#define ABS(x) ((x)>0?(x):-(x))
#endif

#ifndef ALMOST_EQUAL
#define ALMOST_EQUAL(a,b,small) (ABS((a)-(b))<small)
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
