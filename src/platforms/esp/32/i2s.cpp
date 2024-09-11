#include "i2s.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

int pgcd(int smallest, int precision, int a, int b, int c) {
    int pgc_ = 1;
    for (int i = smallest; i > 0; --i) {

        if (a % i <= precision && b % i <= precision && c % i <= precision) {
            pgc_ = i;
            break;
        }
    }
    return pgc_;
}

FASTLED_NAMESPACE_END
