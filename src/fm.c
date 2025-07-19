#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "fm.h"
#include "fm_algo.h"
#include "util.h"
#include "wavetables.h"
#include "instrument.h"
#include "envelope.h"
#include "filtering.h"

/*
algorithms:
0.  1 <- (2 3 4)
1.  1 <- (2 3 <- 4)
2.  1 <- 2 <- (3 4)
3.  1 <- (2 3) <- 4
4.  1 <- 2 <- 3 <- 4
5.  1 <- 3  2 <- 4
6.  1  2 <- (3 4)
7.  1  2 <- 3 <- 4
8.  (1 2) <- 3 <- 4
9.  (1 2) <- (3 4)
10. 1  2  3 <- 4
11. (1 2 3) <- 4
12. 1  2  3  4


feedback types:
0.  1 G
1.  2 G
2.  3 G
3.  4 G
4.  1 G  2 G
5.  3 G  4 G
6.  1 G  2 G  3 G
7.  2 G  3 G  4 G
8.  1 G  2 G  3 G  4 G
9.  1 -> 2
10. 1 -> 3
11. 1 -> 4
12. 2 -> 3
13. 2 -> 4
14. 3 -> 4
15. 1 -> 3  2 -> 4
16. 1 -> 4  2 -> 3
17. 1 -> 2 -> 3 -> 4
*/

typedef struct {
    double mult;
    double hz_offset;
    double amplitude_sign;
} fm_freq_data_s;

static fm_freq_data_s frequency_data[BPBX_FM_FREQ_COUNT];
static int algo_associated_carriers[BPBX_FM_ALGORITHM_COUNT][4];
static double carrier_intervals[FM_OP_COUNT];

#define EXPRESSION_REFERENCE_PITCH 16 // A low "E" as a MIDI pitch.
#define PITCH_DAMPING 48
#define VOICE_BASE_EXPRESSION 0.1 // 0.03 (original value, but i felt like it was too quiet)

static double operator_amplitude_curve(double amplitude) {
    return (pow(16.0, amplitude / 15.0) - 1.0) / 15.0;
}

static void setup_algorithm(fm_inst_s *inst) {
    switch (inst->algorithm) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            inst->carrier_count = 1;
            break;

        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            inst->carrier_count = 2;
            break;

        case 10:
        case 11:
            inst->carrier_count = 3;
            break;

        case 12:
            inst->carrier_count = 4;
            break;

        default:
            inst->carrier_count = 0;
    }
}

void fm_init(fm_inst_s *inst) {    
    memset(inst, 0, sizeof(*inst));

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst->voices[i].active = FALSE;
        inst->voices[i].triggered = FALSE;
    }

    inst->algorithm = 0;

    inst->amplitudes[0] = 15.0;
    inst->amplitudes[1] = 0.0;
    inst->amplitudes[2] = 0.0;
    inst->amplitudes[3] = 0.0;

    inst->freq_ratios[0] = 4;
    inst->freq_ratios[1] = 4;
    inst->freq_ratios[2] = 4;
    inst->freq_ratios[3] = 4;
    
    inst->feedback = 0;
}

int fm_midi_on(bpbx_inst_s *inst, int key, int velocity) {
    fm_inst_s *const fm = inst->fm;

    float velocity_f = velocity / 127.f;

    int voice_index = 0;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        if (fm->voices[i].triggered) continue;
        voice_index = i;
        break;
    }

    fm_voice_s *voice = fm->voices + voice_index;
    *voice = (fm_voice_s) {
        .triggered = TRUE,
        .released = FALSE,
        .key = key < 0 ? 0 : (uint16_t)key,
        .volume = velocity_f,
        .has_prev_vibrato = FALSE
    };

    for (int op = 0; op < FM_OP_COUNT; op++) {
        voice->op_states[op] = (fm_voice_opstate_s) {
            .phase = 0.0,
            .phase_delta = 0.0,
            .expression = 0.0,
            .output = 0.0,
            .prev_pitch_expression = 0.0,
            .has_prev_pitch_expression = FALSE
        };
    }

    for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
        dyn_biquad_reset_output(voice->note_filters + i);
    }

    envelope_computer_init(&voice->env_computer, inst->mod_x, inst->mod_y, inst->mod_wheel);

    return voice_index;
}

void fm_midi_off(bpbx_inst_s *inst, int key, int velocity) {
    fm_inst_s *const fm = inst->fm;
    
    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        fm_voice_s *voice = &fm->voices[i];
        if (voice->triggered && !voice->released && voice->key == key) {
            voice->released = 1;
            break;
        }
    }
}

typedef struct {
    const bpbx_inst_s *base_inst;

    double fade_in;
    double fade_out;
    double samples_per_tick;
    double sample_rate;
    double cur_beat;
    double mod_x, mod_y, mod_w;
    bpbx_vibrato_params_s *vibrato_params;

    uint8_t envelope_count;
    bpbx_envelope_s *envelopes;
} tone_compute_s;

static void compute_voice(const fm_inst_s *const inst, fm_voice_s *const voice, tone_compute_s compute_data) {
    const double sample_len = 1.f / compute_data.sample_rate;
    const double samples_per_tick = compute_data.samples_per_tick;
    const double rounded_samples_per_tick = ceil(samples_per_tick);

    voice->time_ticks = voice->time2_ticks;
    voice->time2_ticks = voice->time_ticks + 1.0;
    voice->time_secs = voice->time2_secs;
    voice->time2_secs = voice->time_secs + samples_per_tick / compute_data.sample_rate;

    // const double ticks_into_bar = compute_data.cur_beat * PARTS_PER_BEAT / TICKS_PER_PART;
    // const double part_time_start = (double)ticks_into_bar / TICKS_PER_PART;
    // const double part_time_end = (double)(ticks_into_bar + 1) / TICKS_PER_PART;

    // update envelope computer modulation
    update_envelope_modulation(&voice->env_computer, compute_data.mod_x, compute_data.mod_y, compute_data.mod_w);

    compute_envelopes(
        &voice->env_computer,
        compute_data.envelopes, compute_data.envelope_count,
        compute_data.cur_beat, voice->time_secs, samples_per_tick * sample_len
    );

    const double fade_in_secs = secs_fade_in(compute_data.fade_in);

    double interval_start = 0.0;
    double interval_end = 0.0;

    // precalculation/volume balancing/etc
    double sine_expr_boost = 1.0;
    double total_carrier_expr = 0.0;
    double fade_expr_start = 1.0;
    double fade_expr_end = 1.0;

    const uint8_t released = voice->time_secs >= fade_in_secs && voice->released;
    if (released) {
        const double ticks = fabs(ticks_fade_out(compute_data.fade_out));
        fade_expr_start = note_size_to_volume_mult((1.0 - voice->ticks_since_release / ticks) * NOTE_SIZE_MAX);
        fade_expr_end = note_size_to_volume_mult((1.0 - (voice->ticks_since_release + 1.0) / ticks) * NOTE_SIZE_MAX);

        if (voice->ticks_since_release >= ticks) {
            voice->is_on_last_tick = TRUE;
        }
    } else {
        // fade in beginning of note
        if (fade_in_secs > 0) {
            fade_expr_start *= min(1.0, voice->time_secs / fade_in_secs);
            fade_expr_end *= min(1.0, voice->time2_secs / fade_in_secs);
        }
    }

    // pitch shift
    if (compute_data.base_inst->active_effects[BPBX_INSTFX_PITCH_SHIFT]) {
        const double env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_PITCH_SHIFT];
        const double env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_PITCH_SHIFT];

        interval_start += compute_data.base_inst->pitch_shift * env_start;
        interval_end += compute_data.base_inst->pitch_shift * env_end;
    }

    // detune
    if (compute_data.base_inst->active_effects[BPBX_INSTFX_DETUNE]) {
        const double env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_DETUNE];
        const double env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_DETUNE];

        interval_start += compute_data.base_inst->detune * env_start / 100.0;
        interval_end += compute_data.base_inst->detune * env_end / 100.0;
    }

    // vibrato
    if (compute_data.base_inst->active_effects[BPBX_INSTFX_VIBRATO]) {
        const bpbx_vibrato_params_s vibrato_params = *compute_data.vibrato_params;

        int delay_ticks = vibrato_params.delay * 2;

        // i don't get the calculations beepbox/jummbox does.
        // i just did my own thing. It sounds the same i think.
        double vibrato_phase_tick = compute_data.cur_beat * PARTS_PER_BEAT * TICKS_PER_PART;
        vibrato_phase_tick *= vibrato_params.speed;

        const double vibrato_time_start = compute_data.base_inst->vibrato_time_start;
        const double vibrato_time_end = compute_data.base_inst->vibrato_time_end;

        double vibrato_start;
        if (voice->has_prev_vibrato) {
            vibrato_start = voice->prev_vibrato;
        } else {
            double lfo_start = get_lfo_amplitude(vibrato_params.type, vibrato_time_start);
            const double vibrato_depth_envelope_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_VIBRATO_DEPTH];
            vibrato_start = vibrato_params.depth * lfo_start * vibrato_depth_envelope_start;

            if (delay_ticks > 0.0) {
                const int ticks_until_vibrato_start = delay_ticks - voice->time_ticks;
                vibrato_start *= max(0.0, min(1.0, 1.0 - ticks_until_vibrato_start / 2.0));
            }
        }

        double lfo_end = get_lfo_amplitude(vibrato_params.type, vibrato_time_end);
        const double vibrato_depth_envelope_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_VIBRATO_DEPTH];
        double vibrato_end = vibrato_params.depth * lfo_end * vibrato_depth_envelope_end;
        if (delay_ticks > 0.0) {
            const int ticks_until_vibrato_end = delay_ticks - voice->time2_ticks;
            vibrato_end *= max(0.0, min(1.0, 1.0 - ticks_until_vibrato_end / 2.0));
        }

        voice->has_prev_vibrato = TRUE;
        voice->prev_vibrato = vibrato_end;

        interval_start += vibrato_start;
        interval_end += vibrato_end;
    }

    // note filter
    double note_filter_expression = voice->env_computer.lp_cutoff_decay_volume_compensation;
    voice->filters_enabled = compute_data.base_inst->active_effects[BPBX_INSTFX_NOTE_FILTER];
    if (voice->filters_enabled) {
        // get modulation for all freqs
        const double note_all_freqs_envelope_start =
            voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_ALL_FREQS];
        const double note_all_freqs_envelope_end=
            voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_ALL_FREQS];
        
        for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
            const filter_group_s *filter_group_start = &compute_data.base_inst->last_note_filter;
            const filter_group_s *filter_group_end = &compute_data.base_inst->note_filter;

            // If switching dot type, do it all at once and do not try to interpolate since no valid interpolation exists.
            if (filter_group_start->type[i] != filter_group_end->type[i]) {
                filter_group_start = filter_group_end;
            }

            if (filter_group_start->type[i] == BPBX_FILTER_TYPE_OFF) {
                voice->note_filters[i].enabled = FALSE;
            } else {
                // get freq modulation
                const double note_freq_envelope_start =
                    voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_FREQ0 + i];
                const double note_freq_envelope_end =
                    voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_FREQ0 + i];

                // get gain modulation
                const double note_peak_envelope_start =
                    voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_FILTER_GAIN0 + i];
                const double note_peak_envelope_end =
                    voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_FILTER_GAIN0 + i];
                
                voice->note_filters[i].enabled = TRUE;

                filter_coefs_s start_coefs = filter_to_coefficients(
                    filter_group_start, i,
                    compute_data.sample_rate,
                    note_all_freqs_envelope_start * note_freq_envelope_start,
                    note_peak_envelope_start);

                filter_coefs_s end_coefs = filter_to_coefficients(
                    filter_group_end, i,
                    compute_data.sample_rate,
                    note_all_freqs_envelope_end * note_freq_envelope_end,
                    note_peak_envelope_end);
                
                dyn_biquad_load(&voice->note_filters[i],
                    start_coefs, end_coefs, 1.0 / rounded_samples_per_tick,
                    filter_group_start->type[i] == BPBX_FILTER_TYPE_LP);

                note_filter_expression *= filter_get_volume_compensation_mult(filter_group_start, i);
            }
        }
    }

    if (note_filter_expression > 3.0)
        note_filter_expression = 3.0;

    for (int op = 0; op < FM_OP_COUNT; op++) {
        // john nesky: I'm adding 1000 to the phase to ensure that it's never negative even when modulated
        // by other waves because negative numbers don't work with the modulus operator very well.
        voice->op_states[op].phase = (fmod(voice->op_states[op].phase, 1.0) + 1000);

        fm_freq_data_s *freq_data = &frequency_data[inst->freq_ratios[op]];

        int associated_carrier_idx = algo_associated_carriers[inst->algorithm][op] - 1;
        const double freq_mult = freq_data->mult;
        const double pitch_start = (double)voice->key + interval_start + carrier_intervals[associated_carrier_idx];
        const double pitch_end = (double)voice->key + interval_end + carrier_intervals[associated_carrier_idx];
        const double base_freq_start = key_to_hz_d(pitch_start);
        const double base_freq_end = key_to_hz_d(pitch_end);
        const double hz_offset = freq_data->hz_offset;
        const double target_freq_start = freq_mult * base_freq_start + hz_offset;
        const double target_freq_end = freq_mult * base_freq_end + hz_offset;

        const double freq_env_start = voice->env_computer.envelope_starts[BPBX_ENV_INDEX_OPERATOR_FREQ0 + op];
        const double freq_env_end = voice->env_computer.envelope_ends[BPBX_ENV_INDEX_OPERATOR_FREQ0 + op];
        double freq_start, freq_end;
        if (freq_env_start != 1.0 || freq_env_end != 1.0) {
            freq_start = pow(2.0, log2(target_freq_start / base_freq_start) * freq_env_start) * base_freq_start;
            freq_end = pow(2.0, log2(target_freq_end / base_freq_end) * freq_env_end) * base_freq_end;
        } else {
            freq_start = target_freq_start;
            freq_end = target_freq_end;
        }

        voice->op_states[op].phase_delta = freq_start * sample_len;
        voice->op_states[op].phase_delta_scale = pow(freq_end / freq_start, 1.0 / rounded_samples_per_tick);

        const double amplitude_curve = operator_amplitude_curve((double) inst->amplitudes[op]);
        const double amplitude_mult = amplitude_curve * freq_data->amplitude_sign;
        double expression_start = amplitude_mult;
        double expression_end = amplitude_mult;

        if (op < inst->carrier_count) {
            // carrier
            double pitch_expression_start;
            if (voice->op_states[op].has_prev_pitch_expression) {
                pitch_expression_start = voice->op_states[op].prev_pitch_expression;
            } else {
                pitch_expression_start = pow(2.0, -(pitch_start - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING);
            }
            const double pitch_expression_end = pow(2.0, -(pitch_end - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING);

            voice->op_states[op].has_prev_pitch_expression = TRUE;
            voice->op_states[op].prev_pitch_expression = pitch_expression_end;

            expression_start *= pitch_expression_start;
            expression_end *= pitch_expression_end;

            total_carrier_expr += amplitude_curve;
        } else {
            // modulator
            expression_start *= SINE_WAVE_LENGTH * 1.5;
            expression_end *= SINE_WAVE_LENGTH * 1.5;

            sine_expr_boost *= 1.0 - min(1.0, inst->amplitudes[op] / 15.0);
        }

        expression_start *= voice->env_computer.envelope_starts[BPBX_ENV_INDEX_OPERATOR_AMP0 + op];
        expression_end *= voice->env_computer.envelope_ends[BPBX_ENV_INDEX_OPERATOR_AMP0 + op];

        voice->op_states[op].expression = expression_start;
        voice->op_states[op].expression_delta = (expression_end - expression_start) / rounded_samples_per_tick;
    }

    sine_expr_boost *= (pow(2.0, (2.0 - 1.4 * inst->feedback / 15.0)) - 1.0) / 3.0;
    sine_expr_boost *= 1.0 - min(1.0, max(0.0, total_carrier_expr - 1) / 2.0);
    sine_expr_boost = 1.0 + sine_expr_boost * 3.0;

    const double expr_start = VOICE_BASE_EXPRESSION * sine_expr_boost * note_filter_expression * fade_expr_start *
        voice->env_computer.envelope_starts[BPBX_ENV_INDEX_NOTE_VOLUME];
    const double expr_end = VOICE_BASE_EXPRESSION * sine_expr_boost * note_filter_expression * fade_expr_end *
        voice->env_computer.envelope_ends[BPBX_ENV_INDEX_NOTE_VOLUME];
    
    voice->expression = expr_start;
    voice->expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;

    const double feedback_amplitude = SINE_WAVE_LENGTH * 0.3 * inst->feedback / 15.0;
    const double feedback_start = feedback_amplitude * voice->env_computer.envelope_starts[BPBX_ENV_INDEX_FEEDBACK_AMP];
    const double feedback_end = feedback_amplitude * voice->env_computer.envelope_ends[BPBX_ENV_INDEX_FEEDBACK_AMP];
    voice->feedback_mult = feedback_start;
    voice->feedback_delta = (feedback_end - feedback_start) / rounded_samples_per_tick;

    if (released) {
        voice->ticks_since_release += 1.0;
        voice->secs_since_release += samples_per_tick;
    }
}

void fm_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx) {
    const double sample_rate = src_inst->sample_rate;
    const double sample_len = 1.0 / sample_rate;
    const size_t frame_count = run_ctx->frame_count;
    float *const out_samples = run_ctx->out_samples;
    const double beat = run_ctx->beat;

    // create copy of instrument on stack, for cache optimization purposes
    // Except this is stupid i don't know if i'll continue following this
    fm_inst_s inst = *src_inst->fm;
    setup_algorithm(&inst);

    fm_algo_f algo_func = fm_algorithm_table[inst.algorithm * BPBX_FM_FEEDBACK_TYPE_COUNT + inst.feedback_type];
    const double samples_per_tick = calc_samples_per_tick(run_ctx->bpm, sample_rate);
    const double fade_in = src_inst->fade_in;
    const double fade_out = src_inst->fade_out;

    const double mod_x = src_inst->mod_x;
    const double mod_y = src_inst->mod_y;
    const double mod_w = run_ctx->mod_wheel;

    // zero-initialize sample data
    //memset(out_samples, 0, frame_count * 2 * sizeof(float));
    
    double inst_volume = inst_volume_to_mult(src_inst->volume);

    //const double secs_per_tick = samples_per_tick * sample_len;
    for (size_t frame = 0; frame < run_ctx->frame_count; frame++) {
        float *out_l = &out_samples[frame * 2];
        float *out_r = &out_samples[frame * 2 + 1];

        // compute a tick
        if (++src_inst->frame_counter >= (size_t)samples_per_tick) {
            src_inst->frame_counter = 0;

            // update vibrato lfo
            bpbx_vibrato_params_s vibrato = src_inst->vibrato;
            bpbx_vibrato_preset_params(src_inst->vibrato_preset, &vibrato);

            src_inst->vibrato_time_start = src_inst->vibrato_time_end;
            src_inst->vibrato_time_end += samples_per_tick * sample_len * vibrato.speed;

            for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
                fm_voice_s *voice = inst.voices + i;
                if (!voice->triggered) continue;
                if (voice->is_on_last_tick) {
                    voice->triggered = FALSE;
                    voice->active = FALSE;
                    continue;
                }
                
                voice->active = TRUE;

                compute_voice(&inst, voice, (tone_compute_s) {
                    .base_inst = src_inst,

                    .samples_per_tick = samples_per_tick,
                    .sample_rate = sample_rate,
                    .fade_in = fade_in,
                    .fade_out = fade_out,
                    .envelope_count = src_inst->envelope_count,
                    .envelopes = src_inst->envelopes,
                    .cur_beat = beat,
                    .vibrato_params = &vibrato,

                    .mod_x = mod_x,
                    .mod_y = mod_y,
                    .mod_w = mod_w
                });
            }

            src_inst->last_eq = src_inst->eq;
            src_inst->last_note_filter = src_inst->note_filter;
        }

        *out_l = 0.0f;
        *out_r = 0.0f;

        for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
            fm_voice_s *voice = inst.voices + i;
            if (!voice->active) continue;
            
            // convert to operable values
            for (int op = 0; op < FM_OP_COUNT; op++) {
                voice->op_states[op].phase *= SINE_WAVE_LENGTH;
                voice->op_states[op].phase_delta *= SINE_WAVE_LENGTH;
            }
            
            // process this frame
            double x0 = (algo_func(voice, voice->feedback_mult) * voice->expression * inst_volume) * voice->volume;
            double x1 = voice->note_filter_input[0];
            double x2 = voice->note_filter_input[1];
            
            float sample;
            if (voice->filters_enabled) {
                sample = (float) apply_filters(x0, x1, x2, voice->note_filters);
            } else {
                sample = (float) x0;
            }

            x2 = x1;
            x1 = x0;

            voice->op_states[0].phase += voice->op_states[0].phase_delta;
            voice->op_states[1].phase += voice->op_states[1].phase_delta;
            voice->op_states[2].phase += voice->op_states[2].phase_delta;
            voice->op_states[3].phase += voice->op_states[3].phase_delta;

            voice->op_states[0].phase_delta *= voice->op_states[0].phase_delta_scale;
            voice->op_states[1].phase_delta *= voice->op_states[1].phase_delta_scale;
            voice->op_states[2].phase_delta *= voice->op_states[2].phase_delta_scale;
            voice->op_states[3].phase_delta *= voice->op_states[3].phase_delta_scale;

            voice->op_states[0].expression += voice->op_states[0].expression_delta;
            voice->op_states[1].expression += voice->op_states[1].expression_delta;
            voice->op_states[2].expression += voice->op_states[2].expression_delta;
            voice->op_states[3].expression += voice->op_states[3].expression_delta;
            
            voice->expression += voice->expression_delta;
            voice->feedback_mult += voice->feedback_delta;

            // output to left and right channels
            *out_l += sample;
            *out_r += sample;

            voice->note_filter_input[0] = x1;
            voice->note_filter_input[1] = x2;
            
            // convert from operable values
            for (int op = 0; op < FM_OP_COUNT; op++) {
                voice->op_states[op].phase /= SINE_WAVE_LENGTH;
                voice->op_states[op].phase_delta /= SINE_WAVE_LENGTH;
            }
        }
    }

    // for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
    //     fm_voice_s *voice = inst.voices + i;
    //     if (!voice->active) continue;

    //     size_t buffer_idx = 0;
    //     size_t frame = 0;

    //     while (frame < frame_count) {
    //         if (voice->remaining_samples == 0) {
    //             voice->remaining_samples += (size_t)ceil(samples_per_tick);

    //             compute_voice(&inst, voice, (tone_compute_s) {
    //                 .base_inst = src_inst,

    //                 .samples_per_tick = samples_per_tick,
    //                 .sample_rate = sample_rate,
    //                 .fade_in = fade_in,
    //                 .fade_out = fade_out,
    //                 .envelope_count = src_inst->envelope_count,
    //                 .envelopes = src_inst->envelopes,
    //                 .cur_beat = beat,

    //                 .mod_x = mod_x,
    //                 .mod_y = mod_y,
    //                 .mod_w = mod_w
    //             });
    //         }

    //         // convert to operable values
    //         for (int op = 0; op < FM_OP_COUNT; op++) {
    //             voice->op_states[op].phase *= SINE_WAVE_LENGTH;
    //             voice->op_states[op].phase_delta *= SINE_WAVE_LENGTH;
    //         }
            
    //         size_t end_frame = frame + voice->remaining_samples;
    //         if (end_frame > frame_count) end_frame = frame_count;
    //         //assert((int64_t)end_frame - frame >= 0);
    //         size_t run_length = end_frame - frame;

    //         for (; frame < end_frame; frame++) {      
    //             float sample = (float) (algo_func(voice, voice->feedback_mult) * voice->expression * inst_volume) * voice->volume;

    //             voice->op_states[0].phase += voice->op_states[0].phase_delta;
    //             voice->op_states[1].phase += voice->op_states[1].phase_delta;
    //             voice->op_states[2].phase += voice->op_states[2].phase_delta;
    //             voice->op_states[3].phase += voice->op_states[3].phase_delta;

    //             voice->op_states[0].phase_delta *= voice->op_states[0].phase_delta_scale;
    //             voice->op_states[1].phase_delta *= voice->op_states[1].phase_delta_scale;
    //             voice->op_states[2].phase_delta *= voice->op_states[2].phase_delta_scale;
    //             voice->op_states[3].phase_delta *= voice->op_states[3].phase_delta_scale;

    //             voice->op_states[0].expression += voice->op_states[0].expression_delta;
    //             voice->op_states[1].expression += voice->op_states[1].expression_delta;
    //             voice->op_states[2].expression += voice->op_states[2].expression_delta;
    //             voice->op_states[3].expression += voice->op_states[3].expression_delta;
                
    //             voice->expression += voice->expression_delta;
    //             voice->feedback_mult += voice->feedback_delta;

    //             // assume two channels
    //             out_samples[buffer_idx++] += sample;
    //             out_samples[buffer_idx++] += sample;
    //         }

    //         voice->remaining_samples -= run_length;
            
    //         // convert from operable values
    //         for (int op = 0; op < FM_OP_COUNT; op++) {
    //             voice->op_states[op].phase /= SINE_WAVE_LENGTH;
    //             voice->op_states[op].phase_delta /= SINE_WAVE_LENGTH;
    //         }
    //     }

    // }

    *src_inst->fm = inst;
}








//////////////////
//     DATA     //
//////////////////

static const char *algo_enum_values[] = {
    "1←(2 3 4)",
    "1←(2 3←4)",
    "1←2←(3 4)",
    "1←(2 3)←4",
    "1←2←3←4",
    "1←3 2←4",
    "1 2←(3 4)",
    "1 2←3←4",
    "(1 2)←3←4",
    "(1 2)←(3 4)",
    "1 2 3←4",
    "(1 2 3)←4",
    "1 2 3 4",
};

static const char *freq_enum_values[] = {
    "0.12×",
    "0.25×",
    "0.5×",
    "0.75×",
    "1×",
    "~1×",
    "2×",
    "~2×",
    "3×",
    "3.5×",
    "4×",
    "~4×",
    "5×",
    "6×",
    "7×",
    "8×",
    "9×",
    "10×",
    "11×",
    "12×",
    "13×",
    "14×",
    "15×",
    //ultrabox
    "16×",
    "17×",
    //ultrabox
    "18×",
    "19×",
    //ultrabox
    "20×",
    "~20×",
    // dogebox (maybe another mod also adds this? I got it from dogebox)
    "25×",
    "50×",
    "75×",
    "100×",
    //50 and 100 are from dogebox
    //128 and 256 from slarmoo's box
    "128×",
    "256×",
};

static const char *feedback_enum_values[] = {
    "1⟲",
    "2⟲",
    "3⟲",
    "4⟲",
    "1⟲ 2⟲",
    "3⟲ 4⟲",
    "1⟲ 2⟲ 3⟲",
    "2⟲ 3⟲ 4⟲",
    "1⟲ 2⟲ 3⟲ 4⟲",
    "1→2",
    "1→3",
    "1→4",
    "2→3",
    "2→4",
    "3→4",
    "1→3 2→4",
    "1→4 2→3",
    "1→2→3→4",
};

const bpbx_inst_param_info_s fm_param_info[BPBX_FM_PARAM_COUNT] = {
    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Algorithm",
        .group = "FM",
        .min_value = 0,
        .max_value = 12,
        .default_value = 0,

        .enum_values = algo_enum_values
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        .name = "Operator 1 Frequency",
        .group = "FM",

        .min_value = 0,
        .max_value = BPBX_FM_FREQ_COUNT-1,
        .default_value = 4,

        .enum_values = freq_enum_values
    },
    {
        .type = BPBX_PARAM_DOUBLE,
        .name = "Operator 1 Volume",
        .group = "FM",

        .min_value = 0,
        .max_value = 15,
        .default_value = 15
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        .name = "Operator 2 Frequency",
        .group = "FM",
        
        .min_value = 0,
        .max_value = BPBX_FM_FREQ_COUNT-1,
        .default_value = 4,

        .enum_values = freq_enum_values
    },
    {
        .type = BPBX_PARAM_DOUBLE,
        .name = "Operator 2 Volume",
        .group = "FM",

        .min_value = 0,
        .max_value = 15,
        .default_value = 0
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        .name = "Operator 3 Frequency",
        .group = "FM",

        .min_value = 0,
        .max_value = BPBX_FM_FREQ_COUNT-1,
        .default_value = 4,
        
        .enum_values = freq_enum_values
    },
    {
        .type = BPBX_PARAM_DOUBLE,
        .name = "Operator 3 Volume",
        .group = "FM",

        .min_value = 0,
        .max_value = 15,
        .default_value = 0
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        .name = "Operator 4 Frequency",
        .group = "FM",

        .min_value = 0,
        .max_value = BPBX_FM_FREQ_COUNT-1,
        .default_value = 4,
        
        .enum_values = freq_enum_values
    },
    {
        .type = BPBX_PARAM_DOUBLE,
        .name = "Operator 4 Volume",
        .group = "FM",

        .min_value = 0,
        .max_value = 15,
        .default_value = 0
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Feedback Type",
        .group = "FM",
        .min_value = 0,
        .max_value = BPBX_FM_FEEDBACK_TYPE_COUNT-1,
        .default_value = 0,

        .enum_values = feedback_enum_values
    },

    {
        .type = BPBX_PARAM_DOUBLE,
        .name = "Feedback Volume",
        .group = "FM",

        .min_value = 0,
        .max_value = 15,
        .default_value = 0
    }
};

const bpbx_envelope_compute_index_e fm_env_targets[FM_MOD_COUNT] = {
    BPBX_ENV_INDEX_OPERATOR_FREQ0,
    BPBX_ENV_INDEX_OPERATOR_AMP0,
    BPBX_ENV_INDEX_OPERATOR_FREQ1,
    BPBX_ENV_INDEX_OPERATOR_AMP1,
    BPBX_ENV_INDEX_OPERATOR_FREQ2,
    BPBX_ENV_INDEX_OPERATOR_AMP2,
    BPBX_ENV_INDEX_OPERATOR_FREQ3,
    BPBX_ENV_INDEX_OPERATOR_AMP3,
    BPBX_ENV_INDEX_FEEDBACK_AMP
};

const size_t fm_param_addresses[BPBX_FM_PARAM_COUNT] = {
    offsetof(fm_inst_s, algorithm),
    offsetof(fm_inst_s, freq_ratios[0]),
    offsetof(fm_inst_s, amplitudes[0]),
    offsetof(fm_inst_s, freq_ratios[1]),
    offsetof(fm_inst_s, amplitudes[1]),
    offsetof(fm_inst_s, freq_ratios[2]),
    offsetof(fm_inst_s, amplitudes[2]),
    offsetof(fm_inst_s, freq_ratios[3]),
    offsetof(fm_inst_s, amplitudes[3]),
    offsetof(fm_inst_s, feedback_type),
    offsetof(fm_inst_s, feedback),
};

static fm_freq_data_s frequency_data[BPBX_FM_FREQ_COUNT] = {
    { .mult = 0.125,    .hz_offset= 0.0,      .amplitude_sign = 1.0 },
    { .mult= 0.25,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 0.5,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 0.75,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 1.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 1.0,       .hz_offset= 1.5,      .amplitude_sign= -1.0 },
    { .mult= 2.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 2.0,       .hz_offset= -1.3,     .amplitude_sign= -1.0 },
    { .mult= 3.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 3.5,       .hz_offset= -0.05,    .amplitude_sign= 1.0 },
    { .mult= 4.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 4.0,       .hz_offset= -2.4,     .amplitude_sign= -1.0 },
    { .mult= 5.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 6.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 7.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 8.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 9.0,       .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 10.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 11.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 12.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 13.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 14.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 15.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    //ultrabox
    { .mult= 16.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 17.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    //ultrabox
    { .mult= 18.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 19.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    //ultrabox
    { .mult= 20.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 20.0,      .hz_offset= -5.0,     .amplitude_sign= -1.0 },
    // dogebox (maybe another mod also adds this? I got it from dogebox)
    { .mult= 25.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 50.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 75.0,      .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 100.0,     .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    //50 and 100 are from dogebox
    //128 and 256 from slarmoo's box
    { .mult= 128.0,     .hz_offset= 0.0,      .amplitude_sign= 1.0 },
    { .mult= 250.0,     .hz_offset= 0.0,      .amplitude_sign= 1.0},
};

static int algo_associated_carriers[BPBX_FM_ALGORITHM_COUNT][4] = {
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
    { 1, 1, 1, 1 },
    { 1, 2, 1, 2 },
    { 1, 2, 2, 2 },
    { 1, 2, 2, 2 },
    { 1, 2, 2, 2 },
    { 1, 2, 2, 2 },
    { 1, 2, 3, 3 },
    { 1, 2, 3, 3 },
    { 1, 2, 3, 4 }
};

static double carrier_intervals[] = {0.0, 0.04, -0.073, 0.091};