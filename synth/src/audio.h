#ifndef _audio_h_
#define _audio_h_

#define NOTE_SIZE_MAX 3
#define PARTS_PER_BEAT 24
#define TICKS_PER_PART 2


double calc_samples_per_tick(double bpm, double sample_rate);

#endif