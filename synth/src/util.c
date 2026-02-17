#include "util.h"

prng_state_s bbsyn_random_seeded_state(uint64_t seed) {
    return (prng_state_s) {
        .s[0] = seed,
        .s[1] = seed
    };
}

// implementation taken from the code snippet in this paper
// https://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
uint64_t bbsyn_random(prng_state_s *state) {
    uint64_t s1 = state->s[0];
    const uint64_t s0 = state->s[1];
    const uint64_t result = s0 + s1;
    state->s[0] = s0;
    s1 ^= s1 << 23; // a
    state->s[1] = state->s[1] ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
    return result;
}

#define RAND_PRECISION 0xffffffff
double bbsyn_frandom(prng_state_s *state) {
    return (double)(bbsyn_random(state) % RAND_PRECISION) / RAND_PRECISION;
}