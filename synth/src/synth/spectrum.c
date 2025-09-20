#include "spectrum.h"

#include <assert.h>
#include <string.h>
#include "../util.h"
#include "../context.h"
#include "../wavetables.h"

// Spectrum can be in pitch or noise
// channels, the expression is doubled for
// noise.
#define SPECTRUM_BASE_EXPRESSION 0.3

static int hash_spectrum_controls(
    const uint8_t controls[BPBXSYN_SPECTRUM_CONTROL_COUNT])
{
    const int hash_mult =
        fitting_power_of_two(BPBXSYN_SPECTRUM_CONTROL_MAX + 2) - 1;
    
    int hash = 0;
    for (int i = 0; i < BPBXSYN_SPECTRUM_CONTROL_COUNT; ++i) {
        hash = ((hash * hash_mult) + controls[i]);
    }

    return hash;
}

void bpbxsyn_synth_init_spectrum(bpbxsyn_context_s *ctx,
                                 spectrum_inst_s *inst)
{
    *inst = (spectrum_inst_s){0};
    inst_init(ctx, &inst->base, BPBXSYN_SYNTH_SPECTRUM);

    // this is the default spectrum configuration for pitch channels
    for (int i = 0; i < BPBXSYN_SPECTRUM_CONTROL_COUNT; ++i) {
        const bool isHarmonic = i == 0 || i == 7 || i == 11 || i == 14 ||
                                i == 16 || i == 18 || i == 21 || i == 23 ||
                                i >= 25;
        inst->controls[i] = isHarmonic ?
            maxi(0, (int)round(BPBXSYN_SPECTRUM_CONTROL_MAX * (1 - (double)i / 30))) :
            0;
    }

    inst->control_hash = hash_spectrum_controls(inst->controls);
    generate_spectrum_wave(&inst->base.ctx->wavetables, inst->controls, 8.0,
                           inst->wave);
}

bpbxsyn_voice_id spectrum_note_on(bpbxsyn_synth_s *p_inst, int key,
                                  double velocity, int32_t length)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id voice_id = trigger_voice(p_inst,
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

void spectrum_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

void spectrum_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    release_all_voices(p_inst, GENERIC_LIST(inst->voices));
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

void spectrum_tick(bpbxsyn_synth_s *p_inst,
                   const bpbxsyn_tick_ctx_s *tick_ctx)
{
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });

    // determine if wave should be recalculated
    int new_hash = hash_spectrum_controls(inst->controls);
    if (new_hash != inst->control_hash) {
        inst->control_hash = new_hash;
        generate_spectrum_wave(&inst->base.ctx->wavetables, inst->controls,
                               8.0, inst->wave);
    }
}

void spectrum_run(bpbxsyn_synth_s *p_inst, float *samples, size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_SPECTRUM);
    spectrum_inst_s *inst = (spectrum_inst_s*)p_inst;

    memset(samples, 0, frame_count * sizeof(float));

    float *wave = inst->wave;

    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; i++) {
        spectrum_voice_s *voice = &inst->voices[i];
        if (!voice_is_computing(&voice->base)) continue;
        
        const int samplesInPeriod = 1 << 7;
        double phaseDelta = voice->phase_delta * samplesInPeriod;
        const double phaseDeltaScale = voice->phase_delta_scale;
        double expression = voice->base.expression;
        const double expressionDelta = voice->base.expression_delta;

        double noiseSample = voice->noise_sample;

        double x1 = voice->base.note_filter_input[0];
        double x2 = voice->base.note_filter_input[1];

        double phase = fmod(voice->phase, 1.0) * SPECTRUM_WAVE_LENGTH;
        // Zero phase means the tone was reset, just give noise a random start
        // phase instead.
        if (voice->phase == 0.0)
            phase = find_random_zero_crossing(inst->wave, SPECTRUM_WAVE_LENGTH)
                + phaseDelta;
        
        const int phaseMask = SPECTRUM_WAVE_LENGTH - 1;

        // This is for a "legacy" style simplified 1st order lowpass filter with
        // a cutoff frequency that is relative to the tone's fundamental
        // frequency.
        const double pitchRelativefilter = min(1.0, phaseDelta);

        for (size_t smp = 0; smp < frame_count; ++smp) {
            const int phaseInt = (int)phase;
            const int index = phaseInt & phaseMask;
            double waveSample = wave[index];
            const double phaseRatio = phase - phaseInt;

            waveSample += (wave[index + 1] - waveSample) * phaseRatio;
            noiseSample += (waveSample - noiseSample) * pitchRelativefilter;

            const double x0 = noiseSample;
            double sample = apply_filters(x0, x1, x2, voice->base.note_filters);
            x2 = x1;
            x1 = x0;

            phase += phaseDelta;
            phaseDelta *= phaseDeltaScale;

            const double output = sample * expression;
            expression += expressionDelta;

            samples[smp] += output;
        }

        voice->phase = phase / SPECTRUM_WAVE_LENGTH;
        voice->phase_delta = phaseDelta / samplesInPeriod;
        voice->base.expression = expression;
        voice->noise_sample = noiseSample;

        sanitize_filters(voice->base.note_filters, 8);
        voice->base.note_filter_input[0] = x1;
        voice->base.note_filter_input[1] = x2;
    }
}

const char *yes_no_values[] = { "No", "Yes" };

/*
import sys

template = """    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl@",
        .name = "Spectrum Control #",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },"""

for i in range(30):
    numstr = str(i + 1)
    print(template.replace("#", numstr).replace("@", numstr.zfill(2)))
*/
const bpbxsyn_param_info_s spectrum_param_info[BPBXSYN_SPECTRUM_PARAM_COUNT] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptNoise",
        .name = "Is Noise Channel?",
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,

        .enum_values = yes_no_values
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl01",
        .name = "Spectrum Control 1",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl02",
        .name = "Spectrum Control 2",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl03",
        .name = "Spectrum Control 3",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl04",
        .name = "Spectrum Control 4",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl05",
        .name = "Spectrum Control 5",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl06",
        .name = "Spectrum Control 6",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl07",
        .name = "Spectrum Control 7",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl08",
        .name = "Spectrum Control 8",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl09",
        .name = "Spectrum Control 9",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl10",
        .name = "Spectrum Control 10",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl11",
        .name = "Spectrum Control 11",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl12",
        .name = "Spectrum Control 12",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl13",
        .name = "Spectrum Control 13",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl14",
        .name = "Spectrum Control 14",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl15",
        .name = "Spectrum Control 15",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl16",
        .name = "Spectrum Control 16",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl17",
        .name = "Spectrum Control 17",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl18",
        .name = "Spectrum Control 18",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl19",
        .name = "Spectrum Control 19",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl20",
        .name = "Spectrum Control 20",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl21",
        .name = "Spectrum Control 21",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl22",
        .name = "Spectrum Control 22",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl23",
        .name = "Spectrum Control 23",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl24",
        .name = "Spectrum Control 24",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl25",
        .name = "Spectrum Control 25",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl26",
        .name = "Spectrum Control 26",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl27",
        .name = "Spectrum Control 27",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl28",
        .name = "Spectrum Control 28",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl29",
        .name = "Spectrum Control 29",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        .id = "sptCtl30",
        .name = "Spectrum Control 30",
        .min_value = 0,
        .max_value = BPBXSYN_SPECTRUM_CONTROL_MAX,
        .default_value = 0
    },
};

const size_t spectrum_param_addresses[BPBXSYN_SPECTRUM_PARAM_COUNT] = {
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







//////////
// DATA //
//////////
const inst_vtable_s inst_spectrum_vtable = {
    .struct_size = sizeof(spectrum_inst_s),

    .param_count = BPBXSYN_SPECTRUM_PARAM_COUNT,
    .param_info = spectrum_param_info,
    .param_addresses = spectrum_param_addresses,

    .inst_init = (inst_init_f)bpbxsyn_synth_init_spectrum,
    .inst_note_on = spectrum_note_on,
    .inst_note_off = spectrum_note_off,
    .inst_note_all_off = spectrum_note_all_off,

    .inst_tick = spectrum_tick,
    .inst_run = spectrum_run
};