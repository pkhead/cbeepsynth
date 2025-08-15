#include "filtering.h"
#include "util.h"
#include "../include/beepbox_synth.h"

#include <math.h>
#include <assert.h>

static double get_filter_freq_max(void) {
    static double cache;
    static uint8_t cache_needs_init = TRUE;

    if (cache_needs_init) {
        cache_needs_init = FALSE;
        cache = FILTER_FREQ_REFERENCE_HZ * pow(2.0, FILTER_FREQ_STEP * (BPBXSYN_FILTER_FREQ_RANGE - 1 - BPBXSYN_FILTER_FREQ_REFERENCE_SETTING)); // ~19khz
    }

    return cache;
}

#define FILTER_FREQ_MAX_HZ (get_filter_freq_max())

double get_hz_from_setting_value(double value) {
    return FILTER_FREQ_REFERENCE_HZ * pow(2.0, (value - BPBXSYN_FILTER_FREQ_REFERENCE_SETTING) * FILTER_FREQ_STEP);
}

static double filter_get_linear_gain(const filter_group_s *group, int index, double peak_mult) {
    const double power = (group->gain_idx[index] - BPBXSYN_FILTER_GAIN_CENTER) * FILTER_GAIN_STEP;
    const double neutral = (group->type[index] == BPBXSYN_FILTER_TYPE_NOTCH) ? 0.0 : -0.5;
    const double interpolated_power = neutral + (power - neutral) * peak_mult;
    return pow(2.0, interpolated_power);
}

filter_coefs_s filter_to_coefficients(
    const filter_group_s *group, int index,
    double sample_rate, double freq_mult, double peak_mult)
{
    const double hz = get_hz_from_setting_value(group->freq_idx[index]);
    const double corner_radians_per_sample = PI2 * clampd(freq_mult * hz, FILTER_FREQ_MIN_HZ, FILTER_FREQ_MAX_HZ) / sample_rate;
    const double linear_gain = filter_get_linear_gain(group, index, peak_mult);
    filter_coefs_s filter = {0};

    switch (group->type[index]) {
        case BPBXSYN_FILTER_TYPE_LP:
            filter_lp2bw(&filter, corner_radians_per_sample, linear_gain);
            break;

        case BPBXSYN_FILTER_TYPE_HP:
            filter_hp2bw(&filter, corner_radians_per_sample, linear_gain);
            break;

        case BPBXSYN_FILTER_TYPE_NOTCH:
            filter_peak2(&filter, corner_radians_per_sample, linear_gain, 1.0);
            break;
        
        case BPBXSYN_FILTER_TYPE_OFF:
            filter.a[0] = 1.0;
            filter.a[1] = 0.0;
            filter.a[2] = 0.0;

            filter.b[0] = 1.0;
            filter.b[1] = 0.0;
            filter.b[2] = 0.0;
            break;

        default:
            assert(FALSE && "filter_to_coefficients: invalid filter type");
            break;
    }

    return filter;
}

double filter_get_volume_compensation_mult(const filter_group_s *group, int index) {
    const double freq = group->freq_idx[index];
    const double gain = group->gain_idx[index];
    const uint8_t type = group->type[index];

    const double octave = (freq - BPBXSYN_FILTER_FREQ_REFERENCE_SETTING) * FILTER_FREQ_STEP;
    const double gain_pow = (gain - BPBXSYN_FILTER_GAIN_CENTER) * FILTER_GAIN_STEP;

    switch (type) {
        case BPBXSYN_FILTER_TYPE_LP: {
            const double freq_relative_to_8khz = pow(2.0, octave) * FILTER_FREQ_REFERENCE_HZ / 8000.0;

            // Reverse the frequency warping from importing legacy simplified filters to imitate how the legacy filter cutoff setting affected volume.
            const double warped_freq = (sqrt(1.0 + 4.0 * freq_relative_to_8khz) - 1.0) / 2.0;
            // const double warped_octave = log2(warped_freq);
            return pow(
                0.5,
                0.2 * max(0.0, gain_pow + 1.0) + min(0.0, max(-3.0, 0.595 * warped_freq + 0.35 * min(0.0, gain_pow + 1.0))));
        }
            
        case BPBXSYN_FILTER_TYPE_HP: {
            return pow(
                0.5,
                0.125 * max(0.0, gain_pow + 1.0) +
                    min(0.0, 0.3 * (-octave - log2(FILTER_FREQ_REFERENCE_HZ / 125.0)) + 0.2 * min(0.0, gain_pow + 1.0)));
        }
            
        case BPBXSYN_FILTER_TYPE_NOTCH: {
            const double distance_from_center = octave + log2(FILTER_FREQ_REFERENCE_HZ / 2000.0);
            const double freq_loudness = pow(1.0 / (1.0 + pow(distance_from_center / 3.0, 2.0)), 2.0);
            return pow(
                0.5,
                0.125 * max(0.0, gain_pow) + 0.1 * freq_loudness * min(0.0, gain_pow));
        }
            
        // error case
        default:
            assert(FALSE && "filter_get_compensation_mult: invalid filter type");
            return 1.0;
    }
}

// low-pass 2nd-order butterworth
void filter_lp2bw(filter_coefs_s *coefs, double corner_radians_per_sample, double peak_linear_gain) {
    // This is Butterworth if peakLinearGain=1/√2 according to:
    // http://web.archive.org/web/20191213120120/https://crypto.stanford.edu/~blynn/sound/analog.html
    // An interesting property is that if peakLinearGain=1/16 then the
    // output resembles a first-order lowpass at a cutoff 4 octaves lower,
    // although it gets distorted near the nyquist.
    const double alpha = sin(corner_radians_per_sample) / (2.0 * peak_linear_gain);
    const double cosv = cos(corner_radians_per_sample);
    const double a0 = 1.0 + alpha;

    coefs->a[0] = 1.0;
    coefs->a[1] = -2.0 * cosv / a0;
    coefs->a[2] = (1.0 - alpha) / a0;
    coefs->b[2] = coefs->b[0] = (1.0 - cosv) / (2.0 * a0);
    coefs->b[1] = (1.0 - cosv) / a0;
    // coefs->order = 2;
}

// high-pass 2nd-order butterworth
void filter_hp2bw(filter_coefs_s *coefs, double corner_radians_per_sample, double peak_linear_gain) {
    const double alpha = sin(corner_radians_per_sample) / (2.0 * peak_linear_gain);
    const double cosv = cos(corner_radians_per_sample);
    const double a0 = 1.0 + alpha;

    coefs->a[0] = 1.0;
    coefs->a[1] = -2.0 * cosv / a0;
    coefs->a[2] = (1.0 - alpha) / a0;
    coefs->b[2] = coefs->b[0] = (1.0 + cosv) / (2.0*a0);
    coefs->b[1] = -(1.0 + cosv) / a0;
    // coefs->order = 2;
}

// peak 2nd-order
void filter_peak2(filter_coefs_s *coefs, double corner_radians_per_sample, double peak_linear_gain, double bw_scale) {
    const double sqrt_gain = sqrt(peak_linear_gain);
    const double bandwidth = bw_scale * corner_radians_per_sample / (sqrt_gain >= 1.0 ? sqrt_gain : 1.0 / sqrt_gain);
    const double alpha = tan(bandwidth * 0.5);
    const double a0 = 1.0 + alpha / sqrt_gain;

    coefs->a[0] = 1.0;
    coefs->b[0] = (1.0 + alpha * sqrt_gain) / a0;
    coefs->b[1] = coefs->a[1] = -2.0 * cos(corner_radians_per_sample) / a0;
    coefs->b[2] = (1.0 - alpha * sqrt_gain) / a0;
    coefs->a[2] = (1.0 - alpha / sqrt_gain) / a0;
    // coefs->order = 2;
}

void dyn_biquad_reset_output(dyn_biquad_s *self) {
    self->output1 = 0.0;
    self->output2 = 0.0;
}

void dyn_biquad_load(
    dyn_biquad_s *self, filter_coefs_s start, filter_coefs_s end,
    double delta_rate, uint8_t use_multiplicative_input_coefficients)
{
    self->a1 = start.a[1];
    self->a2 = start.a[2];
    self->b0 = start.b[0];
    self->b1 = start.b[1];
    self->b2 = start.b[2];
    self->a1_delta = (end.a[1] - start.a[1]) * delta_rate;
    self->a2_delta = (end.a[2] - start.a[2]) * delta_rate;

    if (use_multiplicative_input_coefficients) {
        self->b0_delta = pow(end.b[0] / start.b[0], delta_rate);
        self->b1_delta = pow(end.b[1] / start.b[1], delta_rate);
        self->b2_delta = pow(end.b[2] / start.b[2], delta_rate);
    } else {
        self->b0_delta = (end.b[0] - start.b[0]) * delta_rate;
        self->b1_delta = (end.b[1] - start.b[1]) * delta_rate;
        self->b2_delta = (end.b[2] - start.b[2]) * delta_rate;
    }

    self->use_multiplicative_input_coefficients = use_multiplicative_input_coefficients;
}

// compute group of 2nd-order filters
double apply_filters(double sample, double input1, double input2, dyn_biquad_s filters[FILTER_GROUP_COUNT]) {
    for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
        dyn_biquad_s *filter = &filters[i];
        if (!filter->enabled) continue;

        const double output1 = filter->output1;
        const double output2 = filter->output2;
        const double a1 = filter->a1;
        const double a2 = filter->a2;
        const double b0 = filter->b0;
        const double b1 = filter->b1;
        const double b2 = filter->b2;

        sample = b0 * sample + b1 * input1 + b2 * input2 - a1 * output1 - a2 * output2;

        filter->a1 = a1 + filter->a1_delta;
        filter->a2 = a2 + filter->a2_delta;
        if (filter->use_multiplicative_input_coefficients) {
            filter->b0 = b0 * filter->b0_delta;
            filter->b1 = b1 * filter->b1_delta;
            filter->b2 = b2 * filter->b2_delta;
        } else {
            filter->b0 = b0 + filter->b0_delta;
            filter->b1 = b1 + filter->b1_delta;
            filter->b2 = b2 + filter->b2_delta;
        }

        filter->output2 = output1;
        filter->output1 = sample;
        // Updating the input values is waste if the next filter doesn't exist...
        input2 = output2;
        input1 = output1;
    }

    return sample;
}

bpbxsyn_complex_s filter_analyze_complex(filter_coefs_s coefs, double real, double imag) {
    const double *a = coefs.a;
    const double *b = coefs.b;
    const double real_z1 = real;
    const double imag_z1 = -imag;
    double real_num = b[0] + b[1] * real_z1;
    double imag_num = b[1] * imag_z1;
    double real_denom = 1.0 + a[1] * real_z1;
    double imag_denom = a[1] * imag_z1;
    double real_z = real_z1;
    double imag_z = imag_z1;
    for (int i = 2; i <= 2; i++) {
        const double real_temp = real_z * real_z1 - imag_z * imag_z1;
        const double imag_temp = real_z * imag_z1 + imag_z * real_z1;
        real_z = real_temp;
        imag_z = imag_temp;
        real_num += b[i] * real_z;
        imag_num += b[i] * imag_z;
        real_denom += a[i] * real_z;
        imag_denom += a[i] * imag_z;
    }

    // sqrt(r*r + i*i) / d
    // (r*r + i*i)^(0.5) * (1/d)
    // (r*r + i*i)^(0.5) * ((1/d)^2)^(0.5)
    // ( (r^2 + i^2) * ((1/d)^2) )^(0.5)
    // 
    // r^2 * (1/d)^2
    // (r * (1/d))^2
    // (r / d)^2
    // ...then how come in the original code the denominator was stored separately?

    double final_denom = real_denom * real_denom + imag_denom * imag_denom;
    return (bpbxsyn_complex_s) {
        .real = (real_num * real_denom + imag_num * imag_denom) / final_denom,
        .imag = (imag_num * real_denom - real_num * imag_denom) / final_denom
    };
}

bpbxsyn_complex_s filter_analyze(filter_coefs_s coefs, double radians_per_sample) {
    return filter_analyze_complex(coefs, cos(radians_per_sample), sin(radians_per_sample));
}