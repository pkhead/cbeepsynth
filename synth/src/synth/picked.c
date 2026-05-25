#include <assert.h>
#include <string.h>
#include "../util.h"
#include "../param_util.h"
#include "../wavetables.h"
#include "../filtering.h"
#include "../alloc.h"
#include "../log.h"

#include "synth.h"

#if UNISON_MAX_VOICES != 2
#error picked.c assumes unison max voices to be 2
#endif

#define PICKED_BASE_EXPRESSION 0.025 // Same as harmonics.
#define STRING_DECAY_RATE 0.12
#define IMPULSE_WAVE_LENGTH ((HARMONICS_WAVE_LENGTH + 1))
#define DELAY_LINE_COUNT ((BPBXSYN_SYNTH_MAX_VOICES * UNISON_MAX_VOICES))

// Picked strings have an all-pass filter with a corner frequency based on the
// tone fundamental frequency, in order to add a slight inharmonicity. (Which is
// important for distortion.)

// The tone fundamental freq is pulled toward this freq for computing the
// all-pass corner freq.
#define STRING_DISPERSION_CENTER_FREQ 6000.0

// The tone fundamental freq freq moves this much toward the center freq for
// computing the all-pass corner freq.
#define STRING_DISPERSION_FREQ_SCALE 0.3
                                        
// The all-pass corner freq is based on this times the adjusted tone fundamental
// freq. 
#define STRING_DISPERSION_FREQ_MULT 4.0

// The cutoff freq of the shelf filter that is used to decay the high frequency
// energy in the picked string.
#define STRING_SHELF_HZ 4000.0

// i think string decay types are a scrapped feature
typedef enum string_sustain {
    STRING_SUSTAIN_BRIGHT,
    STRING_SUSTAIN_ACOUSTIC,
    STRING_SUSTAIN_LENGTH,
} string_sustain_e;

typedef struct pstring {
    float *delay_line;
    int delay_index;
    double all_pass_sample;
    double all_pass_prev_input;
    double sustain_filter_sample;
	double sustain_filter_prev_output2;
	double sustain_filter_prev_input1;
	double sustain_filter_prev_input2;
    double fractional_delay_sample;
    double prev_delay_length;
    double delay_length_delta;
    int delay_reset_offset;

    double all_pass_g;
    double all_pass_g_delta;
    double sustain_filter_a0_delta;
    double sustain_filter_a1;
	double sustain_filter_a1_delta;
	double sustain_filter_a2;
	double sustain_filter_a2_delta;
	double sustain_filter_b0;
	double sustain_filter_b0_delta;
	double sustain_filter_b1;
	double sustain_filter_b1_delta;
	double sustain_filter_b2;
	double sustain_filter_b2_delta;
} pstring_s;

typedef struct picked_voice {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];

    double sustain_start, sustain_end;

    double prev_pitch_expression;
    double prev_string_decay;

    bool has_prev_pitch_expression;
    bool has_prev_string_decay;
    bool at_note_start;

    pstring_s strings[UNISON_MAX_VOICES];
} picked_voice_s;

typedef struct picked_inst {
    bpbxsyn_synth_s base;

    uint8_t unison_type;
    uint8_t harmonics[BPBXSYN_HARMONICS_CONTROL_COUNT];
    uint8_t last_harmonics[BPBXSYN_HARMONICS_CONTROL_COUNT];

    // [0] = prev
    // [1] = current
    double sustain[2];

    float impulse_wave[IMPULSE_WAVE_LENGTH];
    
    // TODO: what is a good size for the delay line?
    picked_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];
    float *delay_line_alloc; // a continguous list of delay lines, of size
                             // delay_line_size
    int delay_line_size;
} picked_inst_s;

static void pstring_reset(pstring_s *self)
{
    self->delay_index = -1;
    self->all_pass_sample = 0.0;
    self->all_pass_prev_input = 0.0;
    self->sustain_filter_sample = 0.0;
    self->sustain_filter_prev_output2 = 0.0;
    self->sustain_filter_prev_input1 = 0.0;
    self->sustain_filter_prev_input2 = 0.0;
    self->fractional_delay_sample = 0.0;
    self->prev_delay_length = -1.0;
    self->delay_reset_offset = 0;
}

static void pstring_update(pstring_s *self, const picked_inst_s *inst,
                           picked_voice_s *voice, int string_index,
                           double rounded_samples_per_tick,
                           double string_decay_start, double string_decay_end,
                           string_sustain_e sustain_type)
{
    const double samples_per_second = inst->base.sample_rate;
    double all_pass_center =
        PI2 * STRING_DISPERSION_CENTER_FREQ / samples_per_second;

    double prev_delay_length = self->prev_delay_length;

    double phase_delta_start = voice->phase_delta[string_index];
    double phase_delta_scale = voice->phase_delta_scale[string_index];
    double phase_delta_end =
        phase_delta_start * pow(phase_delta_scale, rounded_samples_per_tick);

    double radians_per_sample_start = PI2 * phase_delta_start;
    double radians_per_sample_end = PI2 * phase_delta_end;

    double center_harmonic_start = radians_per_sample_start * 2.0;
    double center_harmonic_end = radians_per_sample_end * 2.0;

    double all_pass_radians_start =
        min(PI, radians_per_sample_start * STRING_DISPERSION_FREQ_MULT
                * pow(all_pass_center / radians_per_sample_start,
                      STRING_DISPERSION_FREQ_SCALE));
    double all_pass_radians_end =
        min(PI, radians_per_sample_end * STRING_DISPERSION_FREQ_MULT
                * pow(all_pass_center / radians_per_sample_end,
                      STRING_DISPERSION_FREQ_SCALE));
    double shelf_radians = PI2 * STRING_SHELF_HZ / samples_per_second;
    double decay_curve_start = (pow(100.0, string_decay_start) - 1.0) / 99.0;
    double decay_curve_end   = (pow(100.0, string_decay_end  ) - 1.0) / 99.0;
    double register_amt = sustain_type = STRING_SUSTAIN_ACOUSTIC ? 0.25 : 0.0;
    double register_shelf_center = 15.6;
    double register_lowpass_center = 3.0 * samples_per_second / 48000.0;
    //const decay_rate_start: number = Math.pow(0.5, decay_curve_start * shelf_radians / radians_per_sample_start);
    //const decay_rate_end: number   = Math.pow(0.5, decay_curve_end   * shelf_radians / radians_per_sample_end);

    // why do i even try column-limiting this crap
    double decay_rate_start = pow(0.5, decay_curve_start * pow(shelf_radians / (radians_per_sample_start * register_shelf_center), (1.0 + 2.0 * register_amt)) * register_shelf_center);
    double decay_rate_end = pow(0.5, decay_curve_end * pow(shelf_radians / (radians_per_sample_end * register_shelf_center), (1.0 + 2.0 * register_amt)) * register_shelf_center);
    
    double expression_decay_start = pow(decay_rate_start, 0.002);
    double expression_decay_end = pow(decay_rate_end, 0.002);

    filter_coefs_s temp_filter_start_coefs;
    filter_coefs_s temp_filter_end_coefs;
    bpbxsyn_complex_s temp_freq_resp;

    bbsyn_filter_ap1ipa(&temp_filter_start_coefs, all_pass_radians_start);
    temp_freq_resp =
        bbsyn_filter_analyze(temp_filter_start_coefs, center_harmonic_start);
    double all_pass_g_start = temp_filter_start_coefs.b[0]; /* same as a[1] */
    double all_pass_phase_delay_start =
        -atan2(temp_freq_resp.imag, temp_freq_resp.real) / center_harmonic_start;

    bbsyn_filter_ap1ipa(&temp_filter_end_coefs, all_pass_radians_end);
    temp_freq_resp
        = bbsyn_filter_analyze(temp_filter_end_coefs, center_harmonic_end);
    double all_pass_g_end = temp_filter_end_coefs.b[0]; /* same as a[1] */
    double all_pass_phase_delay_end =
        -atan2(temp_freq_resp.imag, temp_freq_resp.real) / center_harmonic_end;

    // 1st order shelf filters and 2nd order lowpass filters have differently
    // shaped frequency responses, as well as adjustable shapes. I originally
    // picked a 1st order shelf filter, but I kinda prefer 2nd order lowpass
    // filters now and I designed a couple settings:
    typedef enum pstring_brightness {
        STRING_BRIGHTNESS_BRIGHT,   // 1st order shelf
        STRING_BRIGHTNESS_NORMAL,   // 2nd order lowpass, rounded corner
        STRING_BRIGHTNESS_RESONANT, // 3rd order lowpass, harder corner
    } pstring_brightness_e;

    pstring_brightness_e brightness_type =
        sustain_type == STRING_SUSTAIN_BRIGHT ? STRING_BRIGHTNESS_BRIGHT
                                              : STRING_BRIGHTNESS_NORMAL;

    if (brightness_type == STRING_BRIGHTNESS_BRIGHT) {
        const double shelf_gain_start = pow(decay_rate_start, STRING_DECAY_RATE);
        const double shelf_gain_end   = pow(decay_rate_end,   STRING_DECAY_RATE);

        bbsyn_filter_hshelf2(&temp_filter_start_coefs, shelf_radians,
                             shelf_gain_start, 0.5);
        bbsyn_filter_hshelf2(&temp_filter_end_coefs, shelf_radians,
                             shelf_gain_end, 0.5);
    } else {
        double corner_hardness =
            pow(brightness_type == STRING_BRIGHTNESS_NORMAL ? 0.0 : 1.0, 0.25);
        double lp1_cutoff_radians_start = pow(register_lowpass_center * register_lowpass_center * radians_per_sample_start * 3.3 * 48000 / samples_per_second, 0.5 + register_amt) / register_lowpass_center / pow(decay_curve_start, .5);
        double lp1_cutoff_radians_end = pow(register_lowpass_center * register_lowpass_center * radians_per_sample_end   * 3.3 * 48000 / samples_per_second, 0.5 + register_amt) / register_lowpass_center / pow(decay_curve_end,   .5);
        double lp2_cutoff_radians_start = lp1_cutoff_radians_start * pow(2.0, 0.5 - 1.75 * (1.0 - pow(1.0 - corner_hardness, 0.85)));
        double lp2_cutoff_radians_end = lp1_cutoff_radians_end   * pow(2.0, 0.5 - 1.75 * (1.0 - pow(1.0 - corner_hardness, 0.85)));
        double lp2_gain_start = pow(2.0, -pow(2.0, -pow(corner_hardness, 0.9)));
        double lp2_gain_end = pow(2.0, -pow(2.0, -pow(corner_hardness, 0.9)));

        bbsyn_filter_lp2bw(&temp_filter_start_coefs,
                           bbsyn_warp_inf_to_nyquist(lp2_cutoff_radians_start),
                           lp2_gain_start);
        bbsyn_filter_lp2bw(&temp_filter_end_coefs,
                           bbsyn_warp_inf_to_nyquist(lp2_cutoff_radians_end),
                           lp2_gain_end);
    }
    
    temp_freq_resp = bbsyn_filter_analyze(temp_filter_start_coefs, center_harmonic_start);
    const double sustain_filter_start_a1 = temp_filter_start_coefs.a[1];
    const double sustain_filter_start_a2 = temp_filter_start_coefs.a[2];
    const double sustain_filter_start_b0 = temp_filter_start_coefs.b[0] * expression_decay_start;
    const double sustain_filter_start_b1 = temp_filter_start_coefs.b[1] * expression_decay_start;
    const double sustain_filter_start_b2 = temp_filter_start_coefs.b[2] * expression_decay_start;
    const double sustain_filter_phase_delay_start = -atan2(temp_freq_resp.imag, temp_freq_resp.real) / center_harmonic_start;
    
    temp_freq_resp = bbsyn_filter_analyze(temp_filter_end_coefs, center_harmonic_end);
    const double sustain_filter_end_a1 = temp_filter_end_coefs.a[1];
    const double sustain_filter_end_a2 = temp_filter_end_coefs.a[2];
    const double sustain_filter_end_b0 = temp_filter_end_coefs.b[0] * expression_decay_end;
    const double sustain_filter_b1_end = temp_filter_end_coefs.b[1] * expression_decay_end;
    const double sustain_filter_b2_end = temp_filter_end_coefs.b[2] * expression_decay_end;
    const double sustain_filter_phase_delay_end = -atan2(temp_freq_resp.imag, temp_freq_resp.real) / center_harmonic_end;
    
    const double period_length_start = 1.0 / phase_delta_start;
    const double period_length_end = 1.0 / phase_delta_end;
    const double min_buffer_length = ceil(max(period_length_start, period_length_end) * 2);
    const double delay_length = period_length_start - all_pass_phase_delay_start - sustain_filter_phase_delay_start;
    const double delay_length_end = period_length_end - all_pass_phase_delay_end - sustain_filter_phase_delay_end;
    
    self->prev_delay_length = delay_length;
    self->delay_length_delta = (delay_length_end - delay_length) / rounded_samples_per_tick;
    self->all_pass_g = all_pass_g_start;
    self->sustain_filter_a1 = sustain_filter_start_a1;
    self->sustain_filter_a2 = sustain_filter_start_a2;
    self->sustain_filter_b0 = sustain_filter_start_b0;
    self->sustain_filter_b1 = sustain_filter_start_b1;
    self->sustain_filter_b2 = sustain_filter_start_b2;
    self->all_pass_g_delta = (all_pass_g_end - all_pass_g_start) / rounded_samples_per_tick;
    self->sustain_filter_a1_delta = (sustain_filter_end_a1 - sustain_filter_start_a1) / rounded_samples_per_tick;
    self->sustain_filter_a2_delta = (sustain_filter_end_a2 - sustain_filter_start_a2) / rounded_samples_per_tick;
    self->sustain_filter_b0_delta = (sustain_filter_end_b0 - sustain_filter_start_b0) / rounded_samples_per_tick;
    self->sustain_filter_b1_delta = (sustain_filter_b1_end - sustain_filter_start_b1) / rounded_samples_per_tick;
    self->sustain_filter_b2_delta = (sustain_filter_b2_end - sustain_filter_start_b2) / rounded_samples_per_tick;

    const bool pitch_changed = fabs(log2(delay_length / prev_delay_length)) > 0.01;

    const bool reinit_impulse = (self->delay_index == -1 || pitch_changed);
    assert(inst->delay_line_size > min_buffer_length);
    // TODO: picked string delay line allocation
    // if (this.delay_line == null || this.delay_line.length <= min_buffer_length) {
    //     // The delay line buffer will get reused for other tones so might as well
    //     // start off with a buffer size that is big enough for most notes.
    //     const likelyMaximumLength: number = Math.ceil(2 * synth.samplesPerSecond / Instrument.frequencyFromPitch(12));
    //     const newDelayLine: Float32Array = new Float32Array(Synth.fittingPowerOfTwo(Math.max(likelyMaximumLength, min_buffer_length)));
    //     if (!reinit_impulse && this.delay_line != null) {
    //         // If the tone has already started but the buffer needs to be reallocated,
    //         // transfer the old data to the new buffer.
    //         const oldDelayBufferMask: number = (this.delay_line.length - 1) >> 0;
    //         const startCopyingFromIndex: number = this.delayIndex + this.delay_reset_offset;
    //         this.delayIndex = this.delay_line.length - this.delay_reset_offset;
    //         for (let i: number = 0; i < this.delay_line.length; i++) {
    //             newDelayLine[i] = this.delay_line[(startCopyingFromIndex + i) & oldDelayBufferMask];
    //         }
    //     }
    //     this.delay_line = newDelayLine;
    // }

    float *const delay_line = self->delay_line;
    const int delay_buf_mask = (inst->delay_line_size - 1);
    assert(((delay_buf_mask + 1) & delay_buf_mask) == 0);

    if (!delay_line) return;

    if (reinit_impulse) {
        // -1 delay index means the tone was reset.
        // Also, if the pitch changed suddenly (e.g. from seamless or arpeggio)
        // then reset the wave.

        self->delay_index = 0;
        self->all_pass_sample = 0.0;
        self->all_pass_prev_input = 0.0;
        self->sustain_filter_sample = 0.0;
        self->sustain_filter_prev_output2 = 0.0;
        self->sustain_filter_prev_input1 = 0.0;
        self->sustain_filter_prev_input2 = 0.0;
        self->fractional_delay_sample = 0.0;

        // Clear away a region of the delay buffer for the new impulse.
        const double start_impulse_from = -delay_length;
        const int start_zeros_from =
            (int) floor(start_impulse_from - period_length_start / 2);
        const int stop_zeros_at =
            (int) ceil(start_zeros_from + period_length_start * 2);
        self->delay_reset_offset = stop_zeros_at; // And continue clearing the
                                                  // area in front of the delay
                                                  // line.
        for (int i = start_zeros_from; i <= stop_zeros_at; i++) {
            delay_line[i & delay_buf_mask] = 0.0f;
        }

        const float *const impulse_wave = inst->impulse_wave;
        // The first sample is duplicated at the end, don't double-count it.
        const int impulse_wave_len = IMPULSE_WAVE_LENGTH - 1;
        const double impulse_phase_delta = (double) impulse_wave_len
                                           / period_length_start;

        const double fade_duration = min(period_length_start * 0.2,
                                         samples_per_second * 0.003);
        const double start_impulse_from_sample = ceil(start_impulse_from);
        const double stop_impulse_at =
            start_impulse_from + period_length_start + fade_duration;
        const double stop_impulse_at_sample = stop_impulse_at; // TODO: is this
                                                               // an int?
        double impulse_phase = (start_impulse_from_sample - start_impulse_from)
                               * impulse_phase_delta;
        double prev_wave_integral = 0.0;
        for (double i = start_impulse_from_sample;
             i <= stop_impulse_at_sample; ++i)
        {
            const int impulse_phase_int = (int) impulse_phase;
            const int index = impulse_phase_int % impulse_wave_len;
            double next_wave_integral = (double) impulse_wave[index];
            const double phase_ratio = impulse_phase - impulse_phase_int;
            next_wave_integral += (impulse_wave[index + 1] - next_wave_integral)
                                  * phase_ratio;
            const double sample = (next_wave_integral - prev_wave_integral)
                                  / impulse_phase_delta;
            const double fade_in =
                min(1.0, (i - start_impulse_from) / fade_duration);
            const double fade_out =
                min(1.0, (stop_impulse_at - i) / fade_duration);
            const double combined_fade = fade_in * fade_out;
            const double curved_fade =
                combined_fade * combined_fade * (3.0 - 2.0 * combined_fade);
                //                              A cubic sigmoid from 0 to 1.
            delay_line[(int)i & delay_buf_mask] += (float)(sample * curved_fade);
            prev_wave_integral = next_wave_integral;
            impulse_phase += impulse_phase_delta;
        }
    }
}

static void picked_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst) {
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    *inst = (picked_inst_s){0};
    bbsyn_inst_init(ctx, &inst->base, BPBXSYN_SYNTH_PICKED_STRING);

    inst->sustain[0] = BPBXSYN_PICKED_STRING_SUSTAIN_MAX;
    inst->sustain[1] = inst->sustain[0];

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i)
    {
        for (int j = 0; j < UNISON_MAX_VOICES; ++j)
        {
            pstring_reset(&inst->voices[i].strings[j]);
        }
    }

    bbsyn_generate_harmonics(&inst->base.ctx->wavetables, inst->harmonics,
                             64, inst->impulse_wave);
    memcpy(inst->last_harmonics, inst->harmonics, sizeof(inst->harmonics));
}

static void picked_destroy(bpbxsyn_synth_s *p_inst)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bpbxsyn_free(inst->base.ctx, inst->delay_line_alloc);
}

static bpbxsyn_voice_id picked_note_on(bpbxsyn_synth_s *p_inst, int key,
                                    double velocity, int32_t length) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id id =
        bbsyn_trigger_voice(p_inst, GENERIC_LIST(inst->voices), key, velocity,
                            length, &continuation);

    if (!continuation) {
        picked_voice_s *voice = &inst->voices[id];

        // i want to preserve picked string properties, and call reset, instead
        // of simply setting them to zero...
        pstring_s temp_strings[UNISON_MAX_VOICES];
        assert(sizeof(voice->strings) == sizeof(temp_strings));

        memcpy(temp_strings, voice->strings, sizeof(voice->strings));

        *voice = (picked_voice_s) {
            .base = voice->base,
        };

        memcpy(voice->strings, temp_strings, sizeof(voice->strings));
        for (int i = 0; i < UNISON_MAX_VOICES; ++i)
            pstring_reset(&voice->strings[i]);
        
        voice->at_note_start = true;
    }

    return id;
}

static void picked_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

static void picked_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void picked_sample_rate_changed(bpbxsyn_synth_s *p_inst, double old,
                                       double new)
{
    if (new == old) return;

    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    const bpbxsyn_context_s *const ctx = inst->base.ctx;

    if (inst->delay_line_alloc)
        bpbxsyn_free(ctx, inst->delay_line_alloc);

    // The delay line buffer will get reused for other tones so might as well
    // start off with a buffer size that is big enough for most notes.
    const int likely_maximum_length = (int) ceil(4 * new / key_to_hz_d(12.0));
    int dl_size = bbsyn_fitting_power_of_two(likely_maximum_length);

    float *dl_alloc =
        bpbxsyn_malloc(ctx, DELAY_LINE_COUNT * dl_size * sizeof(float));
    if (!dl_alloc)
    {
        bbsyn_logmsgf(ctx, BPBXSYN_LOG_ERROR,
                      "picked string could not allocate delay lines!");
        
        // umm i don't know how the program would recover from this. uhhh.
        inst->delay_line_alloc = NULL;
        inst->delay_line_size = 0;
        return;
    }

    bbsyn_logmsgf(ctx, BPBXSYN_LOG_DEBUG,
                  "allocated %llu bytes of delay line buffers",
                  (size_t)dl_size * DELAY_LINE_COUNT * sizeof(float));
    
    memset(dl_alloc, 0, DELAY_LINE_COUNT * dl_size * sizeof(float));

    inst->delay_line_size = dl_size;
    inst->delay_line_alloc = dl_alloc;
    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i)
    {
        for (int j = 0; j < UNISON_MAX_VOICES; ++j)
        {
            inst->voices[i].strings[j].delay_line = dl_alloc;
            dl_alloc += dl_size;
        }
    }
}

static void compute_voice(
    const bpbxsyn_synth_s *const base_inst, inst_base_voice_s *base_voice,
    voice_compute_s *compute_data
) {
    const picked_inst_s *const inst = (picked_inst_s*) base_inst;
    picked_voice_s *const voice = (picked_voice_s*) base_voice;

    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick =
        compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const unison_desc_s unison = bbsyn_unison_info[inst->unison_type];

    double settings_expression_mult = PICKED_BASE_EXPRESSION;
    settings_expression_mult *= unison.expression * unison.voices / 2.0;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start;
    const double end_pitch = voice->base.current_key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (voice->has_prev_pitch_expression) {
        pitch_expression_start = voice->prev_pitch_expression;
    } else {
        pitch_expression_start = calc_pitch_expression(start_pitch);
    }
    const double pitch_expression_end = calc_pitch_expression(end_pitch);
    voice->has_prev_pitch_expression = TRUE;
    voice->prev_pitch_expression = pitch_expression_end;

    double use_sustain_start = inst->sustain[0];
    double use_sustain_end = inst->sustain[1];
    voice->sustain_start = use_sustain_start;
    voice->sustain_end = use_sustain_end;

    // Increase expression to compensate for string decay.
    settings_expression_mult *=
        pow(2.0, 0.7 *
                 (1.0 - use_sustain_start / BPBXSYN_PICKED_STRING_SUSTAIN_MAX));

    // calculate final expression
    const double expr_start = varying->expr_start * settings_expression_mult
                                                  * pitch_expression_start;
    const double expr_end   = varying->expr_end * settings_expression_mult
                                                * pitch_expression_end;
    
    const double unison_env_start =
        voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_UNISON];
    const double unison_env_end =
        voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_UNISON];

    const double freq_end_ratio =
        pow(2.0, (interval_end - interval_start) * 1.0 / 12.0);
    const double base_phase_delta_scale =
        pow(freq_end_ratio, 1.0 / rounded_samples_per_tick);

    const double start_freq = key_to_hz_d(start_pitch);

    // TODO: specialIntervalMult has to do with arpeggios/custom interval
    // But if there is none it will always be 1.0
    assert(UNISON_MAX_VOICES == 2);
    double unison_starts[UNISON_MAX_VOICES];
    double unison_ends[UNISON_MAX_VOICES];

    unison_starts[0] =
        pow(2.0, (unison.offset + unison.spread) * unison_env_start / 12.0);
    unison_ends[0] =
        pow(2.0, (unison.offset + unison.spread) * unison_env_end / 12.0);
    unison_starts[1] =
        pow(2.0, (unison.offset - unison.spread) * unison_env_start / 12.0)/* * specialIntervalMult*/;
    unison_ends[1] =
        pow(2.0, (unison.offset - unison.spread) * unison_env_end / 12.0)/* * specialIntervalMult*/;

    for (int i = 0; i < UNISON_MAX_VOICES; i++) {
        voice->phase_delta[i] = start_freq * sample_len * unison_starts[i];
        voice->phase_delta_scale[i] =
            base_phase_delta_scale * pow(unison_ends[i] / unison_starts[i],
                                         1.0 / rounded_samples_per_tick);
    }
    
    voice->base.expression = expr_start;
    voice->base.expression_delta =
        (expr_end - expr_start) / rounded_samples_per_tick;

    double string_decay_start;
    if (voice->has_prev_string_decay) {
        string_decay_start = voice->prev_string_decay;
    } else {
        double sustain_envelope_start =
            voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_STRING_SUSTAIN];
        string_decay_start =
            1.0- min(1.0, sustain_envelope_start * voice->sustain_start 
                          / BPBXSYN_PICKED_STRING_SUSTAIN_MAX);
    }
    double sustain_envelope_end =
        voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_STRING_SUSTAIN];
    double string_decay_end =
        1.0 - min(1.0, sustain_envelope_end * voice->sustain_end / BPBXSYN_PICKED_STRING_SUSTAIN_MAX);
    voice->prev_string_decay = string_decay_end;

    // for (int i: number = tone.pickedStrings.length; i < unison.voices; i++) {
    //     tone.pickedStrings[i] = new PickedString();
    // }

    // TODO: don't reinit picked string pulse if continue transition?
    if (voice->at_note_start/* && !transition.continues && !tone.forceContinueAtStart*/) {
        for (int i = 0; i < UNISON_MAX_VOICES; ++i)
            voice->strings[i].delay_index = -1;
    }

    for (int i = 0; i < unison.voices; ++i) {
        pstring_update(&voice->strings[i], inst, voice, i,
                       rounded_samples_per_tick, string_decay_start,
                       string_decay_end, STRING_SUSTAIN_BRIGHT);
    }

    voice->at_note_start = false;
}

static void picked_tick(bpbxsyn_synth_s *p_inst,
                     const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });

    inst->sustain[0] = inst->sustain[1];
}

static void picked_run(bpbxsyn_synth_s *p_inst, float *samples,
                       size_t frame_count)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    (void)inst;

    memset(samples, 0, frame_count * sizeof(float));

    // if harmonic controls changed, rebuild the wave
    if (memcmp(inst->harmonics, inst->last_harmonics,
               sizeof(inst->harmonics)))
    {
        memcpy(inst->last_harmonics, inst->harmonics, sizeof(inst->harmonics));
        bbsyn_generate_harmonics(&inst->base.ctx->wavetables,
                                 inst->harmonics, 64, inst->impulse_wave);
    }

    const int delay_line_length = inst->delay_line_size;

    // This algorithm is similar to the Karpluss-Strong algorithm in principle,
    // but with an all-pass filter for dispersion and with more control over the
    // impulse harmonics.
    for (int vi = 0; vi < BPBXSYN_SYNTH_MAX_VOICES; ++vi) {
        picked_voice_s *voice = &inst->voices[vi];
        if (!voice_is_computing(&voice->base)) continue;

        double expression = voice->base.expression;
		const double expression_delta = voice->base.expression_delta;
		
        // const unison_sign = tone.specialIntervalExpressionMult
        //                     * instrumentState.unison.sign;
        const double unison_sign = 1.0;

        // const dyn_biquad_s *filters = voice->base.note_filters
        // const filterCount = NOTE_FILTER

        double init_filter_input1 = voice->base.note_filter_input[0];
        double init_filter_input2 = voice->base.note_filter_input[1];

        typedef struct voice_data {
            double all_pass_sample;
            double all_pass_prev_input;
            double sustain_filter_sample;
            double sustain_filter_prev_output2;
            double sustain_filter_prev_input1;
            double sustain_filter_prev_input2;
            double fractional_delay_sample;
            float *delay_line;
            int delay_buf_mask;
            int delayIndex;
            double delay_length;
            double delay_length_delta;
            double all_pass_g;
            double sustain_filter_a1;
            double sustain_filter_a2;
            double sustain_filter_b0;
            double sustain_filter_b1;
            double sustain_filter_b2;
            double all_pass_g_delta;
            double sustain_filter_a1_delta;
            double sustain_filter_a2_delta;
            double sustain_filter_b0_delta;
            double sustain_filter_b1_delta;
            double sustain_filter_b2_delta;
            int delay_reset_offset;
        } voice_data_s;
        voice_data_s voice_data[UNISON_MAX_VOICES];

        for (int ui = 0; ui < UNISON_MAX_VOICES; ++ui) {
            pstring_s *pstr = &voice->strings[ui];
            voice_data_s *vd = voice_data + ui;

            vd->all_pass_sample = pstr->all_pass_sample;
            vd->all_pass_prev_input = pstr->all_pass_prev_input;
            vd->sustain_filter_sample = pstr->sustain_filter_sample;
            vd->sustain_filter_prev_output2 = pstr->sustain_filter_prev_output2;
            vd->sustain_filter_prev_input1 = pstr->sustain_filter_prev_input1;
            vd->sustain_filter_prev_input2 = pstr->sustain_filter_prev_input2;
            vd->fractional_delay_sample = pstr->fractional_delay_sample;
            vd->delay_line = pstr->delay_line;
            vd->delay_buf_mask = delay_line_length - 1;
            vd->delayIndex = pstr->delay_index;
            vd->delayIndex = (vd->delayIndex & vd->delay_buf_mask)
                             + delay_line_length;
            vd->delay_length = pstr->prev_delay_length;
            vd->delay_length_delta = pstr->delay_length_delta;
            vd->all_pass_g = pstr->all_pass_g;
            vd->sustain_filter_a1 = pstr->sustain_filter_a1;
            vd->sustain_filter_a2 = pstr->sustain_filter_a2;
            vd->sustain_filter_b0 = pstr->sustain_filter_b0;
            vd->sustain_filter_b1 = pstr->sustain_filter_b1;
            vd->sustain_filter_b2 = pstr->sustain_filter_b2;
            vd->all_pass_g_delta = pstr->all_pass_g_delta;
            vd->sustain_filter_a1_delta = pstr->sustain_filter_a1_delta;
            vd->sustain_filter_a2_delta = pstr->sustain_filter_a2_delta;
            vd->sustain_filter_b0_delta = pstr->sustain_filter_a0_delta;
            vd->sustain_filter_b1_delta = pstr->sustain_filter_a1_delta;
            vd->sustain_filter_b2_delta = pstr->sustain_filter_a2_delta;

            vd->delay_reset_offset = pstr->delay_reset_offset;

            if (!vd->delay_line) return;
        }

        for (size_t frame = 0; frame < frame_count; ++frame) {
            for (int ui = 0; ui < UNISON_MAX_VOICES; ++ui) {
                voice_data_s *vd = voice_data + ui;

                const double target_sample_time = vd->delayIndex - vd->delay_length;
                // Offset to improve stability of all-pass filter.
                const int lower_index = (int)(target_sample_time + 0.125);
                const int upper_index = lower_index + 1;
                const double fractional_delay = upper_index - target_sample_time;
                // Inlined version of
                // FilterCoefficients.prototype.allPass1stOrderFractionalDelay
                const double fractional_delay_g =
                    (1.0 - fractional_delay) / (1.0 + fractional_delay);
                const double prev_input =
                    vd->delay_line[lower_index & vd->delay_buf_mask];
                const double input =
                    vd->delay_line[upper_index & vd->delay_buf_mask];
                
                vd->fractional_delay_sample =
                    fractional_delay_g * input + prev_input
                    - fractional_delay_g * vd->fractional_delay_sample;
                
                vd->all_pass_sample =
                    vd->fractional_delay_sample * vd->all_pass_g
                    + vd->all_pass_prev_input
                    - vd->all_pass_g * vd->all_pass_sample;
                vd->all_pass_prev_input = vd->fractional_delay_sample;
                
                const double sustain_filter_prev_output1 = vd->sustain_filter_sample;
                vd->sustain_filter_sample =
                    vd->sustain_filter_b0 * vd->all_pass_sample
                    + vd->sustain_filter_b1 * vd->sustain_filter_prev_input1
                    + vd->sustain_filter_b2 * vd->sustain_filter_prev_input2
                    - vd->sustain_filter_a1 * vd->sustain_filter_sample
                    - vd->sustain_filter_a2 * vd->sustain_filter_prev_output2;
                
                vd->sustain_filter_prev_output2 = sustain_filter_prev_output1;
                vd->sustain_filter_prev_input2 = vd->sustain_filter_prev_input1;
                vd->sustain_filter_prev_input1 = vd->all_pass_sample;
                
                vd->delay_line[vd->delayIndex & vd->delay_buf_mask] +=
                    (float) vd->sustain_filter_sample;
                
                const int delay_line_reset_idx = 
                    (vd->delayIndex + vd->delay_reset_offset)
                    & vd->delay_buf_mask;
                vd->delay_line[delay_line_reset_idx] = 0.0f;

                ++vd->delayIndex;
            }

            const double input_sample =
                (voice_data[0].fractional_delay_sample
                 + voice_data[1].fractional_delay_sample * unison_sign)
                * expression;
            const double sample =
                bbsyn_apply_filters(input_sample, init_filter_input1,
                                    init_filter_input2,
                                    voice->base.note_filters);
            init_filter_input2 = init_filter_input1;
            init_filter_input1 = input_sample;
            samples[frame] += (float) sample;

            expression += expression_delta;
            
            for (int ui = 0; ui < UNISON_MAX_VOICES; ++ui) {
                voice_data_s *vd = voice_data + ui;

                vd->delay_length += vd->delay_length_delta;
                vd->all_pass_g += vd->all_pass_g_delta;
                vd->sustain_filter_a1 += vd->sustain_filter_a1_delta;
                vd->sustain_filter_a2 += vd->sustain_filter_a2_delta;
                vd->sustain_filter_b0 += vd->sustain_filter_b0_delta;
                vd->sustain_filter_b1 += vd->sustain_filter_b1_delta;
                vd->sustain_filter_b2 += vd->sustain_filter_b2_delta;
            }
        }

        for (int ui = 0; ui < UNISON_MAX_VOICES; ++ui) {
            voice_data_s *vd = voice_data + ui;
            pstring_s *pstr = &voice->strings[ui];
            
            // Avoid persistent denormal or NaN values in the delay buffers and
            // filter history.
            const double epsilon = 1.0e-24; // okay what why is this difference
                                            // than the other epsilons?
            
            #define SANITIZE(n)\
                if (!isfinite(n) || fabs(n) < epsilon) n = 0.0;
            
            SANITIZE(vd->all_pass_sample);
            SANITIZE(vd->all_pass_prev_input);
            SANITIZE(vd->sustain_filter_sample);
            SANITIZE(vd->sustain_filter_prev_output2);
            SANITIZE(vd->sustain_filter_prev_input1);
            SANITIZE(vd->sustain_filter_prev_input2);
            SANITIZE(vd->fractional_delay_sample);
            
            #undef SANITIZE

            pstr->all_pass_sample = vd->all_pass_sample;
            pstr->all_pass_prev_input = vd->all_pass_prev_input;
            pstr->sustain_filter_sample = vd->sustain_filter_sample;
            pstr->sustain_filter_prev_output2 = vd->sustain_filter_prev_output2;
            pstr->sustain_filter_prev_input1 = vd->sustain_filter_prev_input1;
            pstr->sustain_filter_prev_input2 = vd->sustain_filter_prev_input2;
            pstr->fractional_delay_sample = vd->fractional_delay_sample;
            pstr->delay_index = vd->delayIndex;
            pstr->prev_delay_length = vd->delay_length;
            pstr->all_pass_g = vd->all_pass_g;
            pstr->sustain_filter_a1 = vd->sustain_filter_a1;
            pstr->sustain_filter_a2 = vd->sustain_filter_a2;
            pstr->sustain_filter_b0 = vd->sustain_filter_b0;
            pstr->sustain_filter_b1 = vd->sustain_filter_b1;
            pstr->sustain_filter_b2 = vd->sustain_filter_b2;
        }

        voice->base.expression = expression;
        bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = init_filter_input1;
        voice->base.note_filter_input[1] = init_filter_input2;
    }
}






//////////
// DATA //
//////////

static const bpbxsyn_param_info_s picked_param_info[] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psUnison",
        .name = "Unison",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_UNISON_COUNT - 1,
        .default_value = BPBXSYN_UNISON_NONE,

        .enum_values = bbsyn_unison_enum_values
    },

    {
        .type = BPBXSYN_PARAM_DOUBLE,

        .id = "psSustai",
        .name = "Sustain",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_PICKED_STRING_SUSTAIN_MAX,
        .default_value = 10.0,
    },
    
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm01",
        .name = "Harmonics 1x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm02",
        .name = "Harmonics 2x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm03",
        .name = "Harmonics 3x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm04",
        .name = "Harmonics 4x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm05",
        .name = "Harmonics 5x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm06",
        .name = "Harmonics 6x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm07",
        .name = "Harmonics 7x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm08",
        .name = "Harmonics 8x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm09",
        .name = "Harmonics 9x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm10",
        .name = "Harmonics 10x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm11",
        .name = "Harmonics 11x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm12",
        .name = "Harmonics 12x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm13",
        .name = "Harmonics 13x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm14",
        .name = "Harmonics 14x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm15",
        .name = "Harmonics 15x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm16",
        .name = "Harmonics 16x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm17",
        .name = "Harmonics 17x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm18",
        .name = "Harmonics 18x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm19",
        .name = "Harmonics 19x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm20",
        .name = "Harmonics 20x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm21",
        .name = "Harmonics 21x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm22",
        .name = "Harmonics 22x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm23",
        .name = "Harmonics 23x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm24",
        .name = "Harmonics 24x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm25",
        .name = "Harmonics 25x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm26",
        .name = "Harmonics 26x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm27",
        .name = "Harmonics 27x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm28",
        .name = "Harmonics 28x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
};

static const size_t picked_param_addresses[] = {
    offsetof(picked_inst_s, unison_type),
    offsetof(picked_inst_s, sustain[1]),
    offsetof(picked_inst_s, harmonics[0]),
    offsetof(picked_inst_s, harmonics[1]),
    offsetof(picked_inst_s, harmonics[2]),
    offsetof(picked_inst_s, harmonics[3]),
    offsetof(picked_inst_s, harmonics[4]),
    offsetof(picked_inst_s, harmonics[5]),
    offsetof(picked_inst_s, harmonics[6]),
    offsetof(picked_inst_s, harmonics[7]),
    offsetof(picked_inst_s, harmonics[8]),
    offsetof(picked_inst_s, harmonics[9]),
    offsetof(picked_inst_s, harmonics[10]),
    offsetof(picked_inst_s, harmonics[11]),
    offsetof(picked_inst_s, harmonics[12]),
    offsetof(picked_inst_s, harmonics[13]),
    offsetof(picked_inst_s, harmonics[14]),
    offsetof(picked_inst_s, harmonics[15]),
    offsetof(picked_inst_s, harmonics[16]),
    offsetof(picked_inst_s, harmonics[17]),
    offsetof(picked_inst_s, harmonics[18]),
    offsetof(picked_inst_s, harmonics[19]),
    offsetof(picked_inst_s, harmonics[20]),
    offsetof(picked_inst_s, harmonics[21]),
    offsetof(picked_inst_s, harmonics[22]),
    offsetof(picked_inst_s, harmonics[23]),
    offsetof(picked_inst_s, harmonics[24]),
    offsetof(picked_inst_s, harmonics[25]),
    offsetof(picked_inst_s, harmonics[26]),
    offsetof(picked_inst_s, harmonics[27]),
};

static const bpbxsyn_envelope_compute_index_e picked_env_targets[] = {
    BPBXSYN_ENV_INDEX_UNISON,
    BPBXSYN_ENV_INDEX_STRING_SUSTAIN,
};

const inst_vtable_s bbsyn_inst_picked_vtable = {
    .struct_size = sizeof(picked_inst_s),

    .param_count = BPBXSYN_PICKED_STRING_PARAM_COUNT,
    .param_info = picked_param_info,
    .param_addresses = picked_param_addresses,

    .envelope_target_count = 2,
    .envelope_targets = picked_env_targets,

    .inst_init = picked_init,
    .inst_destroy = picked_destroy,
    .inst_note_on = picked_note_on,
    .inst_note_off = picked_note_off,
    .inst_note_all_off = picked_note_all_off,
    .inst_sample_rate_changed = picked_sample_rate_changed,

    .inst_tick = picked_tick,
    .inst_run = picked_run
};