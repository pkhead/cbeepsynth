/*
Copyright 2025 pkhead

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _beepbox_synth_h_
#define _beepbox_synth_h_

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
namespace beepbox {
extern "C" {
#endif

#ifdef _WIN32
#   if defined(BPBX_SHARED) && defined(BPBX_SHARED_IMPORT)
#       define BEEPBOX_API __declspec(dllimport)
#   elif defined(BPBX_SHARED)
#       define BEEPBOX_API __declspec(dllexport)
#   else
#       define BEEPBOX_API
#   endif
#else
#   ifdef BPBX_SHARED
#       define BEEPBOX_API __attribute__((visibility("default")))
#   else
#       define BEEPBOX_API
#   endif
#endif

#include "beepbox_instrument_data.h"

#define BPBX_VERSION_MAJOR 0
#define BPBX_VERSION_MINOR 1
#define BPBX_VERSION_REVISION 0

#define BPBX_MAX_ENVELOPE_COUNT 12 // 16 in slarmoo's box
#define BPBX_ENVELOPE_CURVE_PRESET_COUNT 26
#define BPBX_FILTER_GROUP_COUNT 8
#define BPBX_FILTER_GAIN_CENTER 7
#define BPBX_FILTER_FREQ_REFERENCE_SETTING 28
#define BPBX_FILTER_FREQ_RANGE 34
#define BPBX_FILTER_GAIN_RANGE 15
#define BPBX_FILTER_FREQ_MAX ((BPBX_FILTER_FREQ_RANGE - 1))
#define BPBX_FILTER_GAIN_MAX ((BPBX_FILTER_GAIN_RANGE - 1))

typedef enum {
    BPBX_INSTRUMENT_CHIP,
    BPBX_INSTRUMENT_FM,
    BPBX_INSTRUMENT_NOISE,
    BPBX_INSTRUMENT_PULSE_WIDTH,
    BPBX_INSTRUMENT_HARMONICS,
    BPBX_INSTRUMENT_SPECTRUM,
    BPBX_INSTRUMENT_PICKED_STRING,
    BPBX_INSTRUMENT_SUPERSAW,
} bpbx_inst_type_e;

typedef enum {
    BPBX_INSTFX_TRANSITION_TYPE,
    BPBX_INSTFX_CHORD_TYPE,
    BPBX_INSTFX_PITCH_SHIFT,
    BPBX_INSTFX_DETUNE,
    BPBX_INSTFX_VIBRATO,
    BPBX_INSTFX_NOTE_FILTER
} bpbx_instfx_type_e;

typedef enum {
    BPBX_TRANSITION_TYPE_NORMAL,
    BPBX_TRANSITION_TYPE_INTERRUPT,
    BPBX_TRANSITION_TYPE_CONTINUE,
    BPBX_TRANSITION_TYPE_SLIDE
} bpbx_transition_type_e;

typedef enum {
    BPBX_CHORD_TYPE_SIMULTANEOUS,
    BPBX_CHORD_TYPE_STRUM,
    BPBX_CHORD_TYPE_ARPEGGIO,
    BPBX_CHORD_TYPE_CUSTOM_INTERVAL
} bpbx_chord_type_e;

typedef enum {
    BPBX_VIBRATO_PRESET_NONE,
    BPBX_VIBRATO_PRESET_LIGHT,
    BPBX_VIBRATO_PRESET_DELAYED,
    BPBX_VIBRATO_PRESET_HEAVY,
    BPBX_VIBRATO_PRESET_SHAKY,
    BPBX_VIBRATO_PRESET_CUSTOM
} bpbx_vibrato_preset_e;

typedef enum {
    BPBX_VIBRATO_TYPE_NORMAL,
    BPBX_VIBRATO_TYPE_SHAKY
} bpbx_vibrato_type_e;

typedef enum {
    BPBX_FILTER_TYPE_OFF,
    BPBX_FILTER_TYPE_LP,
    BPBX_FILTER_TYPE_HP,
    BPBX_FILTER_TYPE_NOTCH
} bpbx_filter_type_e;

typedef enum {
    BPBX_CHIP_WAVE_ROUNDED,
    BPBX_CHIP_WAVE_TRIANGLE,
    BPBX_CHIP_WAVE_SQUARE,
    BPBX_CHIP_WAVE_PULSE4,
    BPBX_CHIP_WAVE_PULSE8,
    BPBX_CHIP_WAVE_SAWTOOTH,
    BPBX_CHIP_WAVE_DOUBLE_SAW,
    BPBX_CHIP_WAVE_DOUBLE_PULSE,
    BPBX_CHIP_WAVE_SPIKY,
    BPBX_CHIP_WAVE_SINE,
    BPBX_CHIP_WAVE_FLUTE,
    BPBX_CHIP_WAVE_HARP,
    BPBX_CHIP_WAVE_SHARP_CLARINET,
    BPBX_CHIP_WAVE_SOFT_CLARINET,
    BPBX_CHIP_WAVE_ALTO_SAX,
    BPBX_CHIP_WAVE_BASSOON,
    BPBX_CHIP_WAVE_TRUMPET,
    BPBX_CHIP_WAVE_ELECTRIC_GUITAR,
    BPBX_CHIP_WAVE_ORGAN,
    BPBX_CHIP_WAVE_PAN_FLUTE,
    BPBX_CHIP_WAVE_GLITCH,
    BPBX_CHIP_WAVE_COUNT
} bpbx_chip_wave_e;

typedef enum {
    BPBX_UNISON_NONE,
    BPBX_UNISON_SHIMMER,
    BPBX_UNISON_HUM,
    BPBX_UNISON_HONKY_TONK,
    BPBX_UNISON_DISSONANT,
    BPBX_UNISON_FIFTH,
    BPBX_UNISON_OCTAVE,
    BPBX_UNISON_BOWED,
    BPBX_UNISON_PIANO,
    BPBX_UNISON_WARBLED,
    BPBX_UNISON_COUNT,
} bpbx_unsion_type_e;

typedef enum {
    BPBX_NOISE_RETRO,
    BPBX_NOISE_WHITE,
    BPBX_NOISE_CLANG,
    BPBX_NOISE_BUZZ,
    BPBX_NOISE_HOLLOW,
    BPBX_NOISE_SHINE,
    BPBX_NOISE_DEEP,
    BPBX_NOISE_CUTTER,
    BPBX_NOISE_METALLIC,
    BPBX_NOISE_COUNT
} bpbx_noise_type_e;

typedef enum {
    BPBX_PARAM_UINT8,
    BPBX_PARAM_INT,
    BPBX_PARAM_DOUBLE
} bpbx_inst_param_type_e;

typedef enum {
    BPBX_PARAM_FLAG_NO_AUTOMATION = 1
} bpbx_inst_param_flags_e;

typedef enum {
    BPBX_ENV_INDEX_NONE,
    BPBX_ENV_INDEX_NOTE_VOLUME,
    BPBX_ENV_INDEX_NOTE_FILTER_ALL_FREQS,
    BPBX_ENV_INDEX_PULSE_WIDTH,
    BPBX_ENV_INDEX_STRING_SUSTAIN,
    BPBX_ENV_INDEX_UNISON,
    BPBX_ENV_INDEX_OPERATOR_FREQ0,
    BPBX_ENV_INDEX_OPERATOR_FREQ1,
    BPBX_ENV_INDEX_OPERATOR_FREQ2,
    BPBX_ENV_INDEX_OPERATOR_FREQ3,
    BPBX_ENV_INDEX_OPERATOR_AMP0,
    BPBX_ENV_INDEX_OPERATOR_AMP1,
    BPBX_ENV_INDEX_OPERATOR_AMP2,
    BPBX_ENV_INDEX_OPERATOR_AMP3,
    BPBX_ENV_INDEX_FEEDBACK_AMP,
    BPBX_ENV_INDEX_PITCH_SHIFT,
    BPBX_ENV_INDEX_DETUNE,
    BPBX_ENV_INDEX_VIBRATO_DEPTH,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ0,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ1,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ2,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ3,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ4,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ5,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ6,
    BPBX_ENV_INDEX_NOTE_FILTER_FREQ7,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN0,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN1,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN2,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN3,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN4,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN5,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN6,
    BPBX_ENV_INDEX_NOTE_FILTER_GAIN7,
    BPBX_ENV_INDEX_SUPERSAW_DYNAMISM,
    BPBX_ENV_INDEX_SUPERSAW_SPREAD,
    BPBX_ENV_INDEX_SUPERSAW_SHAPE,
    BPBX_ENV_INDEX_COUNT
} bpbx_envelope_compute_index_e;

typedef struct {
    bpbx_envelope_compute_index_e index;
    uint8_t curve_preset;
} bpbx_envelope_s;

typedef struct {
    bpbx_inst_param_type_e type;
    uint32_t flags; // bpbx_inst_param_flags_e

    const char *name;
    const char *group;

    double min_value;
    double max_value;
    double default_value;

    const char **enum_values;
} bpbx_inst_param_info_s;

typedef struct {
    double depth; // in beepbox code, this was quantized to increments of 0.04
    double speed;
    int delay; // in ticks, I think?
    uint8_t type;
} bpbx_vibrato_params_s;

typedef struct {
    float *out_samples;
    size_t frame_count;
    
    // do NOT set this to zero or a very low value!
    // beepbox runs voice computations at a tick rate derived from the tempo.
    // if you don't have bpm information, set it to a dummy value like 60 or 150.
    double bpm;

    // beat must be continuously increasing in order for
    // the tremolo envelopes to work properly!
    // if you don't have that information or the song isn't playing,
    // simply constantly increase this by the bpm.
    double beat;

    // Current value of the mod wheel, from 0 to 1.
    // Resting value is 0.
    double mod_wheel;
} bpbx_run_ctx_s;

// frequency response analysis return data
// this is a complex number with an extra property "denom"
// the magnitude of this number is sqrt(real*real + imag*imag) / denom
typedef struct {
    double real;
    double imag;
    double denom;
} bpbx_freq_response_s;

typedef struct bpbx_inst_s bpbx_inst_s;

typedef void* (*bpbx_malloc_f)(size_t size, void *userdata);
typedef void (*bpbx_mfree_f)(void *ptr, void *userdata);

BEEPBOX_API void bpbx_version(uint32_t *major, uint32_t *minor, uint32_t *revision);

// allocation/deallocation will only occur in bpbx_inst_new and bpbx_inst_destroy
BEEPBOX_API void bpbx_set_allocator(bpbx_malloc_f alloc, bpbx_mfree_f free, void *userdata);

BEEPBOX_API unsigned int bpbx_param_count(bpbx_inst_type_e type);
BEEPBOX_API const bpbx_inst_param_info_s* bpbx_param_info(bpbx_inst_type_e type, unsigned int index);

BEEPBOX_API bpbx_inst_s* bpbx_inst_new(bpbx_inst_type_e inst_type);
BEEPBOX_API void bpbx_inst_destroy(bpbx_inst_s* inst);
BEEPBOX_API bpbx_inst_type_e bpbx_inst_type(const bpbx_inst_s *inst);

BEEPBOX_API void bpbx_inst_set_sample_rate(bpbx_inst_s *inst, double sample_rate);
BEEPBOX_API void bpbx_transport_begin_playback(bpbx_inst_s *inst, double beat, double bpm);

BEEPBOX_API int bpbx_inst_set_param_int(bpbx_inst_s* inst, int index, int value);
BEEPBOX_API int bpbx_inst_set_param_double(bpbx_inst_s* inst, int index, double value);

BEEPBOX_API int bpbx_inst_get_param_int(const bpbx_inst_s* inst, int index, int *value);
BEEPBOX_API int bpbx_inst_get_param_double(const bpbx_inst_s* inst, int index, double *value);

BEEPBOX_API unsigned int bpbx_effect_toggle_param(bpbx_instfx_type_e type);

BEEPBOX_API const char* bpbx_envelope_index_name(bpbx_envelope_compute_index_e index);

// returns the array of curve preset names.
// the array is terminated by a null pointer.
BEEPBOX_API const char** bpbx_envelope_curve_preset_names();

BEEPBOX_API const bpbx_envelope_compute_index_e* bpbx_envelope_targets(bpbx_inst_type_e type, int *size);

BEEPBOX_API uint8_t bpbx_inst_envelope_count(const bpbx_inst_s *inst);
// note: envelopes are stored contiguously and in order, so it is valid to treat the return value
// as an array.
BEEPBOX_API bpbx_envelope_s* bpbx_inst_get_envelope(bpbx_inst_s *inst, uint32_t index);
BEEPBOX_API bpbx_envelope_s* bpbx_inst_add_envelope(bpbx_inst_s *inst);
BEEPBOX_API void bpbx_inst_remove_envelope(bpbx_inst_s *inst, uint8_t index);
BEEPBOX_API void bpbx_inst_clear_envelopes(bpbx_inst_s *inst);

BEEPBOX_API void bpbx_inst_midi_on(bpbx_inst_s *inst, int key, int velocity);
BEEPBOX_API void bpbx_inst_midi_off(bpbx_inst_s *inst, int key, int velocity);

// if you know the length of each note, and the result of this is negative,
// call midi_off that positive number of samples before the note actually ends.
BEEPBOX_API double bpbx_samples_fade_out(double setting, double bpm, double sample_rate);
BEEPBOX_API void bpbx_vibrato_preset_params(bpbx_vibrato_preset_e preset, bpbx_vibrato_params_s *params);

BEEPBOX_API void bpbx_inst_run(bpbx_inst_s* inst, const bpbx_run_ctx_s *const run_ctx);

BEEPBOX_API double bpbx_freq_setting_to_hz(double freq_setting);
BEEPBOX_API double bpbx_linear_gain_to_setting(double gain);

// analyze the frequency response of a specific control point of a given instrument's note filter
// at a given frequency
BEEPBOX_API void bpbx_analyze_freq_response(
    bpbx_filter_type_e filter_type, double freq_setting, double gain_setting,
    double hz, double sample_rate, bpbx_freq_response_s *out);

BEEPBOX_API double bpbx_freq_response_magnitude(const bpbx_freq_response_s *self);
BEEPBOX_API double bpbx_freq_response_angle(const bpbx_freq_response_s *self);

#ifdef __cplusplus
}
}
#endif

#endif