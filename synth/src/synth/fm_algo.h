#ifndef _fm_algo_h_
#define _fm_algo_h_
#include "fm.h"
#include "../wavetables.h"

typedef double (*fm_algo_f)(fm_voice_s *voice, const float sine_wave[SINE_WAVE_LENGTH+1], const double feedback_amp);
extern fm_algo_f fm_algorithm_table[234];
#endif
