#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../include/beepbox_synth.h"

#ifndef BBXS_NO_STD_ALLOC
#include <stdlib.h>
#endif

#include "alloc.h"
#include "context.h"
#include "envelope.h"
#include "filtering.h"
#include "fx/effect.h"
#include "synth/synth.h"
#include "util.h"
#include "wavetables.h"

static const synth_vtable_s *synth_vtables[BBXS_SYNTH_COUNT] = {
    // BBXS_SYNTH_CHIP
    &bbxs__synth_chip_vtable,
    // BBXS_SYNTH_FM
    &bbxs__synth_fm_vtable,
    // BBXS_SYNTH_NOISE
    &bbxs__synth_noise_vtable,
    // BBXS_SYNTH_PULSE_WIDTH
    &bbxs__synth_pwm_vtable,
    // BBXS_SYNTH_HARMONICS
    &bbxs__synth_harmonics_vtable,
    // BBXS_SYNTH_SPECTRUM
    &bbxs__synth_spectrum_vtable,
    // BBXS_SYNTH_PICKED_STRING
    &bbxs__synth_picked_vtable,
    // BBXS_SYNTH_SUPERSAW
    NULL,
    // BBXS_SYNTH_CUSTOM_CHIP
    &bbxs__synth_custom_chip_vtable,
};

static const effect_vtable_s *effect_vtables[BBXS_EFFECT_COUNT] = {
    // BBXS_EFFECT_PANNING,
    &bbxs__effect_panning_vtable,
    // BBXS_EFFECT_DISTORTION,
    &bbxs__effect_distortion_vtable,
    // BBXS_EFFECT_BITCRUSHER,
    &bbxs__effect_bitcrusher_vtable,
    // BBXS_EFFECT_CHORUS,
    &bbxs__effect_chorus_vtable,
    // BBXS_EFFECT_ECHO,
    &bbxs__effect_echo_vtable,
    // BBXS_EFFECT_REVERB,
    &bbxs__effect_reverb_vtable,
    // BBXS_EFFECT_EQ,
    &bbxs__effect_eq_vtable,
    // BBXS_EFFECT_VOLUME,
    &bbxs__effect_volume_vtable,
    // BBXS_EFFECT_LIMITER
    &bbxs__effect_limiter_vtable,
};

void bbxs_version(uint32_t *major, uint32_t *minor, uint32_t *revision) {
    *major = BBXS_VERSION_MAJOR;
    *minor = BBXS_VERSION_MINOR;
    *revision = BBXS_VERSION_REVISION;
}

#ifndef BBXS_NO_STD_ALLOC
static void *std_alloc(size_t size, void *ud) {
    (void)ud;
    return malloc(size);
}

static void std_free(void *ptr, void *ud) {
    (void)ud;
    free(ptr);
}
#endif

bbxs_context_s *bbxs_context_new(const bbxs_allocator_s *p_alloc,
                                 uint64_t wavetable_init_seed) {
    bbxs_context_s *ctx = NULL;
    bbxs_allocator_s alloc;

    if (p_alloc) {
        alloc = *p_alloc;
    } else {
        alloc = (bbxs_allocator_s){0};
    }

    // override both .alloc and .free with libc allocators if any of them are
    // null.
#ifndef BBXS_NO_STD_ALLOC
    if (!(alloc.alloc && alloc.free)) {
        alloc.alloc = std_alloc;
        alloc.free = std_free;
    }
#endif

    if (!alloc.alloc || !alloc.free) return NULL;

    ctx = alloc.alloc(sizeof(bbxs_context_s), alloc.userdata);
    if (!ctx) return NULL;

    ctx->alloc = alloc;

    if (!bbxs__init_wavetables_for_context(ctx, wavetable_init_seed)) {
        bbxs_context_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void bbxs_context_destroy(bbxs_context_s *ctx) {
    if (!ctx) return;

    for (int i = 0; i < BBXS_CHIP_WAVE_COUNT; ++i) {
        bbxs_free(ctx, ctx->wavetables.raw_chip_wavetables[i].samples);
        bbxs_free(ctx, ctx->wavetables.chip_wavetables[i].samples);
    }

    bbxs_free(ctx, ctx);
}

void bbxs_set_log_func(bbxs_context_s *ctx, bbxs_log_f log_func,
                       void *userdata) {
    ctx->log_func = log_func;
    ctx->log_userdata = userdata;
}

unsigned int bbxs_synth_param_count(bbxs_synth_type_e type) {
    const synth_vtable_s *vtable = synth_vtables[type];
    assert(vtable);

    if (!vtable) return 0;
    return BBXS_BASE_PARAM_COUNT + vtable->param_count;
}

const bbxs_param_info_s *bbxs_synth_param_info(bbxs_synth_type_e type,
                                               unsigned int index) {
    if (index < BBXS_BASE_PARAM_COUNT) return &bbxs__base_param_info[index];

    assert(synth_vtables[type]);
    assert(memcmp(
        synth_vtables[type]->param_info[index - BBXS_BASE_PARAM_COUNT].id,
        "\0\0\0\0\0\0\0\0", 8));
    assert(index - BBXS_BASE_PARAM_COUNT < synth_vtables[type]->param_count);

    return &synth_vtables[type]->param_info[index - BBXS_BASE_PARAM_COUNT];
}

uint32_t bbxs_synth_effect_toggle_param(bbxs_synthfx_type_e type) {
    switch (type) {
    case BBXS_SYNTHFX_TRANSITION_TYPE:
        return BBXS_PARAM_ENABLE_TRANSITION_TYPE;

    case BBXS_SYNTHFX_CHORD_TYPE:
        return BBXS_PARAM_ENABLE_CHORD_TYPE;

    case BBXS_SYNTHFX_PITCH_SHIFT:
        return BBXS_PARAM_ENABLE_PITCH_SHIFT;

    case BBXS_SYNTHFX_DETUNE:
        return BBXS_PARAM_ENABLE_DETUNE;

    case BBXS_SYNTHFX_VIBRATO:
        return BBXS_PARAM_ENABLE_VIBRATO;

    case BBXS_SYNTHFX_NOTE_FILTER:
        return BBXS_PARAM_ENABLE_NOTE_FILTER;

    default:
        return UINT32_MAX;
    }
}

bbxs_synth_s *bbxs_synth_new(bbxs_context_s *ctx, bbxs_synth_type_e type) {
    const synth_vtable_s *vtable = synth_vtables[type];
    assert(vtable);
    if (vtable == NULL) return NULL;

    // throw assertion error if any required fields don't exist
    assert(vtable->struct_size > 0);
    assert(vtable->inst_init);
    assert(vtable->inst_note_on);
    assert(vtable->inst_note_off);
    assert(vtable->inst_note_all_off);
    assert(vtable->inst_tick);
    assert(vtable->inst_run);
    assert(vtable->param_count == 0 || vtable->param_info);
    assert(vtable->param_count == 0 || vtable->param_addresses);
    assert(vtable->envelope_target_count == 0 || vtable->envelope_targets);

    bbxs_synth_s *inst = bbxs_malloc(ctx, vtable->struct_size);
    if (inst) vtable->inst_init(ctx, inst);
    return inst;
}

void bbxs_synth_destroy(bbxs_synth_s *inst) {
    if (inst) {
        const synth_vtable_s *vtable = synth_vtables[inst->type];
        assert(vtable);

        if (vtable->inst_destroy) {
            vtable->inst_destroy(inst);
        }

        bbxs_free(inst->ctx, inst);
    }
}

void bbxs_synth_set_sample_rate(bbxs_synth_s *inst, double sample_rate) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    double old_sr = inst->sample_rate;
    if (old_sr != sample_rate) {
        inst->sample_rate = sample_rate;
        if (vtable->inst_sample_rate_changed)
            vtable->inst_sample_rate_changed(inst, old_sr, sample_rate);
    }
}

bbxs_synth_type_e bbxs_synth_type(const bbxs_synth_s *inst) {
    return inst->type;
}

bbxs_synth_callbacks_s *bbxs_synth_get_callback_table(bbxs_synth_s *inst) {
    return &inst->callbacks;
}

void *bbxs_synth_get_userdata(bbxs_synth_s *inst) { return inst->userdata; }

void bbxs_synth_set_userdata(bbxs_synth_s *inst, void *userdata) {
    inst->userdata = userdata;
}

void bbxs_synth_begin_transport(bbxs_synth_s *inst, double beat, double bpm) {
    bbxs_vibrato_params_s vibrato_params = inst->vibrato;
    bbxs_vibrato_preset_params(inst->vibrato_preset, &vibrato_params);

    const double beats_per_sec = bpm / 60.0;
    const double parts_per_sec = PARTS_PER_BEAT * beats_per_sec;
    const double ticks_per_sec = TICKS_PER_PART * parts_per_sec;
    const double sec_per_tick = 1.0 / ticks_per_sec;

    const double time_secs = bpm / 60.0 * beat;
    inst->vibrato_time_start = time_secs * vibrato_params.speed;
    inst->vibrato_time_end = inst->vibrato_time_start + sec_per_tick;
    inst->arp_time = beat * bbxs__inst_calc_arp_speed(inst->arpeggio_speed);
}

static int synth_param_helper(const bbxs_synth_s *inst, uint32_t index,
                              void **addr, bbxs_param_info_s *info) {
    if (index < BBXS_BASE_PARAM_COUNT) {
        *info = bbxs__base_param_info[index];
        *addr = (void *)(((uint8_t *)inst) + bbxs__base_param_offsets[index]);
        return 0;
    }

    index -= BBXS_BASE_PARAM_COUNT;

    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    if (index >= vtable->param_count) return 1;
    *info = vtable->param_info[index];
    *addr = (void *)(((uint8_t *)inst) + vtable->param_addresses[index]);
    return 0;
}

static inline int set_param_int(void *addr, bbxs_param_info_s info, int value) {
    int val_clamped = value;
    if (info.min_value != info.max_value) {
        val_clamped =
            (int)clampd((double)value, info.min_value, info.max_value);
    }

    switch (info.type) {
    case BBXS_PARAM_UINT8:
        *((uint8_t *)addr) = (uint8_t)clampi(val_clamped, 0, UINT8_MAX);
        break;

    case BBXS_PARAM_INT:
        *((int *)addr) = clampi(val_clamped, INT_MIN, INT_MAX);
        break;

    case BBXS_PARAM_DOUBLE:
        *((double *)addr) = (double)val_clamped;
        break;
    }

    return 0;
}

static inline int set_param_double(void *addr, bbxs_param_info_s info,
                                   double value) {
    double val_clamped = value;
    if (info.min_value != info.max_value) {
        val_clamped = clampd(value, info.min_value, info.max_value);
    }

    switch (info.type) {
    case BBXS_PARAM_UINT8:
        *((uint8_t *)addr) = (uint8_t)clampd(val_clamped, 0, UINT8_MAX);
        break;

    case BBXS_PARAM_INT:
        *((int *)addr) = (int)clampd(val_clamped, INT_MIN, INT_MAX);
        break;

    case BBXS_PARAM_DOUBLE:
        *((double *)addr) = val_clamped;
        break;
    }

    return 0;
}

static inline int get_param_int(void *addr, bbxs_param_info_s info,
                                int *value) {
    switch (info.type) {
    case BBXS_PARAM_UINT8:
        *value = (int)*((uint8_t *)addr);
        break;

    case BBXS_PARAM_INT:
        *value = *((int *)addr);
        break;

    case BBXS_PARAM_DOUBLE:
        return 1;
    }

    return 0;
}

static inline int get_param_double(void *addr, bbxs_param_info_s info,
                                   double *value) {
    switch (info.type) {
    case BBXS_PARAM_UINT8:
        *value = (double)*((uint8_t *)addr);
        break;

    case BBXS_PARAM_INT:
        *value = (double)*((int *)addr);
        break;

    case BBXS_PARAM_DOUBLE:
        *value = *((double *)addr);
        break;
    }

    return 0;
}

#define PARAM_HELPER(helper, func, inst, index, value)                         \
    void *addr;                                                                \
    bbxs_param_info_s info;                                                    \
    if (helper(inst, index, &addr, &info)) return 1;                           \
    return func(addr, info, value);

int bbxs_synth_set_param_int(bbxs_synth_s *inst, uint32_t index, int value) {
    PARAM_HELPER(synth_param_helper, set_param_int, inst, index, value);
}

int bbxs_synth_set_param_double(bbxs_synth_s *inst, uint32_t index,
                                double value) {
    PARAM_HELPER(synth_param_helper, set_param_double, inst, index, value);
}

int bbxs_synth_get_param_int(const bbxs_synth_s *inst, uint32_t index,
                             int *value) {
    PARAM_HELPER(synth_param_helper, get_param_int, inst, index, value);
}

int bbxs_synth_get_param_double(const bbxs_synth_s *inst, uint32_t index,
                                double *value) {
    PARAM_HELPER(synth_param_helper, get_param_double, inst, index, value);
}

uint8_t bbxs_synth_envelope_count(const bbxs_synth_s *inst) {
    return inst->envelope_count;
}

bbxs_envelope_s *bbxs_synth_get_envelope(bbxs_synth_s *inst, uint8_t index) {
    // if (index >= inst->envelope_count) return NULL;
    return inst->envelopes + index;
}

bbxs_envelope_s *bbxs_synth_add_envelope(bbxs_synth_s *inst) {
    if (inst->envelope_count == BBXS_MAX_ENVELOPE_COUNT) return NULL;

    bbxs_envelope_s *new_env = &inst->envelopes[inst->envelope_count++];
    *new_env = (bbxs_envelope_s){
        .index = BBXS_ENV_INDEX_NONE,
    };

    return new_env;
}

void bbxs_synth_remove_envelope(bbxs_synth_s *inst, uint8_t index) {
    if (index >= BBXS_MAX_ENVELOPE_COUNT) return;
    if (inst->envelope_count == 0) return;

    for (uint8_t i = index; i < BBXS_MAX_ENVELOPE_COUNT - 1; i++) {
        inst->envelopes[i] = inst->envelopes[i + 1];
    }

    inst->envelope_count--;
}

void bbxs_synth_clear_envelopes(bbxs_synth_s *inst) {
    inst->envelope_count = 0;
}

size_t bbxs_synth_get_exstate_size(bbxs_synth_type_e type) {
    const synth_vtable_s *vtable = synth_vtables[type];
    assert(vtable);
    if (vtable == NULL) return 0;

    return vtable->exstate_size;
}

size_t bbxs_synth_get_exstate(bbxs_synth_s *inst, void *buffer) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    if (!vtable->inst_get_exstate) return 0;
    if (buffer) vtable->inst_get_exstate(inst, buffer);

    return vtable->exstate_size;
}

void bbxs_synth_set_exstate(bbxs_synth_s *inst, const void *buffer) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    if (vtable->inst_set_exstate && buffer)
        vtable->inst_set_exstate(inst, buffer);
}

bbxs_serialize_status_e bbxs_synth_serialize_exstate(bbxs_synth_s *inst,
                                                     bbxs_writer_f writer,
                                                     void *userdata) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    if (vtable->inst_serialize_exstate)
        return vtable->inst_serialize_exstate(inst, writer, userdata);

    return BBXS_SERIALIZE_STATUS_NO_DATA;
}

bbxs_serialize_status_e bbxs_synth_deserialize_exstate(bbxs_synth_s *inst,
                                                       bbxs_reader_f reader,
                                                       void *userdata) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    if (vtable->inst_deserialize_exstate)
        return vtable->inst_deserialize_exstate(inst, reader, userdata);

    return BBXS_SERIALIZE_STATUS_NO_DATA;
}

bbxs_voice_id bbxs_synth_begin_note(bbxs_synth_s *inst, int key,
                                    double velocity, int32_t length) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    return vtable->inst_note_on(inst, key, velocity, length);
}

void bbxs_synth_end_note(bbxs_synth_s *inst, bbxs_voice_id id) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    vtable->inst_note_off(inst, id);
}

void bbxs_synth_end_all_notes(bbxs_synth_s *inst) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);

    vtable->inst_note_all_off(inst);
}

void bbxs_synth_stop(bbxs_synth_s *inst) { (void)inst; }

void bbxs_synth_tick(bbxs_synth_s *inst, const bbxs_tick_ctx_s *tick_ctx) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);
    vtable->inst_tick(inst, tick_ctx);
}

void bbxs_synth_run(bbxs_synth_s *inst, float *out_samples,
                    size_t frame_count) {
    const synth_vtable_s *vtable = synth_vtables[inst->type];
    assert(vtable);
    vtable->inst_run(inst, out_samples, frame_count);
}

double bbxs_calc_samples_per_tick(double bpm, double sample_rate) {
    return bbxs__calc_samples_per_tick(bpm, sample_rate);
}

double bbxs_ticks_fade_out(double setting) {
    return bbxs__ticks_fade_out(setting);
}

const char *env_index_names[] = {
    "none",
    "note volume",      // BBXS_ENV_INDEX_NOTE_VOLUME
    "n. filter freqs",  // BBXS_ENV_INDEX_NOTE_FILTER_ALL_FREQS
    "pulse width",      // BBXS_ENV_INDEX_PULSE_WIDTH
    "sustain",          // BBXS_ENV_INDEX_STRING_SUSTAIN
    "unison",           // BBXS_ENV_INDEX_UNISON
    "fm1 freq",         // BBXS_ENV_INDEX_OPERATOR_FREQ0
    "fm2 freq",         // BBXS_ENV_INDEX_OPERATOR_FREQ1
    "fm3 freq",         // BBXS_ENV_INDEX_OPERATOR_FREQ2
    "fm4 freq",         // BBXS_ENV_INDEX_OPERATOR_FREQ3
    "fm1 volume",       // BBXS_ENV_INDEX_OPERATOR_AMP0
    "fm2 volume",       // BBXS_ENV_INDEX_OPERATOR_AMP1
    "fm3 volume",       // BBXS_ENV_INDEX_OPERATOR_AMP2
    "fm4 volume",       // BBXS_ENV_INDEX_OPERATOR_AMP3
    "fm feedback",      // BBXS_ENV_INDEX_FEEDBACK_AMP
    "pitch shift",      // BBXS_ENV_INDEX_PITCH_SHIFT
    "detune",           // BBXS_ENV_INDEX_DETUNE
    "vibrato range",    // BBXS_ENV_INDEX_VIBRATO_DEPTH
    "n. filter 1 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ0
    "n. filter 2 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ1
    "n. filter 3 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ2
    "n. filter 4 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ3
    "n. filter 5 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ4
    "n. filter 6 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ5
    "n. filter 7 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ6
    "n. filter 8 freq", // BBXS_ENV_INDEX_NOTE_FILTER_FREQ7
    "n. filter 1 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN0
    "n. filter 2 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN1
    "n. filter 3 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN2
    "n. filter 4 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN3
    "n. filter 5 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN4
    "n. filter 6 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN5
    "n. filter 7 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN6
    "n. filter 8 vol",  // BBXS_ENV_INDEX_NOTE_FILTER_GAIN7
    "dynamism",         // BBXS_ENV_INDEX_SUPERSAW_DYNAMISM
    "spread",           // BBXS_ENV_INDEX_SUPERSAW_SPREAD
    "saw↔pulse",        // BBXS_ENV_INDEX_SUPERSAW_SHAPE
};

const char *bbxs_envelope_index_name(bbxs_envelope_compute_index_e index) {
    if (index < 0 ||
        index >= sizeof(env_index_names) / sizeof(*env_index_names))
        return NULL;

    return env_index_names[index];
}

const char **bbxs_envelope_curve_preset_names(void) {
    return bbxs__envelope_curve_preset_names;
}

const bbxs_envelope_compute_index_e *
bbxs_synth_envelope_targets(bbxs_synth_type_e type, int *size) {
    const synth_vtable_s *vtable = synth_vtables[type];
    assert(vtable);

    *size = (int)vtable->envelope_target_count;
    return vtable->envelope_targets;
}

unsigned int bbxs_effect_param_count(bbxs_effect_type_e type) {
    const effect_vtable_s *vtable = effect_vtables[type];
    assert(vtable);

    return vtable->param_count;
}

const bbxs_param_info_s *bbxs_effect_param_info(bbxs_effect_type_e type,
                                                unsigned int index) {
    assert(effect_vtables[type]);
    assert(memcmp(effect_vtables[type]->param_info[index].id,
                  "\0\0\0\0\0\0\0\0", 8));
    assert(index < effect_vtables[type]->param_count);

    return &effect_vtables[type]->param_info[index];
}

bbxs_effect_s *bbxs_effect_new(bbxs_context_s *ctx,
                               bbxs_effect_type_e effect_type) {
    const effect_vtable_s *vtable = effect_vtables[effect_type];
    assert(vtable);
    if (vtable == NULL) return NULL;

    // throw assertion error if any required fields don't exist
    assert(vtable->struct_size > 0);
    assert(vtable->effect_init);
    assert(vtable->effect_tick);
    assert(vtable->effect_run);
    assert(vtable->input_channel_count != 0);
    assert(vtable->output_channel_count != 0);
    assert(vtable->param_count == 0 || vtable->param_info);
    assert(vtable->param_count == 0 || vtable->param_addresses);

    bbxs_effect_s *inst = bbxs_malloc(ctx, vtable->struct_size);
    if (inst) vtable->effect_init(ctx, inst);
    return inst;
}

void bbxs_effect_destroy(bbxs_effect_s *effect) {
    if (!effect) return;

    const effect_vtable_s *vtable = effect_vtables[effect->type];
    assert(vtable);
    if (vtable->effect_destroy) vtable->effect_destroy(effect);

    bbxs_free(effect->ctx, effect);
}

bbxs_effect_type_e bbxs_effect_type(const bbxs_effect_s *effect) {
    return effect->type;
}

void *bbxs_effect_get_userdata(bbxs_effect_s *effect) {
    return effect->userdata;
}

void bbxs_effect_set_userdata(bbxs_effect_s *effect, void *userdata) {
    effect->userdata = userdata;
}

void bbxs_effect_channel_info(bbxs_effect_type_e type, int *input,
                              int *output) {
    const effect_vtable_s *vtable = effect_vtables[type];
    assert(vtable);

    *input = vtable->input_channel_count;
    *output = vtable->output_channel_count;
}

void bbxs_effect_set_sample_rate(bbxs_effect_s *effect, double sample_rate) {
    const effect_vtable_s *vtable = effect_vtables[effect->type];
    assert(vtable);

    double old_sr = effect->sample_rate;
    if (old_sr != sample_rate) {
        effect->sample_rate = sample_rate;
        if (vtable->effect_sample_rate_changed)
            vtable->effect_sample_rate_changed(effect, old_sr, sample_rate);
    }
}

// void bbxs_effect_begin_transport(bbxs_effect_s *effect,
//                                                 double beat, double bpm);

static int effect_param_helper(const bbxs_effect_s *inst, uint32_t index,
                               void **addr, bbxs_param_info_s *info) {
    const effect_vtable_s *vtable = effect_vtables[inst->type];
    assert(vtable);

    if (index >= vtable->param_count) return 1;
    *info = vtable->param_info[index];
    *addr = (void *)(((uint8_t *)inst) + vtable->param_addresses[index]);
    return 0;
}

int bbxs_effect_set_param_int(bbxs_effect_s *inst, uint32_t param, int value) {
    PARAM_HELPER(effect_param_helper, set_param_int, inst, param, value);
}

int bbxs_effect_set_param_double(bbxs_effect_s *inst, uint32_t param,
                                 double value) {
    PARAM_HELPER(effect_param_helper, set_param_double, inst, param, value);
}

int bbxs_effect_get_param_int(const bbxs_effect_s *inst, uint32_t param,
                              int *value) {
    PARAM_HELPER(effect_param_helper, get_param_int, inst, param, value);
}

/**
 * @brief Get the value of an effect instance's parameter as a floating-point
 * value.
 *
 * Gets the value of an effect instance's parameter as a double-
 * floating point value.
 *
 * @param      effect Pointer to the effect instance.
 * @param      param  The BBXS_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param[out] value  The output value.
 * @return 0 on success, and 1 on failure.
 */
int bbxs_effect_get_param_double(const bbxs_effect_s *inst, uint32_t param,
                                 double *value) {
    PARAM_HELPER(effect_param_helper, get_param_double, inst, param, value);
}

void bbxs_effect_stop(bbxs_effect_s *inst) {
    const effect_vtable_s *vtable = effect_vtables[inst->type];
    assert(vtable);

    if (vtable->effect_stop) vtable->effect_stop(inst);
}

/**
 * @brief Tick an effect instance.
 *
 * While computing audio, BeepBox effects need to be ticked at a rate dependent
 * on the tempo of the song. Values will be calculated on each tick and
 * interpolated along the individual frames of an audio render.
 *
 * To get the tick rate, use the bbxs_calc_samples_per_tick function.
 *
 * @param inst     Pointer to the instrument.
 * @param tick_ctx Pointer to a bbxs_tick_ctx structure.
 */
void bbxs_effect_tick(bbxs_effect_s *effect, const bbxs_tick_ctx_s *tick_ctx) {
    const effect_vtable_s *vtable = effect_vtables[effect->type];
    assert(vtable);

    vtable->effect_tick(effect, tick_ctx);
}

void bbxs_effect_run(bbxs_effect_s *effect, float **buffer,
                     size_t frame_count) {
    const effect_vtable_s *vtable = effect_vtables[effect->type];
    assert(vtable);

    vtable->effect_run(effect, buffer, frame_count);
}

void bbxs_vibrato_preset_params(bbxs_vibrato_preset_e preset,
                                bbxs_vibrato_params_s *params) {
    if (preset == BBXS_VIBRATO_PRESET_CUSTOM) return;

    params->speed = 1.0;

    switch (preset) {
    case BBXS_VIBRATO_PRESET_NONE:
        params->depth = 0.0;
        params->delay = 0;
        params->type = (uint8_t)BBXS_VIBRATO_TYPE_NORMAL;
        break;

    case BBXS_VIBRATO_PRESET_LIGHT:
        params->depth = 0.15;
        params->delay = 0;
        params->type = (uint8_t)BBXS_VIBRATO_TYPE_NORMAL;
        break;

    case BBXS_VIBRATO_PRESET_DELAYED:
        params->depth = 0.3;
        params->delay = 19;
        params->type = (uint8_t)BBXS_VIBRATO_TYPE_NORMAL;
        break;

    case BBXS_VIBRATO_PRESET_HEAVY:
        params->depth = 0.45;
        params->delay = 0;
        params->type = (uint8_t)BBXS_VIBRATO_TYPE_NORMAL;
        break;

    case BBXS_VIBRATO_PRESET_SHAKY:
        params->depth = 0.1;
        params->delay = 0;
        params->type = (uint8_t)BBXS_VIBRATO_TYPE_SHAKY;
        break;

    default:
        break;
    }
}

double bbxs_freq_setting_to_hz(double freq_setting) {
    return bbxs__get_hz_from_setting_value(freq_setting);
}

double bbxs_linear_gain_to_setting(double gain) {
    return log2(gain) / FILTER_GAIN_STEP + BBXS_FILTER_GAIN_CENTER;
}

void bbxs_analyze_freq_response(bbxs_filter_type_e filter_type,
                                double freq_setting, double gain_setting,
                                double hz, double sample_rate,
                                bbxs_complex_s *out) {
    filter_group_s temp_group;
    temp_group.type[0] = filter_type;
    temp_group.freq_idx[0] = freq_setting;
    temp_group.gain_idx[0] = gain_setting;

    filter_coefs_s coefs =
        bbxs__filter_to_coefficients(&temp_group, 0, sample_rate, 1.0, 1.0);
    double corner_rads_per_sample = PI2 * hz / sample_rate;
    *out = bbxs__filter_analyze(coefs, corner_rads_per_sample);
}

const char *const *bbxs_get_chip_waveform_names(void) {
    return bbxs__chip_waveform_names;
}

const char *const *bbxs_get_noise_waveform_names(void) {
    return bbxs__noise_waveform_names;
}

void bbxs_get_chip_waveform(bbxs_context_s *ctx, int index, int integrated,
                            bbxs_waveform_data_s *data) {
    wavetable_desc_s *wavetable_list;
    if (integrated)
        wavetable_list = ctx->wavetables.chip_wavetables;
    else
        wavetable_list = ctx->wavetables.raw_chip_wavetables;

    wavetable_desc_s *desc = &wavetable_list[index];
    data->expression = desc->expression;
    data->samples = desc->samples;
    data->length = desc->length;
}

void bbxs_get_noise_waveform(bbxs_context_s *ctx, int index,
                             bbxs_waveform_data_s *data) {
    noise_wavetable_s *desc = &ctx->wavetables.noise_wavetables[index];
    data->expression = desc->expression;
    data->samples = desc->samples;
    data->length = NOISE_WAVETABLE_LENGTH;
}