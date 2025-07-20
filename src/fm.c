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
    *inst = (fm_inst_s){0};
    inst_init(&inst->base, BPBX_INSTRUMENT_FM);

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst->voices[i].base.active = FALSE;
        inst->voices[i].base.triggered = FALSE;
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
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_FM);
    fm_inst_s *const fm = (fm_inst_s*)inst;

    int voice_index = trigger_voice(inst, fm->voices, sizeof(*fm->voices), key, velocity);
    fm_voice_s *voice = &fm->voices[voice_index];

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

    return voice_index;
}

void fm_midi_off(bpbx_inst_s *inst, int key, int velocity) {
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_FM);
    fm_inst_s *const fm = (fm_inst_s*)inst;

    release_voice(inst, fm->voices, sizeof(*fm->voices), key, velocity);
}

static void compute_fm_voice(const bpbx_inst_s *const base_inst, inst_base_voice_s *const voice, voice_compute_s *compute_data) {
    const fm_inst_s *const inst = (fm_inst_s*)base_inst;
    fm_voice_s *const fm_voice = (fm_voice_s*)voice;

    const double sample_len = compute_data->varying.sample_len;
    const double samples_per_tick = compute_data->varying.samples_per_tick;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    double total_carrier_expr = 0.0;
    double sine_expr_boost = 1.0;

    for (int op = 0; op < FM_OP_COUNT; op++) {
        // john nesky: I'm adding 1000 to the phase to ensure that it's never negative even when modulated
        // by other waves because negative numbers don't work with the modulus operator very well.
        fm_voice->op_states[op].phase = (fmod(fm_voice->op_states[op].phase, 1.0) + 1000);

        fm_freq_data_s *freq_data = &frequency_data[inst->freq_ratios[op]];

        int associated_carrier_idx = algo_associated_carriers[inst->algorithm][op] - 1;
        const double freq_mult = freq_data->mult;
        const double pitch_start = (double)voice->key + varying->interval_start + carrier_intervals[associated_carrier_idx];
        const double pitch_end = (double)voice->key + varying->interval_end + carrier_intervals[associated_carrier_idx];
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

        fm_voice->op_states[op].phase_delta = freq_start * sample_len;
        fm_voice->op_states[op].phase_delta_scale = pow(freq_end / freq_start, 1.0 / rounded_samples_per_tick);

        const double amplitude_curve = operator_amplitude_curve((double) inst->amplitudes[op]);
        const double amplitude_mult = amplitude_curve * freq_data->amplitude_sign;
        double expression_start = amplitude_mult;
        double expression_end = amplitude_mult;

        if (op < inst->carrier_count) {
            // carrier
            double pitch_expression_start;
            if (fm_voice->op_states[op].has_prev_pitch_expression) {
                pitch_expression_start = fm_voice->op_states[op].prev_pitch_expression;
            } else {
                pitch_expression_start = pow(2.0, -(pitch_start - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING);
            }
            const double pitch_expression_end = pow(2.0, -(pitch_end - EXPRESSION_REFERENCE_PITCH) / PITCH_DAMPING);

            fm_voice->op_states[op].has_prev_pitch_expression = TRUE;
            fm_voice->op_states[op].prev_pitch_expression = pitch_expression_end;

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

        fm_voice->op_states[op].expression = expression_start;
        fm_voice->op_states[op].expression_delta = (expression_end - expression_start) / rounded_samples_per_tick;
    }

    sine_expr_boost *= (pow(2.0, (2.0 - 1.4 * inst->feedback / 15.0)) - 1.0) / 3.0;
    sine_expr_boost *= 1.0 - min(1.0, max(0.0, total_carrier_expr - 1) / 2.0);
    sine_expr_boost = 1.0 + sine_expr_boost * 3.0;

    const double expr_start = VOICE_BASE_EXPRESSION * sine_expr_boost * varying->expr_start;
    const double expr_end = VOICE_BASE_EXPRESSION * sine_expr_boost * varying->expr_end;
    
    voice->expression = expr_start;
    voice->expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;

    const double feedback_amplitude = SINE_WAVE_LENGTH * 0.3 * inst->feedback / 15.0;
    const double feedback_start = feedback_amplitude * voice->env_computer.envelope_starts[BPBX_ENV_INDEX_FEEDBACK_AMP];
    const double feedback_end = feedback_amplitude * voice->env_computer.envelope_ends[BPBX_ENV_INDEX_FEEDBACK_AMP];
    fm_voice->feedback_mult = feedback_start;
    fm_voice->feedback_delta = (feedback_end - feedback_start) / rounded_samples_per_tick;
}

typedef struct {
    fm_inst_s *fm;
    fm_algo_f algo_func;
} audio_process_fm_userdata_s;

static void audio_render_callback(
    float *output_buffer, size_t frames_to_compute,
    double inst_volume, void *userdata_ptr
) {
    audio_process_fm_userdata_s *const ud = userdata_ptr;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        fm_voice_s *voice = ud->fm->voices + i;
        if (!voice->base.active) continue;

        float *output_sample = output_buffer;
        
        // convert to operable values
        for (int op = 0; op < FM_OP_COUNT; op++) {
            voice->op_states[op].phase *= SINE_WAVE_LENGTH;
            voice->op_states[op].phase_delta *= SINE_WAVE_LENGTH;
        }

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];
        
        for (size_t sf = 0; sf < frames_to_compute; sf++) {
            // process the frames
            double x0 = (ud->algo_func(voice, voice->feedback_mult) * voice->base.expression * inst_volume) * voice->base.volume;
            
            float sample;
            if (voice->base.filters_enabled) {
                sample = (float) apply_filters(x0, x1, x2, voice->base.note_filters);
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
            
            voice->base.expression += voice->base.expression_delta;
            voice->feedback_mult += voice->feedback_delta;

            // output to left and right channels
            *output_sample++ += sample;
            *output_sample++ += sample;
        }

        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;
        
        // convert from operable values
        for (int op = 0; op < FM_OP_COUNT; op++) {
            voice->op_states[op].phase /= SINE_WAVE_LENGTH;
            voice->op_states[op].phase_delta /= SINE_WAVE_LENGTH;
        }
    }
}

void fm_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx) {
    assert(src_inst);
    assert(src_inst->type == BPBX_INSTRUMENT_FM);
    
    fm_inst_s *const fm = (fm_inst_s*)src_inst;
    setup_algorithm(fm);

    audio_process_fm_userdata_s userdata = (audio_process_fm_userdata_s) {
        .fm = fm,
        .algo_func = fm_algorithm_table[fm->algorithm * BPBX_FM_FEEDBACK_TYPE_COUNT + fm->feedback_type]
    };
    
    inst_audio_process(src_inst, run_ctx, &(audio_compute_s) {
        .voice_list = fm->voices,
        .sizeof_voice = sizeof(*fm->voices),

        .compute_voice = compute_fm_voice,
        .render_block = audio_render_callback,

        .userdata = &userdata
    });
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