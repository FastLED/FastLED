#pragma once

#define FASTLED_HAS_DBG 1
// todo move.
#include <iostream>
using std::endl;
using std::cout;
#define _FASTLED_DGB(X) cout << __FILE__ << "(" << __LINE__ << "): " << X
#define FASTLED_DBG(X) _FASTLED_DGB(X) << endl
