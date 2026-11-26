#include <stdint.h>

#include "util.h"

// xorshift128+ implementation following this paper:
// https://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf

prng_state_s bbxs__random_seeded_state(uint64_t seed) {
    prng_state_s state;
    static const uint64_t JUMP[] = {0x8a5cd789635d2dff,
                                    0x121fd2155c472f96};
    uint64_t s0 = seed + 140242;
    uint64_t s1 = (seed + 95309) * s0 + 493;

    for (int i = 0; i < sizeof JUMP / sizeof *JUMP; i++) {
        for (int b = 0; b < 64; b++) {
            if (JUMP[i] & 1ULL << b) {
                s0 ^= state.s[0];
                s1 ^= state.s[1];
            }
            bbxs__random(&state);
        }
    }

    state.s[0] = s0;
    state.s[1] = s1;

    return state;
}

uint64_t bbxs__random(prng_state_s *state) {
    uint64_t s1 = state->s[0];
    const uint64_t s0 = state->s[1];
    const uint64_t result = s0 + s1;
    state->s[0] = s0;
    s1 ^= s1 << 23;                                          // a
    state->s[1] = state->s[1] ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
    return result;
}

#define RAND_PRECISION 0xffffffff
double bbxs__frandom(prng_state_s *state) {
    return (double)(bbxs__random(state) % RAND_PRECISION) / RAND_PRECISION;
}