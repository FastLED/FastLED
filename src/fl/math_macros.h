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

#ifndef EPSILON_F
// smallest possible float
#define EPSILON_F  1.19209290e-07F
#endif

#ifndef EPSILON_D
// smallest possible double
#define EPSILON_D  2.2204460492503131e-16
#endif


#ifndef ALMOST_EQUAL
#define ALMOST_EQUAL(a,b,small) (ABS((a)-(b))<small)
#endif


#ifndef ALMOST_EQUAL_FLOAT
#define ALMOST_EQUAL_FLOAT(a,b) (ABS((a)-(b))<EPSILON_F)
#endif

#ifndef ALMOST_EQUAL_DOUBLE
#define ALMOST_EQUAL_DOUBLE(a,b) (ABS((a)-(b))<EPSILON_F)
#endif

#ifndef INFINITY_DOUBLE
#define INFINITY_DOUBLE (1.0 / 0.0)
#endif

#ifndef INFINITY_FLOAT
#define INFINITY_FLOAT (1.0f / 0.0f)
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef M_PI
#define M_PI PI
#endif