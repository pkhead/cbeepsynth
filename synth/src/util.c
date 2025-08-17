#include "util.h"

// count leading zeroes
static int32_t clz32(int32_t x) {
    // TODO: intrinstic?
    int tz = 0;
    for (; x != 0; x >>= 1)
        ++tz;
    return 32 - tz;
}

int fitting_power_of_two(int x) { return 1 << (32 - clz32(x - 1)); }