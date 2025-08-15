#include "../include/beepbox_synth.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "util.h"
#include "inst/instrument.h"
#include "envelope.h"
#include "filtering.h"

#include "inst/wave.h"
#include "inst/fm.h"

static const inst_vtable_s *inst_vtables[] = {
    // BPBXSYN_SYNTH_CHIP
    &inst_chip_vtable,
    // BPBXSYN_SYNTH_FM
    &inst_fm_vtable,
    // BPBXSYN_SYNTH_NOISE
    NULL,
    // BPBXSYN_SYNTH_PULSE_WIDTH
    NULL,
    // BPBXSYN_SYNTH_HARMONICS
    &inst_harmonics_vtable,
    // BPBXSYN_SYNTH_SPECTRUM
    NULL,
    // BPBXSYN_SYNTH_PICKED_STRING
    NULL,
    // BPBXSYN_SYNTH_SUPERSAW
    NULL
};


void bpbxsyn_version(uint32_t *major, uint32_t *minor, uint32_t *revision) {
    *major = BPBXSYN_VERSION_MAJOR;
    *minor = BPBXSYN_VERSION_MINOR;
    *revision = BPBXSYN_VERSION_REVISION;
}

static void* default_alloc(size_t size, void *userdata) {
    (void)userdata;
    return malloc(size);
}

static void default_free(void *ptr, void *userdata) {
    (void)userdata;
    free(ptr);
}

static bpbxsyn_malloc_f alloc_new = default_alloc;
static bpbxsyn_mfree_f alloc_free = default_free;
static void *alloc_userdata = NULL;


void bpbxsyn_set_allocator(bpbxsyn_malloc_f alloc, bpbxsyn_mfree_f free, void *userdata) {
    assert(alloc);
    assert(free);

    alloc_new = alloc;
    alloc_free = free;
    alloc_userdata = userdata;
}


unsigned int bpbxsyn_synth_param_count(bpbxsyn_synth_type_e type) {
    const inst_vtable_s *vtable = inst_vtables[type];
    assert(vtable);

    if (!vtable) return 0;
    return BPBXSYN_BASE_PARAM_COUNT + vtable->param_count;
}


const bpbxsyn_param_info_s* bpbxsyn_synth_param_info(bpbxsyn_synth_type_e type, unsigned int index) {
    if (index < BPBXSYN_BASE_PARAM_COUNT)
        return &base_param_info[index];

    assert(inst_vtables[type]);
    assert(memcmp(inst_vtables[type]->param_info[index - BPBXSYN_BASE_PARAM_COUNT].id, "\0\0\0\0\0\0\0\0", 8));
    assert(index - BPBXSYN_BASE_PARAM_COUNT < inst_vtables[type]->param_count);

    return &inst_vtables[type]->param_info[index - BPBXSYN_BASE_PARAM_COUNT];
}


uint32_t bpbxsyn_synth_effect_toggle_param(bpbxsyn_synthfx_type_e type) {
    switch (type) {
        case BPBXSYN_SYNTHFX_TRANSITION_TYPE:
            return BPBXSYN_PARAM_ENABLE_TRANSITION_TYPE;

        case BPBXSYN_SYNTHFX_CHORD_TYPE:
            return BPBXSYN_PARAM_ENABLE_CHORD_TYPE;

        case BPBXSYN_SYNTHFX_PITCH_SHIFT:
            return BPBXSYN_PARAM_ENABLE_PITCH_SHIFT;

        case BPBXSYN_SYNTHFX_DETUNE:
            return BPBXSYN_PARAM_ENABLE_DETUNE;

        case BPBXSYN_SYNTHFX_VIBRATO:
            return BPBXSYN_PARAM_ENABLE_VIBRATO;

        case BPBXSYN_SYNTHFX_NOTE_FILTER:
            return BPBXSYN_PARAM_ENABLE_NOTE_FILTER;

        default:
            return UINT32_MAX;
    }
}


bpbxsyn_synth_s* bpbxsyn_synth_new(bpbxsyn_synth_type_e type) {
    init_wavetables();

    const inst_vtable_s *vtable = inst_vtables[type];
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
    assert(vtable->param_count > 0 && vtable->param_info);
    assert(vtable->param_count > 0 && vtable->param_addresses);
    assert(vtable->envelope_target_count && vtable->envelope_targets);

    bpbxsyn_synth_s *inst = alloc_new(vtable->struct_size, alloc_userdata);
    if (inst)
        vtable->inst_init(inst);
    return inst;
}


void bpbxsyn_synth_destroy(bpbxsyn_synth_s *inst) {
    if (inst) {
        const inst_vtable_s *vtable = inst_vtables[inst->type];
        assert(vtable);

        if (vtable->inst_destroy) {
            vtable->inst_destroy(inst);
        }

        alloc_free(inst, alloc_userdata);
    }
}


void bpbxsyn_synth_set_sample_rate(bpbxsyn_synth_s *inst, double sample_rate) {
    inst->sample_rate = sample_rate;
}


bpbxsyn_synth_type_e bpbxsyn_synth_type(const bpbxsyn_synth_s *inst) {
    return inst->type;
}


bpbxsyn_synth_callbacks_s* bpbxsyn_synth_get_callback_table(bpbxsyn_synth_s *inst) {
    return &inst->callbacks;
}


void* bpbxsyn_synth_get_userdata(bpbxsyn_synth_s *inst) {
    return inst->userdata;
}


void bpbxsyn_synth_set_userdata(bpbxsyn_synth_s *inst, void *userdata) {
    inst->userdata = userdata;
}


void bpbxsyn_synth_begin_transport(bpbxsyn_synth_s *inst, double beat, double bpm) {
    bpbxsyn_vibrato_params_s vibrato_params = inst->vibrato;
    bpbxsyn_vibrato_preset_params(inst->vibrato_preset, &vibrato_params);

    const double beats_per_sec = bpm / 60.0;
    const double parts_per_sec = PARTS_PER_BEAT * beats_per_sec;
    const double ticks_per_sec = TICKS_PER_PART * parts_per_sec;
    const double sec_per_tick = 1.0 / ticks_per_sec;

    const double time_secs = bpm / 60.0 * beat;
    inst->vibrato_time_start = time_secs * vibrato_params.speed;
    inst->vibrato_time_end = inst->vibrato_time_start + sec_per_tick;
    inst->arp_time = beat * inst_calc_arp_speed(inst->arpeggio_speed);
}

static int param_helper(const bpbxsyn_synth_s *inst, uint32_t index, void **addr, bpbxsyn_param_info_s *info) {
    if (index < BPBXSYN_BASE_PARAM_COUNT) {
        *info = base_param_info[index];
        *addr = (void*)(((uint8_t*)inst) + base_param_offsets[index]);
        return 0;
    }

    index -= BPBXSYN_BASE_PARAM_COUNT;
    
    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);

    if (index >= vtable->param_count) return 1;
    *info = vtable->param_info[index];
    *addr = (void*)(((uint8_t*)inst) + vtable->param_addresses[index]);
    return 0;
}


int bpbxsyn_synth_set_param_int(bpbxsyn_synth_s* inst, uint32_t index, int value) {
    void *addr;
    bpbxsyn_param_info_s info;

    if (param_helper(inst, index, &addr, &info))
        return 1;

    int val_clamped = value;
    if (info.min_value != info.max_value) {
        val_clamped = (int)clampd((double)value, info.min_value, info.max_value);
    }
    
    switch (info.type) {
        case BPBXSYN_PARAM_UINT8:
            *((uint8_t*)addr) = clampi(val_clamped, 0, UINT8_MAX);
            break;

        case BPBXSYN_PARAM_INT:
            *((int*)addr) = clampi(val_clamped, INT_MIN, INT_MAX);
            break;

        case BPBXSYN_PARAM_DOUBLE:
            *((double*)addr) = (double)val_clamped;
            break;
    }

    return 0;
}


int bpbxsyn_synth_set_param_double(bpbxsyn_synth_s* inst, uint32_t index, double value) {
    void *addr;
    bpbxsyn_param_info_s info;

    if (param_helper(inst, index, &addr, &info))
        return 1;

    double val_clamped = value;
    if (info.min_value != info.max_value) {
        val_clamped = clampd(value, info.min_value, info.max_value);
    }
    
    switch (info.type) {
        case BPBXSYN_PARAM_UINT8:
        case BPBXSYN_PARAM_INT:
            return 1;

        case BPBXSYN_PARAM_DOUBLE:
            *((double*)addr) = val_clamped;
            break;
    }

    return 0;
}


int bpbxsyn_synth_get_param_int(const bpbxsyn_synth_s* inst, uint32_t index, int *value) {
    void *addr;
    bpbxsyn_param_info_s info;

    if (param_helper(inst, index, &addr, &info))
        return 1;

    switch (info.type) {
        case BPBXSYN_PARAM_UINT8:
            *value = (int) *((uint8_t*)addr);
            break;

        case BPBXSYN_PARAM_INT:
            *value = *((int*)addr);
            break;

        case BPBXSYN_PARAM_DOUBLE:
            return 1;
    }

    return 0;
}


int bpbxsyn_synth_get_param_double(const bpbxsyn_synth_s* inst, uint32_t index, double *value) {
    void *addr;
    bpbxsyn_param_info_s info;

    if (param_helper(inst, index, &addr, &info))
        return 1;

    switch (info.type) {
        case BPBXSYN_PARAM_UINT8:
            *value = (double) *((uint8_t*)addr);
            break;

        case BPBXSYN_PARAM_INT:
            *value = (double) *((int*)addr);
            break;

        case BPBXSYN_PARAM_DOUBLE:
            *value = *((double*)addr);
            break;
    }

    return 0;
}


uint8_t bpbxsyn_synth_envelope_count(const bpbxsyn_synth_s *inst) {
    return inst->envelope_count;
}


bpbxsyn_envelope_s* bpbxsyn_synth_get_envelope(bpbxsyn_synth_s *inst, uint8_t index) {
    // if (index >= inst->envelope_count) return NULL; 
    return inst->envelopes + index;
}


bpbxsyn_envelope_s* bpbxsyn_synth_add_envelope(bpbxsyn_synth_s *inst) {
    if (inst->envelope_count == BPBXSYN_MAX_ENVELOPE_COUNT)
        return NULL;

    bpbxsyn_envelope_s *new_env = &inst->envelopes[inst->envelope_count++];
    *new_env = (bpbxsyn_envelope_s) {
        .index = BPBXSYN_ENV_INDEX_NONE,
    };

    return new_env;
}


void bpbxsyn_synth_remove_envelope(bpbxsyn_synth_s *inst, uint8_t index) {
    if (index >= BPBXSYN_MAX_ENVELOPE_COUNT) return;
    if (inst->envelope_count == 0) return;

    for (uint8_t i = index; i < BPBXSYN_MAX_ENVELOPE_COUNT - 1; i++) {
        inst->envelopes[i] = inst->envelopes[i+1];
    }

    inst->envelope_count--;
}


void bpbxsyn_synth_clear_envelopes(bpbxsyn_synth_s *inst) {
    inst->envelope_count = 0;
}


bpbxsyn_voice_id bpbxsyn_synth_begin_note(bpbxsyn_synth_s *inst, int key, double velocity, int32_t length) {
    // TODO: implement length
    (void)length;

    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);

    return vtable->inst_note_on(inst, key, velocity, length);
}


void bpbxsyn_synth_end_note(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id) {
    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);

    vtable->inst_note_off(inst, id);
}


void bpbxsyn_synth_end_all_notes(bpbxsyn_synth_s *inst) {
    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);

    vtable->inst_note_all_off(inst);
}


void bpbxsyn_synth_tick(bpbxsyn_synth_s *inst, const bpbxsyn_tick_ctx_s *tick_ctx) {
    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);
    vtable->inst_tick(inst, tick_ctx);
}


void bpbxsyn_synth_run(bpbxsyn_synth_s* inst, float *out_samples, size_t frame_count) {
    const inst_vtable_s *vtable = inst_vtables[inst->type];
    assert(vtable);
    vtable->inst_run(inst, out_samples, frame_count);
}


double bpbxsyn_calc_samples_per_tick(double bpm, double sample_rate) {
    return calc_samples_per_tick(bpm, sample_rate);
}


double bpbxsyn_ticks_fade_out(double setting) {
    return ticks_fade_out(setting);
}

const char *env_index_names[] = {
    "none",
    "note volume", // BPBXSYN_ENV_INDEX_NOTE_VOLUME
    "n. filter freqs", // BPBXSYN_ENV_INDEX_NOTE_FILTER_ALL_FREQS
    "pulse width", // BPBXSYN_ENV_INDEX_PULSE_WIDTH
    "sustain", // BPBXSYN_ENV_INDEX_STRING_SUSTAIN
    "unison", // BPBXSYN_ENV_INDEX_UNISON
    "fm1 freq", // BPBXSYN_ENV_INDEX_OPERATOR_FREQ0
    "fm2 freq", // BPBXSYN_ENV_INDEX_OPERATOR_FREQ1
    "fm3 freq", // BPBXSYN_ENV_INDEX_OPERATOR_FREQ2
    "fm4 freq", // BPBXSYN_ENV_INDEX_OPERATOR_FREQ3
    "fm1 volume", // BPBXSYN_ENV_INDEX_OPERATOR_AMP0
    "fm2 volume", // BPBXSYN_ENV_INDEX_OPERATOR_AMP1
    "fm3 volume", // BPBXSYN_ENV_INDEX_OPERATOR_AMP2
    "fm4 volume", // BPBXSYN_ENV_INDEX_OPERATOR_AMP3
    "fm feedback", // BPBXSYN_ENV_INDEX_FEEDBACK_AMP
    "pitch shift", // BPBXSYN_ENV_INDEX_PITCH_SHIFT
    "detune", // BPBXSYN_ENV_INDEX_DETUNE
    "vibrato range", // BPBXSYN_ENV_INDEX_VIBRATO_DEPTH
    "n. filter 1 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ0
    "n. filter 2 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ1
    "n. filter 3 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ2
    "n. filter 4 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ3
    "n. filter 5 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ4
    "n. filter 6 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ5
    "n. filter 7 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ6
    "n. filter 8 freq", // BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ7
    "n. filter 1 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN0
    "n. filter 2 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN1
    "n. filter 3 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN2
    "n. filter 4 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN3
    "n. filter 5 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN4
    "n. filter 6 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN5
    "n. filter 7 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN6
    "n. filter 8 vol", // BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN7
    "dynamism", // BPBXSYN_ENV_INDEX_SUPERSAW_DYNAMISM
    "spread", // BPBXSYN_ENV_INDEX_SUPERSAW_SPREAD
    "saw↔pulse", // BPBXSYN_ENV_INDEX_SUPERSAW_SHAPE
};


const char* bpbxsyn_envelope_index_name(bpbxsyn_envelope_compute_index_e index) {
    if (index < 0 || index >= sizeof(env_index_names)/sizeof(*env_index_names))
        return NULL;

    return env_index_names[index];
}


const char** bpbxsyn_envelope_curve_preset_names(void) {
    static int need_init = 1;
    static const char* env_curve_names[BPBXSYN_ENVELOPE_CURVE_PRESET_COUNT + 1];

    if (need_init) {
        need_init = 0;
        for (int i = 0; i < BPBXSYN_ENVELOPE_CURVE_PRESET_COUNT; i++) {
            env_curve_names[i] = envelope_curve_presets[i].name;
        }

        env_curve_names[BPBXSYN_ENVELOPE_CURVE_PRESET_COUNT] = NULL;
    }

    return env_curve_names;
}


const bpbxsyn_envelope_compute_index_e* bpbxsyn_synth_envelope_targets(bpbxsyn_synth_type_e type, int *size) {
    const inst_vtable_s *vtable = inst_vtables[type];
    assert(vtable);

    *size = vtable->envelope_target_count;
    return vtable->envelope_targets;
}


void bpbxsyn_vibrato_preset_params(bpbxsyn_vibrato_preset_e preset, bpbxsyn_vibrato_params_s *params) {
    if (preset == BPBXSYN_VIBRATO_PRESET_CUSTOM) return;

    params->speed = 1.0;

    switch (preset) {
        case BPBXSYN_VIBRATO_PRESET_NONE:
            params->depth = 0.0;
            params->delay = 0;
            params->type = (uint8_t) BPBXSYN_VIBRATO_TYPE_NORMAL;
            break;
        
        case BPBXSYN_VIBRATO_PRESET_LIGHT:
            params->depth = 0.15;
            params->delay = 0;
            params->type = (uint8_t) BPBXSYN_VIBRATO_TYPE_NORMAL;
            break;

        case BPBXSYN_VIBRATO_PRESET_DELAYED:
            params->depth = 0.3;
            params->delay = 19;
            params->type = (uint8_t) BPBXSYN_VIBRATO_TYPE_NORMAL;
            break;

        case BPBXSYN_VIBRATO_PRESET_HEAVY:
            params->depth = 0.45;
            params->delay = 0;
            params->type = (uint8_t) BPBXSYN_VIBRATO_TYPE_NORMAL;
            break;

        case BPBXSYN_VIBRATO_PRESET_SHAKY:
            params->depth = 0.1;
            params->delay = 0;
            params->type = (uint8_t) BPBXSYN_VIBRATO_TYPE_SHAKY;
            break;

        default:
            break;
    }
}


double bpbxsyn_freq_setting_to_hz(double freq_setting) {
    return get_hz_from_setting_value(freq_setting);
}


double bpbxsyn_linear_gain_to_setting(double gain) {
    return log2(gain) / FILTER_GAIN_STEP + BPBXSYN_FILTER_GAIN_CENTER;
}


void bpbxsyn_analyze_freq_response(
    bpbxsyn_filter_type_e filter_type, double freq_setting, double gain_setting,
    double hz, double sample_rate, bpbxsyn_complex_s *out)
{
    filter_group_s temp_group;
    temp_group.type[0] = filter_type;
    temp_group.freq_idx[0] = freq_setting;
    temp_group.gain_idx[0] = gain_setting;

    filter_coefs_s coefs = filter_to_coefficients(&temp_group, 0, sample_rate, 1.0, 1.0);
    double corner_rads_per_sample = PI2 * hz / sample_rate;
    *out = filter_analyze(coefs, corner_rads_per_sample);
}