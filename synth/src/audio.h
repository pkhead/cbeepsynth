#ifndef _audio_h_
#define _audio_h_

#define NOTE_SIZE_MAX 3
#define PARTS_PER_BEAT 24
#define TICKS_PER_PART 2

// For detecting and avoiding float denormals, which have poor performance.
#define FLUSH_ZERO_EPSILON 1e-23

double calc_samples_per_tick(double bpm, double sample_rate);

void sanitize_delay_line(float *buffer, int last_index, int mask);

#endif