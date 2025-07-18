#ifndef _filtering_h_
#define _filtering_h_

#include <stddef.h>
#include <stdint.h>
#include "../include/beepbox_synth.h"

#define FILTER_GROUP_COUNT 8
#define FILTER_FREQ_MIN_HZ 8.0
#define FILTER_FREQ_REFERENCE_HZ 8000.0
#define FILTER_FREQ_STEP ((1.0 / 4.0))
#define FILTER_GAIN_STEP ((1.0 / 2.0))

typedef struct {
    uint8_t type[FILTER_GROUP_COUNT];
    
    double freq_idx[FILTER_GROUP_COUNT]; // 0 - 33
    double gain_idx[FILTER_GROUP_COUNT]; // 0 - 14
} filter_group_s;

// filter coefficients for 2nd-order IIR filters
// a[0] is always unused, but assumed to be 1.0
// since the rest of the parameters are normalized according to it
typedef struct {
    double a[3];
    double b[3];
} filter_coefs_s;

typedef struct {
    double a1;
    double a2;
    double b0;
    double b1;
    double b2;

    double a1_delta;
    double a2_delta;
    double b0_delta;
    double b1_delta;
    double b2_delta;

    double output1;
    double output2;

    uint8_t use_multiplicative_input_coefficients;
    uint8_t enabled;
} dyn_biquad_s;

filter_coefs_s filter_to_coefficients(
    const filter_group_s *group, int index,
    double sample_rate, double freq_mult, double peak_mult);

double filter_get_volume_compensation_mult(const filter_group_s *group, int index);

// low-pass 2nd-order butterworth
void filter_lp2bw(filter_coefs_s *coefs, double corner_radians_per_sample, double peak_linear_gain);
// high-pass 2nd-order butterworth
void filter_hp2bw(filter_coefs_s *coefs, double corner_radians_per_sample, double linear_gain);
// peak 2nd-order
void filter_peak2(filter_coefs_s *coefs, double corner_radians_per_sample, double linear_gain, double bw_scale);

void dyn_biquad_reset_output(dyn_biquad_s *self);
// loadCoefficientsWithGradient
void dyn_biquad_load(
    dyn_biquad_s *self, filter_coefs_s start, filter_coefs_s end,
    double delta_rate, uint8_t use_multiplicative_input_coefficients);

double get_hz_from_setting_value(double value);

// compute group of 2nd-order filters
// input0: current sample (x[0])
// input1: last sample (x[-1])
// input2: sample before input1 (x[-2])
double apply_filters(double input0, double input1, double input2, dyn_biquad_s filters[FILTER_GROUP_COUNT]);

bpbx_freq_response_s filter_analyze_complex(filter_coefs_s coefs, double real, double imag);
bpbx_freq_response_s filter_analyze(filter_coefs_s coefs, double radians_per_sample);

#endif