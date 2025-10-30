#include <assert.h>
#include <string.h>
#include <time.h> // for seeding the prng
#include "synth.h"
#include "../util.h"
#include "../context.h"
#include "../wavetables.h"

// Spectrum can be in pitch or noise
// channels, the expression is doubled for
// noise.
#define SPECTRUM_BASE_EXPRESSION 0.3
#define SPECTRUM_BASE_PITCH 24

typedef struct spectrum_voice {
    inst_base_voice_s base;

    double phase;
    double phase_delta;
    double phase_delta_scale;

    double noise_sample;

    double prev_pitch_expression;
    bool has_prev_pitch_expression;
} spectrum_voice_s;

typedef struct spectrum_inst {
    bpbxsyn_synth_s base;
    
    uint8_t is_noise_channel; // a boolean
    uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT];
    int control_hash;

    spectrum_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];

    float wave[SPECTRUM_WAVE_LENGTH + 1];
    prng_state_s prng_state;
} spectrum_inst_s;

static int hash_spectrum_controls(const uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT]) {
    const int hash_mult =
        bbsyn_fitting_power_of_two(BPBXSYN_SPECTRUM_CONTROL_MAX + 2) - 1;
    
    int hash = 0;
    for (int i = 0; i < BPBXSYN_SPECTRUM_CONTROL_COUNT; ++i) {
        hash = ((hash * hash_mult) + controls[i]);
    }

    return hash;
}

static void spectrum_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst)
{
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;
    *inst = (spectrum_inst_s){0};
    bbsyn_inst_init(ctx, &inst->base, BPBXSYN_SYNTH_SPECTRUM);

    // this is the default spectrum configuration for pitch channels
    for (int i = 0; i < BPBXSYN_SPECTRUM_CONTROL_COUNT; ++i) {
        const bool isHarmonic = i == 0 || i == 7 || i == 11 || i == 14 ||
                                i == 16 || i == 18 || i == 21 || i == 23 ||
                                i >= 25;
        inst->controls[i] = isHarmonic ?
            (uint8_t)maxi(0, (int)round(BPBXSYN_SPECTRUM_CONTROL_MAX * (1 - (double)i / 30))) :
            0;
    }

    inst->control_hash = hash_spectrum_controls(inst->controls);
    bbsyn_generate_spectrum_wave(&inst->base.ctx->wavetables, inst->controls,
                                8.0, inst->wave);
    
    inst->prng_state = bbsyn_random_seeded_state((uint64_t)clock());
}

static bpbxsyn_voice_id spectrum_note_on(bpbxsyn_synth_s *p_inst, int key,
                                         double velocity, int32_t length)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id voice_id = bbsyn_trigger_voice(p_inst,
                                              GENERIC_LIST(inst->voices),
                                              key, velocity, length,
                                              &continuation);

    if (!continuation) {
        spectrum_voice_s *voice = &inst->voices[voice_id];
        *voice = (spectrum_voice_s) {
            .base = voice->base
        };
    }

    return voice_id;
}

static void spectrum_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    bbsyn_release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

static void spectrum_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    bbsyn_release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void compute_voice(const bpbxsyn_synth_s *const base_inst,
                          inst_base_voice_s *base_voice,
                          voice_compute_s *compute_data)
{
    const spectrum_inst_s *const inst = (spectrum_inst_s*)base_inst;
    spectrum_voice_s *voice = (spectrum_voice_s*)base_voice;
    
    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const double pitch_damping = 28.0;
    const double ref_pitch = SPECTRUM_BASE_PITCH;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start;
    const double end_pitch = voice->base.current_key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (voice->has_prev_pitch_expression) {
        pitch_expression_start = voice->prev_pitch_expression;
    } else {
        pitch_expression_start =
            calc_pitch_expression_ex(start_pitch, ref_pitch, pitch_damping);
    }
    const double pitch_expression_end =
        calc_pitch_expression_ex(end_pitch, ref_pitch, pitch_damping);
    voice->has_prev_pitch_expression = true;
    voice->prev_pitch_expression = pitch_expression_end;

    double base_expr = inst->is_noise_channel ?
        SPECTRUM_BASE_EXPRESSION * 2.0 :
        SPECTRUM_BASE_EXPRESSION;

    const double expr_start = base_expr * varying->expr_start * pitch_expression_start;
    const double expr_end = base_expr * varying->expr_end * pitch_expression_end;

    const double start_freq = key_to_hz_d(start_pitch);
    const double end_freq = key_to_hz_d(end_pitch);

    voice->phase_delta = start_freq * sample_len;
    voice->phase_delta_scale = pow(end_freq / start_freq, 1.0 / rounded_samples_per_tick);
    
    voice->base.expression = expr_start;
    voice->base.expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;
}

static void spectrum_tick(bpbxsyn_synth_s *p_inst,
                          const bpbxsyn_tick_ctx_s *tick_ctx)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    bbsyn_inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });

    // determine if wave should be recalculated
    int new_hash = hash_spectrum_controls(inst->controls);
    if (new_hash != inst->control_hash) {
        inst->control_hash = new_hash;
        bbsyn_generate_spectrum_wave(&inst->base.ctx->wavetables,
                                     inst->controls, 8.0, inst->wave);
    }
}

static void spectrum_run(bpbxsyn_synth_s *p_inst, float *samples,
                         size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    memset(samples, 0, frame_count * sizeof(float));

    float *wave = inst->wave;

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; i++) {
        spectrum_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;
        
        const int samples_in_period = 1 << 7;
        double phase_delta = voice->phase_delta * samples_in_period;
        const double phase_delta_scale = voice->phase_delta_scale;
        double expression = voice->base.expression;
        const double expression_delta = voice->base.expression_delta;

        double noise_sample = voice->noise_sample;

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        double phase = fmod(voice->phase, 1.0) * SPECTRUM_WAVE_LENGTH;

        // Zero phase means the tone was reset, just give noise a random start
        // phase instead.
        if (voice->phase == 0.0)
            phase = bbsyn_find_random_zero_crossing(&inst->prng_state, inst->wave, SPECTRUM_WAVE_LENGTH) + phase_delta;
        
        const int phase_mask = SPECTRUM_WAVE_LENGTH - 1;

        // This is for a "legacy" style simplified 1st order lowpass filter with
        // a cutoff frequency that is relative to the tone's fundamental
        // frequency.
        const double pitch_relative_filter = min(1.0, phase_delta);

        for (size_t smp = 0; smp < frame_count; ++smp) {
            const int phase_int = (int)phase;
            const int index = phase_int & phase_mask;
            double wave_sample = wave[index];
            const double phase_ratio = phase - phase_int;

            wave_sample += (wave[index + 1] - wave_sample) * phase_ratio;
            noise_sample +=
                (wave_sample - noise_sample) * pitch_relative_filter;

            const double x0 = noise_sample;
            double sample =
                bbsyn_apply_filters(x0, x1, x2, voice->base.note_filters);
            x2 = x1;
            x1 = x0;

            phase += phase_delta;
            phase_delta *= phase_delta_scale;

            const double output = sample * expression;
            expression += expression_delta;

            samples[smp] += (float)output;
        }

        voice->phase = phase / SPECTRUM_WAVE_LENGTH;
        voice->phase_delta = phase_delta / samples_in_period;
        voice->base.expression = expression;
        voice->noise_sample = noise_sample;

        bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;
    }
}










//////////
// DATA //
//////////

/*
import sys

default_values = dict([
    (0, 7),
    (7, 5),
    (11, 4),
    (14, 4),
    (16, 2),
    (18, 3),
    (21, 2),
    (23, 2),
    (25, 1),
    (26, 1),
    (27, 1)
])

template = """    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl@",
        .group = "Spectrum",
        .name = "Spectrum Control #",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = $
    },"""

for i in range(30):
    numstr = str(i + 1)
    print(template
        .replace("#", numstr)
        .replace("@", numstr.zfill(2))
        .replace("$", str(default_values.get(i, 0)))
    )
*/
static const bpbxsyn_param_info_s spectrum_param_info[BPBXSYN_SPECTRUM_PARAM_COUNT] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptNoise",
        .group = "Spectrum",
        .name = "Is Noise Channel?",
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,

        .enum_values = bbsyn_yes_no_enum_values
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl01",
        .group = "Spectrum",
        .name = "Spectrum Control 1",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 7
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl02",
        .group = "Spectrum",
        .name = "Spectrum Control 2",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl03",
        .group = "Spectrum",
        .name = "Spectrum Control 3",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl04",
        .group = "Spectrum",
        .name = "Spectrum Control 4",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl05",
        .group = "Spectrum",
        .name = "Spectrum Control 5",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl06",
        .group = "Spectrum",
        .name = "Spectrum Control 6",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl07",
        .group = "Spectrum",
        .name = "Spectrum Control 7",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl08",
        .group = "Spectrum",
        .name = "Spectrum Control 8",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 5
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl09",
        .group = "Spectrum",
        .name = "Spectrum Control 9",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl10",
        .group = "Spectrum",
        .name = "Spectrum Control 10",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl11",
        .group = "Spectrum",
        .name = "Spectrum Control 11",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl12",
        .group = "Spectrum",
        .name = "Spectrum Control 12",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 4
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl13",
        .group = "Spectrum",
        .name = "Spectrum Control 13",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl14",
        .group = "Spectrum",
        .name = "Spectrum Control 14",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl15",
        .group = "Spectrum",
        .name = "Spectrum Control 15",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 4
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl16",
        .group = "Spectrum",
        .name = "Spectrum Control 16",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl17",
        .group = "Spectrum",
        .name = "Spectrum Control 17",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 2
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl18",
        .group = "Spectrum",
        .name = "Spectrum Control 18",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl19",
        .group = "Spectrum",
        .name = "Spectrum Control 19",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 3
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl20",
        .group = "Spectrum",
        .name = "Spectrum Control 20",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl21",
        .group = "Spectrum",
        .name = "Spectrum Control 21",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl22",
        .group = "Spectrum",
        .name = "Spectrum Control 22",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 2
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl23",
        .group = "Spectrum",
        .name = "Spectrum Control 23",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl24",
        .group = "Spectrum",
        .name = "Spectrum Control 24",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 2
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl25",
        .group = "Spectrum",
        .name = "Spectrum Control 25",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl26",
        .group = "Spectrum",
        .name = "Spectrum Control 26",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 1
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl27",
        .group = "Spectrum",
        .name = "Spectrum Control 27",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 1
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl28",
        .group = "Spectrum",
        .name = "Spectrum Control 28",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 1
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl29",
        .group = "Spectrum",
        .name = "Spectrum Control 29",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl30",
        .group = "Spectrum",
        .name = "Spectrum Control 30",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
};

static const size_t spectrum_param_addresses[BPBXSYN_SPECTRUM_PARAM_COUNT] = {
    offsetof(spectrum_inst_s, is_noise_channel),
    offsetof(spectrum_inst_s, controls[0]),
    offsetof(spectrum_inst_s, controls[1]),
    offsetof(spectrum_inst_s, controls[2]),
    offsetof(spectrum_inst_s, controls[3]),
    offsetof(spectrum_inst_s, controls[4]),
    offsetof(spectrum_inst_s, controls[5]),
    offsetof(spectrum_inst_s, controls[6]),
    offsetof(spectrum_inst_s, controls[7]),
    offsetof(spectrum_inst_s, controls[8]),
    offsetof(spectrum_inst_s, controls[9]),
    offsetof(spectrum_inst_s, controls[10]),
    offsetof(spectrum_inst_s, controls[11]),
    offsetof(spectrum_inst_s, controls[12]),
    offsetof(spectrum_inst_s, controls[13]),
    offsetof(spectrum_inst_s, controls[14]),
    offsetof(spectrum_inst_s, controls[15]),
    offsetof(spectrum_inst_s, controls[16]),
    offsetof(spectrum_inst_s, controls[17]),
    offsetof(spectrum_inst_s, controls[18]),
    offsetof(spectrum_inst_s, controls[19]),
    offsetof(spectrum_inst_s, controls[20]),
    offsetof(spectrum_inst_s, controls[21]),
    offsetof(spectrum_inst_s, controls[22]),
    offsetof(spectrum_inst_s, controls[23]),
    offsetof(spectrum_inst_s, controls[24]),
    offsetof(spectrum_inst_s, controls[25]),
    offsetof(spectrum_inst_s, controls[26]),
    offsetof(spectrum_inst_s, controls[27]),
    offsetof(spectrum_inst_s, controls[28]),
    offsetof(spectrum_inst_s, controls[29]),
};







const inst_vtable_s bbsyn_inst_spectrum_vtable = {
    .struct_size = sizeof(spectrum_inst_s),

    .param_count = BPBXSYN_SPECTRUM_PARAM_COUNT,
    .param_info = spectrum_param_info,
    .param_addresses = spectrum_param_addresses,

    .inst_init = spectrum_init,
    .inst_note_on = spectrum_note_on,
    .inst_note_off = spectrum_note_off,
    .inst_note_all_off = spectrum_note_all_off,

    .inst_tick = spectrum_tick,
    .inst_run = spectrum_run
};