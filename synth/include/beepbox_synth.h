/*
Copyright 2025 pkhead

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
extern "C" {
#endif

// clang-format off
#ifdef _WIN32
#   if defined(BPBXSYN_SHARED) && defined(BPBXSYN_SHARED_IMPORT)
#       define BPBXSYN_API __declspec(dllimport)
#   elif defined(BPBXSYN_SHARED)
#       define BPBXSYN_API __declspec(dllexport)
#   else
#       define BPBXSYN_API
#   endif
#else
#   ifdef BPBXSYN_SHARED
#       define BPBXSYN_API __attribute__((visibility("default")))
#   else
#       define BPBXSYN_API
#   endif
#endif
// clang-format on

#include "beepbox_instrument_data.h"

#define BPBXSYN_VERSION_MAJOR 0
#define BPBXSYN_VERSION_MINOR 2
// to get the revision, call bpbxsyn_version

#define BPBXSYN_MAX_ENVELOPE_COUNT 12 // 16 in slarmoo's box
#define BPBXSYN_ENVELOPE_CURVE_PRESET_COUNT 26
#define BPBXSYN_FILTER_GROUP_COUNT 8
#define BPBXSYN_FILTER_GAIN_CENTER 7
#define BPBXSYN_FILTER_FREQ_REFERENCE_SETTING 28
#define BPBXSYN_FILTER_FREQ_RANGE 34
#define BPBXSYN_FILTER_GAIN_RANGE 15
#define BPBXSYN_FILTER_FREQ_MAX ((BPBXSYN_FILTER_FREQ_RANGE - 1))
#define BPBXSYN_FILTER_GAIN_MAX ((BPBXSYN_FILTER_GAIN_RANGE - 1))
#define BPBXSYN_NOTE_LENGTH_UNKNOWN -1

typedef enum {
    BPBXSYN_SYNTH_CHIP,
    BPBXSYN_SYNTH_FM,
    BPBXSYN_SYNTH_NOISE,
    BPBXSYN_SYNTH_PULSE_WIDTH,
    BPBXSYN_SYNTH_HARMONICS,
    BPBXSYN_SYNTH_SPECTRUM,
    BPBXSYN_SYNTH_PICKED_STRING,
    BPBXSYN_SYNTH_SUPERSAW,
} bpbxsyn_synth_type_e;

typedef enum {
    BPBXSYN_SYNTHFX_TRANSITION_TYPE,
    BPBXSYN_SYNTHFX_CHORD_TYPE,
    BPBXSYN_SYNTHFX_PITCH_SHIFT,
    BPBXSYN_SYNTHFX_DETUNE,
    BPBXSYN_SYNTHFX_VIBRATO,
    BPBXSYN_SYNTHFX_NOTE_FILTER
} bpbxsyn_synthfx_type_e;

typedef enum {
    BPBXSYN_TRANSITION_TYPE_NORMAL,
    BPBXSYN_TRANSITION_TYPE_INTERRUPT,
    BPBXSYN_TRANSITION_TYPE_CONTINUE,
    BPBXSYN_TRANSITION_TYPE_SLIDE
} bpbxsyn_transition_type_e;

typedef enum {
    BPBXSYN_CHORD_TYPE_SIMULTANEOUS,
    BPBXSYN_CHORD_TYPE_STRUM,
    BPBXSYN_CHORD_TYPE_ARPEGGIO,
    BPBXSYN_CHORD_TYPE_CUSTOM_INTERVAL
} bpbxsyn_chord_type_e;

typedef enum {
    BPBXSYN_VIBRATO_PRESET_NONE,
    BPBXSYN_VIBRATO_PRESET_LIGHT,
    BPBXSYN_VIBRATO_PRESET_DELAYED,
    BPBXSYN_VIBRATO_PRESET_HEAVY,
    BPBXSYN_VIBRATO_PRESET_SHAKY,
    BPBXSYN_VIBRATO_PRESET_CUSTOM
} bpbxsyn_vibrato_preset_e;

typedef enum {
    BPBXSYN_VIBRATO_TYPE_NORMAL,
    BPBXSYN_VIBRATO_TYPE_SHAKY
} bpbxsyn_vibrato_type_e;

typedef enum {
    BPBXSYN_CHIP_WAVE_ROUNDED,
    BPBXSYN_CHIP_WAVE_TRIANGLE,
    BPBXSYN_CHIP_WAVE_SQUARE,
    BPBXSYN_CHIP_WAVE_PULSE4,
    BPBXSYN_CHIP_WAVE_PULSE8,
    BPBXSYN_CHIP_WAVE_SAWTOOTH,
    BPBXSYN_CHIP_WAVE_DOUBLE_SAW,
    BPBXSYN_CHIP_WAVE_DOUBLE_PULSE,
    BPBXSYN_CHIP_WAVE_SPIKY,
    BPBXSYN_CHIP_WAVE_SINE,
    BPBXSYN_CHIP_WAVE_FLUTE,
    BPBXSYN_CHIP_WAVE_HARP,
    BPBXSYN_CHIP_WAVE_SHARP_CLARINET,
    BPBXSYN_CHIP_WAVE_SOFT_CLARINET,
    BPBXSYN_CHIP_WAVE_ALTO_SAX,
    BPBXSYN_CHIP_WAVE_BASSOON,
    BPBXSYN_CHIP_WAVE_TRUMPET,
    BPBXSYN_CHIP_WAVE_ELECTRIC_GUITAR,
    BPBXSYN_CHIP_WAVE_ORGAN,
    BPBXSYN_CHIP_WAVE_PAN_FLUTE,
    BPBXSYN_CHIP_WAVE_GLITCH,
    BPBXSYN_CHIP_WAVE_COUNT
} bpbxsyn_chip_wave_e;

typedef enum {
    BPBXSYN_UNISON_NONE,
    BPBXSYN_UNISON_SHIMMER,
    BPBXSYN_UNISON_HUM,
    BPBXSYN_UNISON_HONKY_TONK,
    BPBXSYN_UNISON_DISSONANT,
    BPBXSYN_UNISON_FIFTH,
    BPBXSYN_UNISON_OCTAVE,
    BPBXSYN_UNISON_BOWED,
    BPBXSYN_UNISON_PIANO,
    BPBXSYN_UNISON_WARBLED,
    BPBXSYN_UNISON_COUNT,
} bpbxsyn_unsion_type_e;

typedef enum {
    BPBXSYN_EFFECT_EQ,
    BPBXSYN_EFFECT_PANNING,
    BPBXSYN_EFFECT_DISTORTION,
    BPBXSYN_EFFECT_BITCRUSHER,
    BPBXSYN_EFFECT_CHORUS,
    BPBXSYN_EFFECT_ECHO,
    BPBXSYN_EFFECT_REVERB,
} bpbxsyn_effect_type_e;

typedef enum {
    BPBXSYN_NOISE_RETRO,
    BPBXSYN_NOISE_WHITE,
    BPBXSYN_NOISE_CLANG,
    BPBXSYN_NOISE_BUZZ,
    BPBXSYN_NOISE_HOLLOW,
    BPBXSYN_NOISE_SHINE,
    BPBXSYN_NOISE_DEEP,
    BPBXSYN_NOISE_CUTTER,
    BPBXSYN_NOISE_METALLIC,
    BPBXSYN_NOISE_COUNT
} bpbxsyn_noise_type_e;

typedef enum {
    BPBXSYN_FILTER_TYPE_OFF,
    BPBXSYN_FILTER_TYPE_LP,
    BPBXSYN_FILTER_TYPE_HP,
    BPBXSYN_FILTER_TYPE_NOTCH
} bpbxsyn_filter_type_e;

typedef enum {
    BPBXSYN_PARAM_UINT8,
    BPBXSYN_PARAM_INT,
    BPBXSYN_PARAM_DOUBLE
} bpbxsyn_param_type_e;

typedef enum { BPBXSYN_PARAM_FLAG_NO_AUTOMATION } bpbxsyn_param_flags_e;

typedef enum {
    BPBXSYN_ENV_INDEX_NONE,
    BPBXSYN_ENV_INDEX_NOTE_VOLUME,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_ALL_FREQS,
    BPBXSYN_ENV_INDEX_PULSE_WIDTH,
    BPBXSYN_ENV_INDEX_STRING_SUSTAIN,
    BPBXSYN_ENV_INDEX_UNISON,
    BPBXSYN_ENV_INDEX_OPERATOR_FREQ0,
    BPBXSYN_ENV_INDEX_OPERATOR_FREQ1,
    BPBXSYN_ENV_INDEX_OPERATOR_FREQ2,
    BPBXSYN_ENV_INDEX_OPERATOR_FREQ3,
    BPBXSYN_ENV_INDEX_OPERATOR_AMP0,
    BPBXSYN_ENV_INDEX_OPERATOR_AMP1,
    BPBXSYN_ENV_INDEX_OPERATOR_AMP2,
    BPBXSYN_ENV_INDEX_OPERATOR_AMP3,
    BPBXSYN_ENV_INDEX_FEEDBACK_AMP,
    BPBXSYN_ENV_INDEX_PITCH_SHIFT,
    BPBXSYN_ENV_INDEX_DETUNE,
    BPBXSYN_ENV_INDEX_VIBRATO_DEPTH,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ0,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ1,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ2,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ3,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ4,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ5,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ6,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_FREQ7,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN0,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN1,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN2,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN3,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN4,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN5,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN6,
    BPBXSYN_ENV_INDEX_NOTE_FILTER_GAIN7,
    BPBXSYN_ENV_INDEX_SUPERSAW_DYNAMISM,
    BPBXSYN_ENV_INDEX_SUPERSAW_SPREAD,
    BPBXSYN_ENV_INDEX_SUPERSAW_SHAPE,
    BPBXSYN_ENV_INDEX_COUNT
} bpbxsyn_envelope_compute_index_e;

typedef struct {
    bpbxsyn_envelope_compute_index_e index;
    uint8_t curve_preset;
} bpbxsyn_envelope_s;

typedef struct {
    bpbxsyn_param_type_e type;
    uint32_t flags; // bpbxsyn_param_flags_e

    const char *name;
    const char *group;
    char id[8];

    double min_value;
    double max_value;
    double default_value;

    const char **enum_values;
} bpbxsyn_param_info_s;

typedef struct {
    double depth; // in beepbox code, this was quantized to increments of 0.04
    double speed;
    int delay; // in ticks, I think?
    uint8_t type;
} bpbxsyn_vibrato_params_s;

typedef struct {
    /**
     * The tempo of the transport, in beats per minute.
     *
     * @important Do not set this to zero or a very low value! The rate at which
     *            certain computations are done depends on the transport's
     *            tempo. See the documentation on @ref bpbxsyn_synth_tick for
     *            more information.
     *
     * If you don't have tempo information, set it to a placeholder value like
     * 150.
     **/
    double bpm;

    /**
     * The current beat position of the transport.
     * @important This must be continuously increasing in order for the tremolo
     *            envelopes to work properly! This must be accurately and
     *            constantly increased by the tempo of the transport.
     **/
    double beat;

    /**
     * This is basically essentially the "note size" parameter in BeepBox.
     * It is a value from 0 to 1. The resting value is 1.
     **/
    double mod_wheel;
} bpbxsyn_tick_ctx_s;

typedef struct {
    double real;
    double imag;
} bpbxsyn_complex_s;

typedef struct bpbxsyn_synth_s bpbxsyn_synth_s;
typedef struct bpbxsyn_effect_s bpbxsyn_effect_s;

typedef void *(*bpbxsyn_malloc_f)(size_t size, void *userdata);
typedef void (*bpbxsyn_mfree_f)(void *ptr, void *userdata);
typedef int8_t bpbxsyn_voice_id;

typedef enum {
    BPBXSYN_LOG_DEBUG,
    BPBXSYN_LOG_INFO,
    BPBXSYN_LOG_WARNING,
    BPBXSYN_LOG_ERROR,
    BPBXSYN_LOG_FATAL,
} bpbxsyn_log_severity_e;

typedef void (*bpbxsyn_log_f)(bpbxsyn_log_severity_e severity, const char *msg,
                              void *userdata);

typedef struct {
    /**
     * Called when a voice is finished being processed.
     * @param inst Pointer to the instrument.
     * @param id The ID of the voice.
     */
    void (*voice_end)(bpbxsyn_synth_s *inst, bpbxsyn_voice_id id);
} bpbxsyn_synth_callbacks_s;

/**
 * @brief Obtain the version of the synth library.
 *
 * This will write the major, minor, and revision numbers of the current version
 * of cbeepsynth into the given output parameters.
 *
 * @param[out] major Major version number
 * @param[out] minor Minor version number
 * @param[out] revision Revision version number
 */
BPBXSYN_API void bpbxsyn_version(uint32_t *major, uint32_t *minor,
                                 uint32_t *revision);

/**
 * @brief Set custom allocators.
 *
 * Use this function if you want cbeepsynth to use a custom allocator instead of
 * the one in the C standard library.
 *
 * There are a couple of functions where allocation/freeing may occur:
 *   - bpbxsyn_synth_new
 *   - bpbxsyn_effect_new
 *   - bpbxsyn_synth_destroy
 *   - bpbxsyn_effect_destroy
 *   - bpbxsyn_synth_set_sample_rate
 *   - bpbxsyn_effect_set_sample_rate
 *
 * @param alloc The function to allocate a new block of memory.
 * @param free The function to free an allocated block of memory.
 * @param userdata An opaque pointer passed to the two allocation functions.
 */
BPBXSYN_API void bpbxsyn_set_allocator(bpbxsyn_malloc_f alloc,
                                       bpbxsyn_mfree_f free, void *userdata);

/**
 * @brief Set the log function for the library.
 *
 * Set the log function to be used by the library. The given log function
 * must be thread-safe.
 *
 * @param log_func The log function to use.
 * @param userdata An opaque pointer to be passed to the given log function.
 */
BPBXSYN_API void bpbxsyn_set_log_func(bpbxsyn_log_f log_func, void *userdata);

/**
 * @brief Obtain the number of parameters for a given instrument type.
 *
 * @param type The BPBXSYN_SYNTH_* enum that identifies the instrument type.
 * @return The number of parameters the instrument type has.
 */
BPBXSYN_API unsigned int bpbxsyn_synth_param_count(bpbxsyn_synth_type_e type);

/**
 * @brief Obtain information for a specific parameter of an instrument type.
 *
 * @param type  The BPBXSYN_SYNTH_* enum that identifies the instrument type.
 * @param index The BPBXSYN_PARAM_* or BPBXSYN_{inst}_PARAM_* enum that
 *              identifies the parameter.
 * @return The bpbxsyn_param_info_s struct containing information about the
 * parameter.
 */
BPBXSYN_API const bpbxsyn_param_info_s *
bpbxsyn_synth_param_info(bpbxsyn_synth_type_e type, unsigned int index);

/**
 * @brief Allocate a new instrument of a given type.
 *
 * This allocates a new insturment of a given type. Since it uses memory
 * allocation functions, this function may return a failure notice.
 *
 * @param inst_type The BPBXSYN_SYNTH_* enum that identifies the instrument
 *                  type.
 * @return Pointer to the newly allocated instrument. NULL on error.
 */
BPBXSYN_API bpbxsyn_synth_s *bpbxsyn_synth_new(bpbxsyn_synth_type_e inst_type);

/**
 * @brief Free a previously allocated instrument.
 *
 * @param inst Pointer to the instrument struct to free. Does nothing if NULL is
 *             passed.
 */
BPBXSYN_API void bpbxsyn_synth_destroy(bpbxsyn_synth_s *inst);

/**
 * @brief Obtains the type of an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The BPBXSYN_SYNTH_* enum that identifies the instrument type.
 */
BPBXSYN_API bpbxsyn_synth_type_e
bpbxsyn_synth_type(const bpbxsyn_synth_s *inst);

/**
 * @brief Get the callback table of the instrument.
 *
 * This will return a pointer to the callback table structure of the instrument
 * These functions are called during inst_tick or inst_run.
 *
 * @param inst Pointer to the instrument.
 * @return Pointer to the bpbxsyn_synth_callbacks_s structure.
 */
BPBXSYN_API bpbxsyn_synth_callbacks_s *
bpbxsyn_synth_get_callback_table(bpbxsyn_synth_s *inst);

/**
 * Obtain the user-set opaque pointer associated with an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The user-set opaque pointer, or NULL if it was not set.
 */
BPBXSYN_API void *bpbxsyn_synth_get_userdata(bpbxsyn_synth_s *inst);

/**
 * Associate an instrument with an opaque pointer.
 *
 * @param inst Pointer to the instrument.
 * @param userdata The pointer to associate with the instrument.
 */
BPBXSYN_API void bpbxsyn_synth_set_userdata(bpbxsyn_synth_s *inst,
                                            void *userdata);

/**
 * @brief Set the sample rate of an instrument.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BPBXSYN_API void bpbxsyn_synth_set_sample_rate(bpbxsyn_synth_s *inst,
                                               double sample_rate);

/**
 * @brief Signal to an instrument that transport playback has started.
 *
 * This function should be called whenever transport begins. Or in other words,
 * whenever the song's track has started being played.
 *
 * @param inst Pointer to the instrument.
 * @param beat The beat that the transport started on.
 * @param bpm  The tempo of the transport, in beats per minute.
 */
BPBXSYN_API void bpbxsyn_synth_begin_transport(bpbxsyn_synth_s *inst,
                                               double beat, double bpm);

/**
 * @brief Set an instrument's parameter to an integer value.
 *
 * Set an instrument's parameter to an integer value. The parameter can have any
 * underlying type.
 *
 * @param inst  Pointer to the instrument.
 * @param param The BPBXSYN_PARAM_* or BPBXSYN_{inst}_PARAM_* enum that
 * identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_synth_set_param_int(bpbxsyn_synth_s *inst,
                                            uint32_t param, int value);

/**
 * @brief Set an instrument's parameter to a floating-point value.
 *
 * Set an instrument's parameter to a double-precision floating-point value. If
 * the underlying type of the parameter is an integer type, this function will
 * fail.
 *
 * @param inst  Pointer to the instrument.
 * @param param The BPBXSYN_PARAM_* or BPBXSYN_{inst}_PARAM_* enum that
 * identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_synth_set_param_double(bpbxsyn_synth_s *inst,
                                               uint32_t param, double value);

/**
 * @brief Get the value of an instrument's parameter as an integer value.
 *
 * Gets the value of an instrument's parameter as an integer value. If the
 * underlying type of the param is a floating-point type, this function will
 * fail.
 *
 * @param      inst  Pointer to the instrument.
 * @param      param The BPBXSYN_PARAM_* or BPBXSYN_{inst}_PARAM_* enum that
 *                   identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_synth_get_param_int(const bpbxsyn_synth_s *inst,
                                            uint32_t param, int *value);

/**
 * @brief Get the value of an instrument's parameter as a floating-point value.
 *
 * Gets the value of an instrument's parameter as a double-precision floating
 * point value.
 *
 * @param      inst  Pointer to the instrument.
 * @param      param The BPBXSYN_PARAM_* or BPBXSYN_{inst}_PARAM_* enum that
 *                   identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_synth_get_param_double(const bpbxsyn_synth_s *inst,
                                               uint32_t param, double *value);

/**
 * @brief Get the parameter index of a note effect toggle.
 *
 * This returns the instrument index of the toggle parameter for a given note
 * effect.
 *
 * @param type The BPBXSYN_SYNTHFX_* enum that identifies the note effect type.
 * @return The parameter index on success, and UINT32_MAX on failure.
 */
BPBXSYN_API uint32_t
bpbxsyn_synth_effect_toggle_param(bpbxsyn_synthfx_type_e type);

/**
 * @brief Get the list of possible envelope targets for a given instrument type.
 * *
 *
 * @param      type The BPBXSYN_SYNTH_* enum that identifies the instrument
 *                  type.
 * @param[out] size Output parameter, giving the size of the output list.
 * @return List of envelope target indices.
 */
BPBXSYN_API const bpbxsyn_envelope_compute_index_e *
bpbxsyn_synth_envelope_targets(bpbxsyn_synth_type_e type, int *size);

/**
 * @brief Get the number of active envelopes for an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The number of active envelopes.
 */
BPBXSYN_API uint8_t bpbxsyn_synth_envelope_count(const bpbxsyn_synth_s *inst);
// note: envelopes are stored contiguously and in order, so it is valid to treat
// the return value as an array.

/**
 * @brief Get the contiguous list of envelope configurations, starting from an
 * index.
 *
 * This returns the pointer to the data for the configuration of an envelope of
 * an instrument. These are stored contiguously and in order, so it is valid to
 * treat the return value asn array.
 *
 * @param inst  Pointer to the instrument.
 * @param index The index to the envelope configuration.
 */
BPBXSYN_API bpbxsyn_envelope_s *
bpbxsyn_synth_get_envelope(bpbxsyn_synth_s *inst, uint8_t index);

/**
 * @brief Add a new envelope to an instrument.
 *
 * This activates a new envelope for an instrument, and then returns the pointer
 * to its configuration structure.
 *
 * This function will return a failure notice if the number of envelopes will
 * exceed BPBXSYN_MAX_ENVELOPE_COUNT.
 *
 * @param inst Pointer to the instrument.
 * @return On success, pointer to the envelope configuration structure. NULL on
 * failure.
 */
BPBXSYN_API bpbxsyn_envelope_s *
bpbxsyn_synth_add_envelope(bpbxsyn_synth_s *inst);

/**
 * @brief Removes an instrument's envelope.
 *
 * This will remove an active envelope for an instrument, and shift the indices
 * of the other active envelopes to fill the spot.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The index of the envelope to remove
 */
BPBXSYN_API void bpbxsyn_synth_remove_envelope(bpbxsyn_synth_s *inst,
                                               uint8_t index);

/**
 * @brief Clears all the active envelopes of an instrument.
 *
 * @param inst Pointer to the instrument.
 */
BPBXSYN_API void bpbxsyn_synth_clear_envelopes(bpbxsyn_synth_s *inst);

/**
 * @brief Send a note on event to an instrument.
 *
 * @note If the length given is BPBXSYN_NOTE_LENGTH_UNKNOWN, then the note will
 * play until bpbxsyn_synth_end_note is called for the given voice id.
 * Otherwise, the note will end automatically based on the given length.
 *
 * @param inst     Pointer to the instrument.
 * @param key      The MIDI key of the note.
 * @param velocity The velocity of the note, from 0-1.
 * @param length   The length of the note, in ticks. BPBXSYN_NOTE_LENGTH_UNKNOWN
 *                 if unknown.
 * @returns The ID of the newly created voice.
 */
BPBXSYN_API bpbxsyn_voice_id bpbxsyn_synth_begin_note(bpbxsyn_synth_s *inst,
                                                      int key, double velocity,
                                                      int32_t length);

/**
 * @brief Send a note off event to an instrument.
 * 
 * @param inst Pointer to the instrument.
 * @param id The ID of the note to end.
 */
BPBXSYN_API void bpbxsyn_synth_end_note(bpbxsyn_synth_s *inst,
                                        bpbxsyn_voice_id id);

/**
 * @brief Turn off all active notes of an instrument.
 *
 * @param inst Pointer to the instrument.
 */
BPBXSYN_API void bpbxsyn_synth_end_all_notes(bpbxsyn_synth_s *inst);

/**
 * @brief Choke all active notes.
 *
 * @param inst Pointer to the instrument.
 */
BPBXSYN_API void bpbxsyn_synth_stop(bpbxsyn_synth_s *inst);

/**
 * @brief Tick an instrument.
 *
 * While computing audio, BeepBox instruments need to be ticked at a rate
 * dependent on the tempo of the song. On each tick, voices will be updated, and
 * envelope/automation values will be calculated, and interpolated across the
 * frames of an audio render.
 *
 * To get the tick rate, use the bpbxsyn_calc_samples_per_tick function.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BPBXSYN_API void bpbxsyn_synth_tick(bpbxsyn_synth_s *inst,
                                    const bpbxsyn_tick_ctx_s *tick_ctx);
// mono output

/**
 * @brief Render the audio of an instrument.
 *
 * This renders the audio of an instrument to a mono audio output buffer of a
 * given size.
 *
 * @param inst        Pointer to the instrument.
 * @param output      The mono audio output buffer.
 * @param frame_count The number of frames to render to the output buffer.
 */
BPBXSYN_API void bpbxsyn_synth_run(bpbxsyn_synth_s *inst, float *output,
                                   size_t frame_count);

/**
 * @brief Obtain the number of parameters for a given effect type.
 *
 * @param type The BPBXSYN_EFFECT_* enum that identifies the effect type.
 * @return The number of parameters the effect type has.
 */
BPBXSYN_API unsigned int bpbxsyn_effect_param_count(bpbxsyn_effect_type_e type);

/**
 * @brief Get channel count info for a given effect type.
 *
 * Obtains the number channels that an effect type receives and outputs,
 * respectively. 1 is mono, and 2 is stereo.
 *
 * @param      type The BPBXSYN_EFFECT_* enum that identifies the effect type.
 * @param[out] inputs The number of input channels for the given type.
 * @param[out] outputs The number of output channels for the given type.
 */
BPBXSYN_API void bpbxsyn_effect_channel_info(bpbxsyn_effect_type_e type,
                                             int *inputs, int *outputs);

/**
 * @brief Obtain information for a specific parameter of an effect type.
 *
 * @param type  The BPBXSYN_EFFECT_* enum that identifies the instrument type.
 * @param index The BPBXSYN_{effect}_PARAM_* enum that identifies the parameter.
 * @return The bpbxsyn_param_info_s struct containing information about the
 * parameter.
 */
BPBXSYN_API const bpbxsyn_param_info_s *
bpbxsyn_effect_param_info(bpbxsyn_effect_type_e type, unsigned int index);

/**
 * @brief Allocate a new effect instance of a given type.
 *
 * This allocates a new effect instance of a given type. Since it uses memory
 * allocation functions, this function may return a failure notice.
 *
 * @param effect_type The BPBXSYN_EFFECT_* enum that identifies the effect type.
 * @return Pointer to the newly allocated effect instance. NULL on error.
 */
BPBXSYN_API bpbxsyn_effect_s *
bpbxsyn_effect_new(bpbxsyn_effect_type_e effect_type);

/**
 * @brief Free a previously allocated effect instance.
 *
 * @param effect Pointer to the effect instance struct to free. Does nothing if
 *               NULL is passed.
 */
BPBXSYN_API void bpbxsyn_effect_destroy(bpbxsyn_effect_s *effect);

/**
 * @brief Obtains the type of an effect instance.
 *
 * @param effect Pointer to the effect instance.
 * @return The BPBXSYN_EFFECT_* enum that identifies the instrument type.
 */
BPBXSYN_API bpbxsyn_effect_type_e
bpbxsyn_effect_type(const bpbxsyn_effect_s *effect);

/**
 * Obtain the user-set opaque pointer associated with an effect instance.
 *
 * @param effect Pointer to the effect instance.
 * @return The user-set opaque pointer, or NULL if it was not set.
 */
BPBXSYN_API void *bpbxsyn_effect_get_userdata(bpbxsyn_effect_s *effect);

/**
 * Associate an instrument with an opaque pointer.
 *
 * @param effect   Pointer to the effect instance.
 * @param userdata The pointer to associate with the effect instance.
 */
BPBXSYN_API void bpbxsyn_effect_set_userdata(bpbxsyn_effect_s *effect,
                                             void *userdata);

/**
 * @brief Set the sample rate of an effect instance.
 *
 * @param effect      Pointer to the effect instance.
 * @param sample_rate The sample rate in Hz.
 */
BPBXSYN_API void bpbxsyn_effect_set_sample_rate(bpbxsyn_effect_s *effect,
                                                double sample_rate);

// /**
//  * @brief Signal to an effect instance that transport playback has started.
//  *
//  * This function should be called whenever transport begins. Or in other words,
//  * whenever the song's track has started being played.
//  *
//  * @param effect Pointer to the effect instance.
//  * @param beat   The beat that the transport started on.
//  * @param bpm    The tempo of the transport, in beats per minute.
//  */
// BPBXSYN_API void bpbxsyn_effect_begin_transport(bpbxsyn_effect_s *effect,
//                                                 double beat, double bpm);

/**
 * @brief Set an effect instance's parameter to an integer value.
 *
 * Set an effect instance's parameter to an integer value. The parameter can
 * have any underlying type.
 *
 * @param effect Pointer to the effect instance.
 * @param param  The BPBXSYN_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param value  The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_effect_set_param_int(bpbxsyn_effect_s *effect,
                                             uint32_t param, int value);

/**
 * @brief Set an effect's parameter to a floating-point value.
 *
 * Set an effect's parameter to a double-precision floating-point value. If the
 * underlying type of the parameter is an integer type, this function will
 * fail.
 *
 * @param effect Pointer to the effect instance.
 * @param param  The BPBXSYN_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param value  The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_effect_set_param_double(bpbxsyn_effect_s *inst,
                                                uint32_t param, double value);

/**
 * @brief Get the value of an effect instance's parameter as an integer value.
 *
 * Gets the value of an effect instance's parameter as an integer value. If the
 * underlying type of the param is a floating-point type, this function will
 * fail.
 *
 * @param      effect Pointer to the effect instance.
 * @param      param  The BPBXSYN_{effect}_PARAM_* enum that identifies the
 *                    parameter.
 * @param[out] value  The output value.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_effect_get_param_int(const bpbxsyn_effect_s *inst,
                                             uint32_t param, int *value);

/**
 * @brief Get the value of an effect instance's parameter as a floating-point
 * value.
 *
 * Gets the value of an effect instance's parameter as a double-
 * floating point value.
 *
 * @param      effect Pointer to the effect instance.
 * @param      param  The BPBXSYN_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param[out] value  The output value.
 * @return 0 on success, and 1 on failure.
 */
BPBXSYN_API int bpbxsyn_effect_get_param_double(const bpbxsyn_effect_s *effect,
                                                uint32_t param, double *value);

/**
 * @brief Clear any feedback/delay systems.
 *
 * Clears any feedback or delay systems the effect is using. It has the
 * effect of removing any potential tail that the effect might produce at
 * the moment of the call.
 * 
 * @param inst Pointer to the instrument.
 */
BPBXSYN_API void bpbxsyn_effect_stop(bpbxsyn_effect_s *inst);

/**
 * @brief Tick an effect instance.
 *
 * While computing audio, BeepBox effects need to be ticked at a rate dependent
 * on the tempo of the song. Values will be calculated on each tick and
 * interpolated along the individual frames of an audio render.
 *
 * To get the tick rate, use the bpbxsyn_calc_samples_per_tick function.
 *
 * @param inst     Pointer to the instrument.
 * @param tick_ctx Pointer to a bpbxsyn_tick_ctx structure.
 */
BPBXSYN_API void bpbxsyn_effect_tick(bpbxsyn_effect_s *effect,
                                     const bpbxsyn_tick_ctx_s *tick_ctx);
// mono output

/**
 * @brief Run an effect instance.
 *
 * This takes in a stereo audio buffer containing the input, and performs audio
 * processing in-place. Each of the two audio channels are independent buffers.
 *
 * @param effect      Pointer to the effect instance.
 * @param buffer      Pointer to two audio buffers, for the left and right
                      channels respectively.
 * @param frame_count The number of frames in both the input and output buffer.
 */
BPBXSYN_API void bpbxsyn_effect_run(bpbxsyn_effect_s *effect, float **buffer,
                                    size_t frame_count);

/**
 * @brief Get the name of an envelope.
 *
 * Given the index of an envelope target, this returns the name of the target as
 * a pointer to a a null-terminated UTF8-encoded string. The caller must not
 * modify this string.
 *
 * @param inst The BPBXSYN_ENV_INDEX_* enum that identifies the envelope target.
 * @return The C string on success, and NULL on error.
 */
BPBXSYN_API const char *
bpbxsyn_envelope_index_name(bpbxsyn_envelope_compute_index_e index);

/**
 * @brief Get the null-terminated list of names for each curve preset.
 *
 * This returns a list containing the names of each envelope curve preset.
 * Each name is given with a pointer to a null-terminated UTF8-encoded string.
 * The end of the list is denoted by a NULL pointer instead of a pointer to a
 * string. The caller must not modify the return value of this function.
 *
 * @return The null-terminated list of names.
 */
BPBXSYN_API const char **bpbxsyn_envelope_curve_preset_names(void);

/**
 * @brief Calculates the number of audio frames per tick.
 *
 * This calculates the number of audio frames that need to be computed
 * in-between each call to bpbxsyn_synth_tick, given the tempo of the transport
 * and the audio output's sample rate.
 *
 * If you do not have a transport, or do not know its tempo, it is fine to use a
 * placeholder value such as 150 bpm.
 *
 * @param inst        Pointer to the instrument.
 * @param bpm         The tempo of the transport, in beats per minute.
 * @param sample_rate The sample rate of the audio output.
 * @return The samples per tick, as a double.
 */
BPBXSYN_API double bpbxsyn_calc_samples_per_tick(double bpm,
                                                 double sample_rate);

/**
 * @brief Get the number of ticks it takes for an instrument to fade out.
 *
 * This calculates the number of ticks it takes for an instrument to fade out,
 * given the value of the fade out parameter of an instrument.
 *
 * If you know the length of each note, and the result of this is a negative
 * value, call bpbxsyn_synth_end_note that positive number of ticks before the
 * note actually ends.
 *
 * @param inst    Pointer to the instrument.
 * @param setting The value of the fade-out parameter of a (hypothetical)
 * instrument.
 * @return The signed fade-out length in ticks, as a double.
 */
BPBXSYN_API double bpbxsyn_ticks_fade_out(double setting);

/**
 * @brief Get the vibrato parameters of a vibrato preset.
 *
 * @param      preset The BPBXSYN_VIBRATO_PRESET_* enum that identifies the
 * vibrato preset.
 * @param[out] params Pointer to an output structure where the vibrato
 * parameters will be written.
 */
BPBXSYN_API void
bpbxsyn_vibrato_preset_params(bpbxsyn_vibrato_preset_e preset,
                              bpbxsyn_vibrato_params_s *params);

/**
 * @brief Convert the frequency setting of a hypothetical filter to Hz.
 *
 * This converts the frequency units of BeepBox audio filters to Hz.
 *
 * @param freq_setting The frequency setting of a hypothetical BeepBox filter.
 * @return The frequency in Hz.
 */
BPBXSYN_API double bpbxsyn_freq_setting_to_hz(double freq_setting);

/**
 * @brief Convert a linear gain value to the units for the gain/volume setting
 * of a filter.
 *
 * This converts a linear gain multiplier to the unit used for the gain/volume
 * setting of a BeepBox filter's control point.
 *
 * @param gain Linear gain.
 * @return The same gain represented in the unit of the gain/volume of a filter.
 */
BPBXSYN_API double bpbxsyn_linear_gain_to_setting(double gain);

/**
 * @brief Analyze the frequency response of a filter.
 *
 * This returns the frequency response data of a control point of an EQ or note
 * filter effect at the given frequency, as a complex number. The linear gain at
 * that frequency is then represented by the magnitude of that complex number.
 *
 * @param      filter_type  The BPBXSYN_FILTER_TYPE_* enum identifying the type
 *                          of the filter control point.
 * @param      freq_setting The frequency of the control point in the BeepBox
 *                          setting unit.
 * @param      gain_setting The gain/volume of the control point in the BeepBox
 *                          gain setting unit.
 * @param      hz           The frequency to analyze, in HZ.
 * @param      sample_rate  The sample rate to reference with.
 * @param[out] out          The output complex number
 */
BPBXSYN_API void bpbxsyn_analyze_freq_response(
    bpbxsyn_filter_type_e filter_type, double freq_setting, double gain_setting,
    double hz, double sample_rate, bpbxsyn_complex_s *out);

#ifdef __cplusplus
}
#endif

#endif