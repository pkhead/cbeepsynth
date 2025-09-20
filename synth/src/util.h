#ifndef _util_h_
#define _util_h_

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "../include/beepbox_synth.h"

#define PI 3.14159265358979323846
#define PI2 (2.0 * PI)
#define PI2f (float)(2.f * 3.14159265358979323846f)

// I didn't know C99+ had a bool type I guess?
#define FALSE 0
#define TRUE 1

static inline float lerpf(float min, float max, float t) {
    return (max - min) * t + min;
}

static inline float clampf(float v, float min, float max) {
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

static inline double clampd(double v, double min, double max) {
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

static inline int clampi(int v, int min, int max) {
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

static inline int signf(float v) {
    if (v > 0.0f) return 1;
    if (v < 0.0f) return -1;
    return 0;
}

// wtf i'm not even including any windows headers
#undef min
#undef max

static inline double min(double a, double b) {
    return a < b ? a : b;
}

static inline double max(double a, double b) {
    return a > b ? a : b;
}

static inline double mini(int a, int b) {
    return a < b ? a : b;
}

static inline double maxi(int a, int b) {
    return a > b ? a : b;
}

static inline float key_to_hz_f(int key) {
    return powf(2.f, (key - 69) / 12.f) * 440.f;
}

static inline double key_to_hz_d(double key) {
    return pow(2.0, (key - 69) / 12.0) * 440.0;
}

int fitting_power_of_two(int x);

static inline bool is_power_of_two(unsigned int n) {
    return (n & (n - 1)) == 0;
}

// random number generator: xorshift128+
// implementation taken from the code snippet in this paper
// https://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
typedef struct prng_state {
    uint64_t s[2];
} prng_state_s;

prng_state_s random_seeded_state(uint64_t seed);
uint64_t random(prng_state_s *state);
double frandom(prng_state_s *state);

#endif