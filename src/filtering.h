#ifndef _filtering_h_
#define _filtering_h_

#include <stddef.h>
#include <stdint.h>

#define FILTER_GROUP_COUNT 8

typedef struct {
    uint8_t type[FILTER_GROUP_COUNT];
    
    double freq_idx[FILTER_GROUP_COUNT]; // 0 - 33
    double gain_idx[FILTER_GROUP_COUNT]; // 0 - 14
} filter_group_s;

// 2nd-order IIR filters
typedef struct {
    // filter coefficients
    double a[3];
    double b[3];
} filter_coefs_s;

filter_coefs_s filter_to_coefficients(
    const filter_group_s *group, int index,
    double sample_rate, double freq_mult, double peak_mult);

double filter_get_volume_compensation_mult(const filter_group_s *group, int index);

// low-pass 2nd-order butterworth
void filter_lp2bw(filter_coefs_s *coefs, double corner_radians_per_sample, double peak_linear_gain);
// high-pass 2nd-order butterworth
void filter_hp2bw(filter_coefs_s *coefs, double sample_rate, double frequency, double linear_gain);
// peak 2nd-order
void filter_peak2nd(filter_coefs_s *coefs, double sample_rate, double frequency, double linear_gain, double bw_scale);

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

// loadCoefficientsWithGradient
void dyn_biquad_reset_output(dyn_biquad_s *self);
void dyn_biquad_load(
    dyn_biquad_s *self, filter_coefs_s start, filter_coefs_s end,
    double delta_rate, uint8_t use_multiplicative_input_coefficients);

// compute group of 2nd-order filters
double apply_filters(double input0, double input1, double input2, dyn_biquad_s filters[FILTER_GROUP_COUNT]);

#endif