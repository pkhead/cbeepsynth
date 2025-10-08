#ifndef _fm_algo_h_
#define _fm_algo_h_
#include "fm.h"
#include "../wavetables.h"

static inline double fm_calc_op(const float sine_wave[SINE_WAVE_LENGTH+1],
                                const double phase_mix) {
    const int phase_int = (int) phase_mix;
    const int index = phase_int & (SINE_WAVE_LENGTH - 1);
    const double sample = sine_wave[index];
    return sample + (sine_wave[index+1] - sample) * (phase_mix - phase_int);
}

typedef double (*fm_algo_f)(fm_voice_s *voice, const float sine_wave[SINE_WAVE_LENGTH+1], const double feedback_amp);
extern fm_algo_f bbsyn_fm_algorithm_table[234];
#endif
