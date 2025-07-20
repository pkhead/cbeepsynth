#include "chip.h"

#include <assert.h>
#include "util.h"
#include "wavetables.h"

#define VOICE_BASE_EXPRESSION 0.1 // i don't really care about the original value

void chip_init(chip_inst_s *inst) {
    inst_init(&inst->base, BPBX_INSTRUMENT_CHIP);

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        inst->voices[i].base.active = FALSE;
        inst->voices[i].base.triggered = FALSE;
    }

    inst->waveform = 2;
    inst->unison_type = 0;
}

int chip_midi_on(bpbx_inst_s *inst, int key, int velocity) {
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;

    int voice_index = trigger_voice(inst, chip->voices, sizeof(*chip->voices), key, velocity);
    chip_voice_s *voice = &chip->voices[voice_index];

    return voice_index;
}

void chip_midi_off(bpbx_inst_s *inst, int key, int velocity) {
    assert(inst);
    assert(inst->type == BPBX_INSTRUMENT_CHIP);
    chip_inst_s *const chip = (chip_inst_s*)inst;

    release_voice(inst, chip->voices, sizeof(*chip->voices), key, velocity);
}

static void compute_chip_voice(const bpbx_inst_s *const base_inst, inst_base_voice_s *voice, voice_compute_s *compute_data) {
    const chip_inst_s *const inst = (chip_inst_s*) base_inst;
    chip_voice_s *const chip_voice = (chip_voice_s*) voice;

    const double sample_len = compute_data->varying.sample_len;
    const double samples_per_tick = compute_data->varying.samples_per_tick;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    wavetable_desc_s wavetable = chip_wavetables[inst->waveform];

    const double expr_start = VOICE_BASE_EXPRESSION * varying->expr_start * wavetable.expression;
    const double expr_end = VOICE_BASE_EXPRESSION * varying->expr_end * wavetable.expression;

    const double start_pitch = (double)voice->key + compute_data->varying.interval_start;
    const double end_pitch = (double)voice->key + compute_data->varying.interval_end;

    const double start_freq = key_to_hz_d(start_pitch);
    const double end_freq = key_to_hz_d(end_pitch);

    chip_voice->phase_delta = start_freq * sample_len;
    chip_voice->phase_delta_scale = pow(end_freq / start_freq, 1.0 / rounded_samples_per_tick);
    
    voice->expression = expr_start;
    voice->expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;
}

static void audio_render_callback(
    float *output_buffer, size_t frames_to_compute,
    double inst_volume, void *userdata_ptr
) {
    chip_inst_s *const chip = userdata_ptr;

    wavetable_desc_s wavetable = chip_wavetables[chip->waveform];
    const size_t wave_length = wavetable.length - 1;

    for (int i = 0; i < BPBX_INST_MAX_VOICES; i++) {
        chip_voice_s *voice = chip->voices + i;
        if (!voice->base.active) continue;
        
        // convert to operable values
        double phase_mix = voice->phase * wave_length;
        double phase_delta = voice->phase_delta * wave_length;

        float *out = output_buffer;

        for (size_t frame = 0; frame < frames_to_compute; frame++) {
            const int phase_int = (int)phase_mix;
            const int index = phase_int % wave_length;
            double sample = wavetable.values[index];
            sample *= wavetable.expression * inst_volume * voice->base.expression * voice->base.volume;

            const float final_sample = (float)sample;

            phase_mix += phase_delta;
            phase_delta *= voice->phase_delta_scale;

            voice->base.expression += voice->base.expression_delta;

            // output frame
            *out++ += final_sample;
            *out++ += final_sample;
        }

        voice->phase = phase_mix / wave_length;
        voice->phase_delta = phase_delta / wave_length;
        
        // process this frame
        // double x0 = (algo_func(voice, voice->feedback_mult) * voice->base.expression * inst_volume) * voice->base.volume;
        // double x1 = voice->base.note_filter_input[0];
        // double x2 = voice->base.note_filter_input[1];
        
        // float sample;
        // if (voice->base.filters_enabled) {
        //     sample = (float) apply_filters(x0, x1, x2, voice->base.note_filters);
        // } else {
        //     sample = (float) x0;
        // }

        // x2 = x1;
        // x1 = x0;

        // voice->base.note_filter_input[0] = x1;
        // voice->base.note_filter_input[1] = x2;
        
        // convert from operable values
        // for (int op = 0; op < FM_OP_COUNT; op++) {
        //     voice->op_states[op].phase /= SINE_WAVE_LENGTH;
        //     voice->op_states[op].phase_delta /= SINE_WAVE_LENGTH;
        // }
    }
}

void chip_run(bpbx_inst_s *src_inst, const bpbx_run_ctx_s *const run_ctx) {
    assert(src_inst);
    assert(src_inst->type == BPBX_INSTRUMENT_CHIP);

    chip_inst_s *const chip = (chip_inst_s*)src_inst;

    inst_audio_process(src_inst, run_ctx, &(audio_compute_s) {
        .voice_list = chip->voices,
        .sizeof_voice = sizeof(*chip->voices),

        .compute_voice = compute_chip_voice,
        .render_block = audio_render_callback,

        .userdata = chip
    });
}

































////////////
//  DATA  //
////////////

static const char *waveform_enum_values[BPBX_CHIP_WAVE_COUNT] = {
    "rounded", "triangle", "square", "1/4 pulse", "1/8 pulse", "sawtooth",
    "double saw", "double pulse", "spiky", "sine", "flute", "harp",
    "sharp clarinet", "soft clarinet", "alto sax", "bassoon",
    "trumpet", "electric guitar", "organ", "pan flute", "glitch"
};

static const char *unison_enum_values[BPBX_UNISON_COUNT] = {
    "none", "shimmer", "hum", "honky tonk", "dissonant",
    "fifth", "octave", "bowed", "piano", "warbled"
};

const bpbx_inst_param_info_s chip_param_info[BPBX_CHIP_PARAM_COUNT] = {
    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Wave",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBX_CHIP_WAVE_COUNT - 1,
        .default_value = BPBX_CHIP_WAVE_SQUARE,

        .enum_values = waveform_enum_values
    },

    {
        .type = BPBX_PARAM_UINT8,
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .name = "Unison",
        .group = "Chip",
        .min_value = 0,
        .max_value = BPBX_UNISON_COUNT - 1,
        .default_value = BPBX_UNISON_NONE,

        .enum_values = unison_enum_values
    },
};

const bpbx_envelope_compute_index_e chip_env_targets[CHIP_MOD_COUNT] = {
    BPBX_ENV_INDEX_UNISON
};

const size_t chip_param_addresses[BPBX_CHIP_PARAM_COUNT] = {
    offsetof(chip_inst_s, waveform),
    offsetof(chip_inst_s, unison_type)
};