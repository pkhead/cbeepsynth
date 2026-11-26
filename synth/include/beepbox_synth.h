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

#ifndef BBXS_BEEPBOX_SYNTH_H
#define BBXS_BEEPBOX_SYNTH_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#ifdef _WIN32
#   if defined(BBXS_SHARED) && defined(BBXS_SHARED_IMPORT)
#       define BBXS_API __declspec(dllimport)
#   elif defined(BBXS_SHARED)
#       define BBXS_API __declspec(dllexport)
#   else
#       define BBXS_API
#   endif
#else
#   ifdef BBXS_SHARED
#       define BBXS_API __attribute__((visibility("default")))
#   else
#       define BBXS_API
#   endif
#endif
// clang-format on

#include "beepbox_instrument_data.h"

#define BBXS_VERSION_MAJOR 0
#define BBXS_VERSION_MINOR 2
// to get the revision, call bbxs_version

#define BBXS_MAX_ENVELOPE_COUNT 12 // 16 in slarmoo's box
#define BBXS_ENVELOPE_CURVE_PRESET_COUNT 26
#define BBXS_FILTER_GROUP_COUNT 8
#define BBXS_FILTER_GAIN_CENTER 7
#define BBXS_FILTER_FREQ_REFERENCE_SETTING 28
#define BBXS_FILTER_FREQ_RANGE 34
#define BBXS_FILTER_GAIN_RANGE 15
#define BBXS_FILTER_FREQ_MAX ((BBXS_FILTER_FREQ_RANGE - 1))
#define BBXS_FILTER_GAIN_MAX ((BBXS_FILTER_GAIN_RANGE - 1))

#define BBXS_USE_NOTE_SIZE NAN
#define BBXS_NOTE_LENGTH_UNKNOWN -1

typedef enum bbxs_synth_type {
    BBXS_SYNTH_CHIP,
    BBXS_SYNTH_FM,
    BBXS_SYNTH_NOISE,
    BBXS_SYNTH_PULSE_WIDTH,
    BBXS_SYNTH_HARMONICS,
    BBXS_SYNTH_SPECTRUM,
    BBXS_SYNTH_PICKED_STRING,
    BBXS_SYNTH_SUPERSAW,
    BBXS_SYNTH_CUSTOM_CHIP,

    BBXS_SYNTH_COUNT,
} bbxs_synth_type_e;

typedef enum bbxs_synthfx_type {
    BBXS_SYNTHFX_TRANSITION_TYPE,
    BBXS_SYNTHFX_CHORD_TYPE,
    BBXS_SYNTHFX_PITCH_SHIFT,
    BBXS_SYNTHFX_DETUNE,
    BBXS_SYNTHFX_VIBRATO,
    BBXS_SYNTHFX_NOTE_FILTER
} bbxs_synthfx_type_e;

typedef enum bbxs_transition_type {
    BBXS_TRANSITION_TYPE_NORMAL,
    BBXS_TRANSITION_TYPE_INTERRUPT,
    BBXS_TRANSITION_TYPE_CONTINUE,
    BBXS_TRANSITION_TYPE_SLIDE
} bbxs_transition_type_e;

typedef enum bbxs_chord_type {
    BBXS_CHORD_TYPE_SIMULTANEOUS,
    BBXS_CHORD_TYPE_STRUM,
    BBXS_CHORD_TYPE_ARPEGGIO,
    BBXS_CHORD_TYPE_CUSTOM_INTERVAL
} bbxs_chord_type_e;

typedef enum bbxs_vibrato_preset {
    BBXS_VIBRATO_PRESET_NONE,
    BBXS_VIBRATO_PRESET_LIGHT,
    BBXS_VIBRATO_PRESET_DELAYED,
    BBXS_VIBRATO_PRESET_HEAVY,
    BBXS_VIBRATO_PRESET_SHAKY,
    BBXS_VIBRATO_PRESET_CUSTOM
} bbxs_vibrato_preset_e;

typedef enum bbxs_vibrato_type {
    BBXS_VIBRATO_TYPE_NORMAL,
    BBXS_VIBRATO_TYPE_SHAKY
} bbxs_vibrato_type_e;

typedef enum bbxs_chip_wave {
    BBXS_CHIP_WAVE_ROUNDED,
    BBXS_CHIP_WAVE_TRIANGLE,
    BBXS_CHIP_WAVE_SQUARE,
    BBXS_CHIP_WAVE_PULSE4,
    BBXS_CHIP_WAVE_PULSE8,
    BBXS_CHIP_WAVE_SAWTOOTH,
    BBXS_CHIP_WAVE_DOUBLE_SAW,
    BBXS_CHIP_WAVE_DOUBLE_PULSE,
    BBXS_CHIP_WAVE_SPIKY,
    BBXS_CHIP_WAVE_SINE,
    BBXS_CHIP_WAVE_FLUTE,
    BBXS_CHIP_WAVE_HARP,
    BBXS_CHIP_WAVE_SHARP_CLARINET,
    BBXS_CHIP_WAVE_SOFT_CLARINET,
    BBXS_CHIP_WAVE_ALTO_SAX,
    BBXS_CHIP_WAVE_BASSOON,
    BBXS_CHIP_WAVE_TRUMPET,
    BBXS_CHIP_WAVE_ELECTRIC_GUITAR,
    BBXS_CHIP_WAVE_ORGAN,
    BBXS_CHIP_WAVE_PAN_FLUTE,
    BBXS_CHIP_WAVE_GLITCH,
    BBXS_CHIP_WAVE_COUNT
} bbxs_chip_wave_e;

typedef enum bbxs_unsion_type {
    BBXS_UNISON_NONE,
    BBXS_UNISON_SHIMMER,
    BBXS_UNISON_HUM,
    BBXS_UNISON_HONKY_TONK,
    BBXS_UNISON_DISSONANT,
    BBXS_UNISON_FIFTH,
    BBXS_UNISON_OCTAVE,
    BBXS_UNISON_BOWED,
    BBXS_UNISON_PIANO,
    BBXS_UNISON_WARBLED,
    BBXS_UNISON_COUNT,
} bbxs_unsion_type_e;

typedef enum bbxs_effect_type {
    BBXS_EFFECT_PANNING,
    BBXS_EFFECT_DISTORTION,
    BBXS_EFFECT_BITCRUSHER,
    BBXS_EFFECT_CHORUS,
    BBXS_EFFECT_ECHO,
    BBXS_EFFECT_REVERB,
    BBXS_EFFECT_EQ,
    BBXS_EFFECT_VOLUME,
    BBXS_EFFECT_LIMITER,

    BBXS_EFFECT_COUNT,
} bbxs_effect_type_e;

typedef enum bbxs_noise_type {
    BBXS_NOISE_RETRO,
    BBXS_NOISE_WHITE,
    BBXS_NOISE_CLANG,
    BBXS_NOISE_BUZZ,
    BBXS_NOISE_HOLLOW,
    BBXS_NOISE_SHINE,
    BBXS_NOISE_DEEP,
    BBXS_NOISE_CUTTER,
    BBXS_NOISE_METALLIC,
    BBXS_NOISE_COUNT
} bbxs_noise_type_e;

typedef enum bbxs_filter_type {
    BBXS_FILTER_TYPE_OFF,
    BBXS_FILTER_TYPE_LP,
    BBXS_FILTER_TYPE_HP,
    BBXS_FILTER_TYPE_NOTCH
} bbxs_filter_type_e;

typedef enum bbxs_param_type {
    BBXS_PARAM_UINT8,
    BBXS_PARAM_INT,
    BBXS_PARAM_DOUBLE
} bbxs_param_type_e;

typedef enum bbxs_param_flags {
    BBXS_PARAM_FLAG_NO_AUTOMATION
} bbxs_param_flags_e;

typedef enum bbxs_envelope_compute_index {
    BBXS_ENV_INDEX_NONE,
    BBXS_ENV_INDEX_NOTE_VOLUME,
    BBXS_ENV_INDEX_NOTE_FILTER_ALL_FREQS,
    BBXS_ENV_INDEX_PULSE_WIDTH,
    BBXS_ENV_INDEX_STRING_SUSTAIN,
    BBXS_ENV_INDEX_UNISON,
    BBXS_ENV_INDEX_OPERATOR_FREQ0,
    BBXS_ENV_INDEX_OPERATOR_FREQ1,
    BBXS_ENV_INDEX_OPERATOR_FREQ2,
    BBXS_ENV_INDEX_OPERATOR_FREQ3,
    BBXS_ENV_INDEX_OPERATOR_AMP0,
    BBXS_ENV_INDEX_OPERATOR_AMP1,
    BBXS_ENV_INDEX_OPERATOR_AMP2,
    BBXS_ENV_INDEX_OPERATOR_AMP3,
    BBXS_ENV_INDEX_FEEDBACK_AMP,
    BBXS_ENV_INDEX_PITCH_SHIFT,
    BBXS_ENV_INDEX_DETUNE,
    BBXS_ENV_INDEX_VIBRATO_DEPTH,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ0,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ1,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ2,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ3,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ4,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ5,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ6,
    BBXS_ENV_INDEX_NOTE_FILTER_FREQ7,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN0,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN1,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN2,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN3,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN4,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN5,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN6,
    BBXS_ENV_INDEX_NOTE_FILTER_GAIN7,
    BBXS_ENV_INDEX_SUPERSAW_DYNAMISM,
    BBXS_ENV_INDEX_SUPERSAW_SPREAD,
    BBXS_ENV_INDEX_SUPERSAW_SHAPE,
    BBXS_ENV_INDEX_COUNT
} bbxs_envelope_compute_index_e;

typedef enum bbxs_serialize_status {
    BBXS_SERIALIZE_STATUS_OK,
    BBXS_SERIALIZE_STATUS_NO_DATA,
    BBXS_SERIALIZE_STATUS_INVALID,
    BBXS_SERIALIZE_STATUS_IO_ERROR,
} bbxs_serialize_status_e;

typedef struct bbxs_envelope {
    bbxs_envelope_compute_index_e index;
    uint8_t curve_preset;
} bbxs_envelope_s;

typedef struct bbxs_param_info {
    bbxs_param_type_e type;
    uint32_t flags; // bbxs_param_flags_e

    const char *name;
    const char *group;

#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((nonstring)) // string does not end with NUL terminator
#endif
    char id[8];

    double min_value;
    double max_value;
    double default_value;

    const char **enum_values;
} bbxs_param_info_s;

typedef struct bbxs_vibrato_params {
    double depth; // in beepbox code, this was quantized to increments of 0.04
    double speed;
    int delay; // in ticks, I think?
    uint8_t type;
} bbxs_vibrato_params_s;

typedef struct bbxs_tick_ctx {
    /**
     * The tempo of the transport, in beats per minute.
     *
     * @important Do not set this to zero or a very low value! The rate at which
     *            certain computations are done depends on the transport's
     *            tempo. See the documentation on @ref bbxs_synth_tick for
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
     * The MIDI pitch, in octave 0, of the current key of the song. That is, for
     * the key of C, the base pitch is C0 (12). If you do not have this
     * information, C will suffice.
     *
     * If this is zero, it will be treated as if it were 12.
     *
     * @note This is only used for the bitcrusher effect.
     */
    int base_pitch;

    /**
     * This controls the "note size" envelope source for all participating
     * voices. It is a value in the range of [0, 1]. Full note size is 1.
     *
     * @note This is only relevant when passed to a synth.
     */
    double note_size;

    /**
     * This controls the "mod wheel" envelope source. It is a value in the range
     * [0, 1].
     *
     * @note This is only relevant when passed to a synth.
     **/
    double mod_wheel;
} bbxs_tick_ctx_s;

typedef struct bbxs_complex {
    double real;
    double imag;
} bbxs_complex_s;

typedef struct bbxs_waveform_data {
    /**
     * The expression (volume) of the waveform in the range [0, 1].
     */
    double expression;
    /**
     * Array containing mono samples. The amplitude range of the waveform is not
     * defined except that 0.0 indicates -inf dBFS.
     *
     * Note that there will always be an extra 0.0 as the last sample, and that
     * this extra sample is reflected in the length field.
     */
    float *samples;
    /**
     * The number of samples in the waveform.
     */
    size_t length;
} bbxs_waveform_data_s;

typedef struct bbxs_context bbxs_context_s;
typedef struct bbxs_synth bbxs_synth_s;
typedef struct bbxs_effect bbxs_effect_s;

typedef void *(*bbxs_malloc_f)(size_t size, void *userdata);
typedef void (*bbxs_mfree_f)(void *ptr, void *userdata);

typedef int8_t bbxs_voice_id;

typedef struct bbxs_allocator {
    bbxs_malloc_f alloc;
    bbxs_mfree_f free;
    void *userdata;
} bbxs_allocator_s;

typedef enum bbxs_log_severity {
    BBXS_LOG_DEBUG,
    BBXS_LOG_INFO,
    BBXS_LOG_WARNING,
    BBXS_LOG_ERROR,
    BBXS_LOG_FATAL,
} bbxs_log_severity_e;

typedef void (*bbxs_log_f)(bbxs_log_severity_e severity, const char *msg,
                           void *userdata);

/**
 * @brief Callback to write to a stream.
 *
 * @param data     Pointer to the data which should be written to the stream.
 * @param size     The number of bytes to write.
 * @param userdata An opaque pointer supplied by the user.
 * @return 0 to indicate success. Non-zero on failure.
 */
typedef int (*bbxs_writer_f)(const void *data, size_t size, void *userdata);

/**
 * @brief Callback to read from a stream.
 *
 * @param [in] data Pointer to the buffer which will be written to.
 * @param size      The number of bytes to read.
 * @param userdata  An opaque pointer supplied by the user.
 * @return 0 to indicate that all bytes were successfully read.
           Non-zero otherwise.
 */
typedef int (*bbxs_reader_f)(void *data, size_t size, void *userdata);

typedef struct bbxs_synth_callbacks {
    /**
     * Called when a voice is finished being processed.
     * @param inst Pointer to the instrument.
     * @param id The ID of the voice.
     */
    void (*voice_end)(bbxs_synth_s *inst, bbxs_voice_id id);
} bbxs_synth_callbacks_s;

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
BBXS_API void bbxs_version(uint32_t *major, uint32_t *minor,
                           uint32_t *revision);

/**
 * @brief Create a new context.
 *
 * Creates a new context, which is the first thing you must do in order to use
 * this library. The context is allocated using a pointer to the given
 * allocator, and a copy of the allocator structure will be kept in the context
 * structure for any further allocations. If NULL is given instead, it will use
 * the allocators from the C standard library. A pointer to the newly allocated
 * bbxs_context struct will be returned, or NULL if there was an error.
 *
 * Other than this function, there are a couple of others where allocation or
 * freeing may occur:
 *   - bbxs_synth_new
 *   - bbxs_effect_new
 *   - bbxs_synth_destroy
 *   - bbxs_effect_destroy
 *   - bbxs_synth_set_sample_rate
 *   - bbxs_effect_set_sample_rate
 *
 * @param alloc Pointer to the allocator.
 * @param random_seed Initial seed for the PRNG.
 * @return Pointer to the newly created bbxs_context struct.
 * */
BBXS_API bbxs_context_s *bbxs_context_new(const bbxs_allocator_s *alloc,
                                          uint64_t random_seed);

/**
 * @brief Destroy a context.
 *
 * Destroys and deallocates a given bbxs_context. Afterwards, all remaining
 * instruments or effects created with this context may produce undefined
 * behavior when interacted with.
 *
 * @param ctx The context to destroy.
 */
BBXS_API void bbxs_context_destroy(bbxs_context_s *ctx);

/**
 * @brief Set the log function for the context.
 *
 * Set the log function to be used by the context. The given log function
 * must be thread-safe.
 *
 * @param log_func The log function to use.
 * @param userdata An opaque pointer to be passed to the given log function.
 */
BBXS_API void bbxs_set_log_func(bbxs_context_s *ctx, bbxs_log_f log_func,
                                void *userdata);

/**
 * @brief Obtain the number of parameters for a given instrument type.
 *
 * @param type The BBXS_SYNTH_* enum that identifies the instrument type.
 * @return The number of parameters the instrument type has.
 */
BBXS_API unsigned int bbxs_synth_param_count(bbxs_synth_type_e type);

/**
 * @brief Obtain information for a specific parameter of an instrument type.
 *
 * @param type  The BBXS_SYNTH_* enum that identifies the instrument type.
 * @param index The BBXS_PARAM_* or BBXS_{inst}_PARAM_* enum that
 *              identifies the parameter.
 * @return The bbxs_param_info_s struct containing information about the
 * parameter.
 */
BBXS_API const bbxs_param_info_s *bbxs_synth_param_info(bbxs_synth_type_e type,
                                                        unsigned int index);

/**
 * @brief Allocate a new synth instance of a given type.
 *
 * This allocates a new synth instance of a given type. Since it uses memory
 * allocation functions, this function may return a failure notice.
 *
 * @param ctx       The context to create the synth instance with.
 * @param inst_type The BBXS_SYNTH_* enum that identifies the instrument
 *                  type.
 * @return Pointer to the newly allocated instrument. NULL on error.
 */
BBXS_API bbxs_synth_s *bbxs_synth_new(bbxs_context_s *ctx,
                                      bbxs_synth_type_e inst_type);

/**
 * @brief Free a previously allocated instrument.
 *
 * @param inst Pointer to the instrument struct to free. Does nothing if NULL is
 *             passed.
 */
BBXS_API void bbxs_synth_destroy(bbxs_synth_s *inst);

/**
 * @brief Obtains the type of an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The BBXS_SYNTH_* enum that identifies the instrument type.
 */
BBXS_API bbxs_synth_type_e bbxs_synth_type(const bbxs_synth_s *inst);

/**
 * @brief Get the callback table of the instrument.
 *
 * This will return a pointer to the callback table structure of the instrument
 * These functions are called during inst_tick or inst_run.
 *
 * @param inst Pointer to the instrument.
 * @return Pointer to the bbxs_synth_callbacks_s structure.
 */
BBXS_API bbxs_synth_callbacks_s *
bbxs_synth_get_callback_table(bbxs_synth_s *inst);

/**
 * @brief Obtain the user-set opaque pointer associated with an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The user-set opaque pointer, or NULL if it was not set.
 */
BBXS_API void *bbxs_synth_get_userdata(bbxs_synth_s *inst);

/**
 * @brief Associate an instrument with an opaque pointer.
 *
 * @param inst Pointer to the instrument.
 * @param userdata The pointer to associate with the instrument.
 */
BBXS_API void bbxs_synth_set_userdata(bbxs_synth_s *inst, void *userdata);

/**
 * @brief Set or change the sample rate of a synthesizer instance. May
 *        cause allocation.
 *
 * @note The synth instance should not be modified or examined by another thread
 *       while this function is running!
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BBXS_API void bbxs_synth_set_sample_rate(bbxs_synth_s *inst,
                                         double sample_rate);

/**
 * @brief Signal to an instrument that transport playback has started.
 *
 * This function should be called whenever transport begins. Or in other words,
 * whenever the song's track has started being played.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 * @param beat The beat that the transport started on.
 * @param bpm  The tempo of the transport, in beats per minute.
 */
BBXS_API void bbxs_synth_begin_transport(bbxs_synth_s *inst, double beat,
                                         double bpm);

/**
 * @brief Set an instrument's parameter to an integer value.
 *
 * Set an instrument's parameter to an integer value. The parameter can have any
 * underlying type.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst  Pointer to the instrument.
 * @param param The BBXS_PARAM_* or BBXS_{inst}_PARAM_* enum that
 * identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_synth_set_param_int(bbxs_synth_s *inst, uint32_t param,
                                      int value);

/**
 * @brief Set an instrument's parameter to a floating-point value.
 *
 * Set an instrument's parameter to a double-precision floating-point value. If
 * the underlying type of the parameter is an integer type, this function will
 * fail.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst  Pointer to the instrument.
 * @param param The BBXS_PARAM_* or BBXS_{inst}_PARAM_* enum that
 * identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_synth_set_param_double(bbxs_synth_s *inst, uint32_t param,
                                         double value);

/**
 * @brief Get the value of an instrument's parameter as an integer value.
 *
 * Gets the value of an instrument's parameter as an integer value. If the
 * underlying type of the param is a floating-point type, this function will
 * fail.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param      inst  Pointer to the instrument.
 * @param      param The BBXS_PARAM_* or BBXS_{inst}_PARAM_* enum that
 *                   identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_synth_get_param_int(const bbxs_synth_s *inst, uint32_t param,
                                      int *value);

/**
 * @brief Get the value of an instrument's parameter as a double-precision
 *        floating-point value.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param      inst  Pointer to the instrument.
 * @param      param The BBXS_PARAM_* or BBXS_{inst}_PARAM_* enum that
 *                   identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_synth_get_param_double(const bbxs_synth_s *inst,
                                         uint32_t param, double *value);

/**
 * @brief Get the parameter index of a note effect toggle.
 *
 * This returns the instrument index of the toggle parameter for a given note
 * effect.
 *
 * @param type The BBXS_SYNTHFX_* enum that identifies the note effect type.
 * @return The parameter index on success, and UINT32_MAX on failure.
 */
BBXS_API uint32_t bbxs_synth_effect_toggle_param(bbxs_synthfx_type_e type);

/**
 * @brief Get the list of possible envelope targets for a given instrument type.
 *
 * @param      type The BBXS_SYNTH_* enum that identifies the instrument
 *                  type.
 * @param[out] size Output parameter, giving the size of the output list.
 * @return List of envelope target indices.
 */
BBXS_API const bbxs_envelope_compute_index_e *
bbxs_synth_envelope_targets(bbxs_synth_type_e type, int *size);

/**
 * @brief Get the number of active envelopes for an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The number of active envelopes.
 */
BBXS_API uint8_t bbxs_synth_envelope_count(const bbxs_synth_s *inst);
// note: envelopes are stored contiguously and in order, so it is valid to treat
// the return value as an array.

/**
 * @brief Get the contiguous list of envelope configurations, starting from an
 * index.
 *
 * This returns the pointer to the data for the configuration of an envelope of
 * an instrument. These are stored contiguously and in order, so it is valid to
 * treat the return value as an array.
 *
 * @note It is not safe to modify the contents of the return value outside of
 *       the audio thread.
 *
 * @param inst  Pointer to the instrument.
 * @param index The index to the envelope configuration.
 */
BBXS_API bbxs_envelope_s *bbxs_synth_get_envelope(bbxs_synth_s *inst,
                                                  uint8_t index);

/**
 * @brief Add a new envelope to an instrument.
 *
 * This activates a new envelope for an instrument, and then returns the pointer
 * to its configuration structure.
 *
 * This function will return a failure notice if the number of envelopes will
 * exceed BBXS_MAX_ENVELOPE_COUNT.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 * @return On success, pointer to the envelope configuration structure. NULL on
 * failure.
 */
BBXS_API bbxs_envelope_s *bbxs_synth_add_envelope(bbxs_synth_s *inst);

/**
 * @brief Removes an instrument's envelope.
 *
 * This will remove an active envelope for an instrument, and shift the indices
 * of the other active envelopes to fill the spot.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst  Pointer to the instrument.
 * @param index The index of the envelope to remove
 */
BBXS_API void bbxs_synth_remove_envelope(bbxs_synth_s *inst, uint8_t index);

/**
 * @brief Clears all the active envelopes of an instrument.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 */
BBXS_API void bbxs_synth_clear_envelopes(bbxs_synth_s *inst);

/**
 * @brief Obtain the size of the exstate for a given synth type.
 *
 * @return The exstate size, in bytes. Returns 0 if the instrument type has no
 *         exstate.
 */
BBXS_API size_t bbxs_synth_get_exstate_size(bbxs_synth_type_e type);

/**
 * @brief Copy the exstate of the synth into the provided buffer
 *
 * @note There are no guarantees of atomicity between threads.
 *
 * @param inst         Pointer to the synth instance.
 * @param [out] buffer Pointer to the buffer to write into. Passing in NULL is
 *                     safe: no writes will occur.
 * @return             The size of the synth's exstate.
 **/
BBXS_API size_t bbxs_synth_get_exstate(bbxs_synth_s *inst, void *buffer);

/**
 * @brief Apply exstate data to the given synth.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the synth instance.
 * @param buffer Pointer to the buffer to read from.
 */
BBXS_API void bbxs_synth_set_exstate(bbxs_synth_s *inst, const void *buffer);

/**
 * @brief Serialize exstate and write it to a stream.
 *
 * The written data will be platform-agnostic.
 *
 * @param inst     Pointer to the synth instance.
 * @param writer   Writer callback function.
 * @param userdata Opaque pointer to pass to the writer function.
 * @return Non-zero on success, and zero on failure.
 */
BBXS_API bbxs_serialize_status_e bbxs_synth_serialize_exstate(
    bbxs_synth_s *inst, bbxs_writer_f writer, void *userdata);

/**
 * @brief Deserialize exstate from a stream.
 *
 * @param inst     Pointer to the synth instance.
 * @param reader   Reader callback function.
 * @param userdata Opaque pointer to pass to the reader function.
 * @return Non-zero on success, and zero on failure.
 */
BBXS_API bbxs_serialize_status_e bbxs_synth_deserialize_exstate(
    bbxs_synth_s *inst, bbxs_reader_f reader, void *userdata);

/**
 * @brief Send a note on event to an instrument.
 *
 * Sends a note on event to an instrument.
 *
 * To have the note use "note size" data passed into @ref bbxs_synth_tick,
 * pass in the constant BBXS_USE_NOTE_SIZE as the velocity parameter.
 * Otherwise, the size of the note will be dictated by the given velocity, and
 * will be constant throughout its duration.
 *
 * If the length given is BBXS_NOTE_LENGTH_UNKNOWN, then the note will
 * play until @ref bbxs_synth_end_note is called for the given voice id.
 * Otherwise, the note will end automatically based on the given length.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst     Pointer to the instrument.
 * @param key      The MIDI key of the note.
 * @param velocity The velocity of the note, from 0-1, or the constant
 *                 BBXS_USE_NOTE_SIZE.
 * @param length   The length of the note, in ticks. BBXS_NOTE_LENGTH_UNKNOWN
 *                 if unknown.
 * @returns The ID of the newly created voice.
 */
BBXS_API bbxs_voice_id bbxs_synth_begin_note(bbxs_synth_s *inst, int key,
                                             double velocity, int32_t length);

/**
 * @brief Send a note off event to an instrument.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 * @param id The ID of the note to end.
 */
BBXS_API void bbxs_synth_end_note(bbxs_synth_s *inst, bbxs_voice_id id);

/**
 * @brief Turn off all active notes of an instrument.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 */
BBXS_API void bbxs_synth_end_all_notes(bbxs_synth_s *inst);

/**
 * @brief Choke all active notes.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 */
BBXS_API void bbxs_synth_stop(bbxs_synth_s *inst);

/**
 * @brief Tick an instrument.
 *
 * While computing audio, BeepBox instruments need to be ticked at a rate
 * dependent on the tempo of the song. On each tick, voices will be updated, and
 * envelope/automation values will be calculated, and interpolated across the
 * frames of an audio render.
 *
 * To get the tick rate, use the @ref bbxs_calc_samples_per_tick function.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BBXS_API void bbxs_synth_tick(bbxs_synth_s *inst,
                              const bbxs_tick_ctx_s *tick_ctx);

/**
 * @brief Render the audio of an instrument.
 *
 * This renders the audio of an instrument to a mono audio output buffer of a
 * given size.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst        Pointer to the instrument.
 * @param output      The mono audio output buffer.
 * @param frame_count The number of frames to render to the output buffer.
 */
BBXS_API void bbxs_synth_run(bbxs_synth_s *inst, float *output,
                             size_t frame_count);

/**
 * @brief Obtain the number of parameters for a given effect type.
 *
 * @param type The BBXS_EFFECT_* enum that identifies the effect type.
 * @return The number of parameters the effect type has.
 */
BBXS_API unsigned int bbxs_effect_param_count(bbxs_effect_type_e type);

/**
 * @brief Get channel count info for a given effect type.
 *
 * Obtains the number channels that an effect type receives and outputs,
 * respectively. 1 is mono, and 2 is stereo.
 *
 * @param      type The BBXS_EFFECT_* enum that identifies the effect type.
 * @param[out] inputs The number of input channels for the given type.
 * @param[out] outputs The number of output channels for the given type.
 */
BBXS_API void bbxs_effect_channel_info(bbxs_effect_type_e type, int *inputs,
                                       int *outputs);

/**
 * @brief Obtain information for a specific parameter of an effect type.
 *
 * @param type  The BBXS_EFFECT_* enum that identifies the instrument type.
 * @param index The BBXS_{effect}_PARAM_* enum that identifies the parameter.
 * @return The bbxs_param_info_s struct containing information about the
 * parameter.
 */
BBXS_API const bbxs_param_info_s *
bbxs_effect_param_info(bbxs_effect_type_e type, unsigned int index);

/**
 * @brief Allocate a new effect instance of a given type.
 *
 * This allocates a new effect instance of a given type. Since it uses memory
 * allocation functions, this function may return a failure notice.
 *
 * @param ctx The context to create the effect instance with.
 * @param effect_type The BBXS_EFFECT_* enum that identifies the effect type.
 * @return Pointer to the newly allocated effect instance. NULL on error.
 */
BBXS_API bbxs_effect_s *bbxs_effect_new(bbxs_context_s *ctx,
                                        bbxs_effect_type_e effect_type);

/**
 * @brief Free a previously allocated effect instance.
 *
 * @param effect Pointer to the effect instance struct to free. Does nothing if
 *               NULL is passed.
 */
BBXS_API void bbxs_effect_destroy(bbxs_effect_s *effect);

/**
 * @brief Obtains the type of an effect instance.
 *
 * @param effect Pointer to the effect instance.
 * @return The BBXS_EFFECT_* enum that identifies the instrument type.
 */
BBXS_API bbxs_effect_type_e bbxs_effect_type(const bbxs_effect_s *effect);

/**
 * Obtain the user-set opaque pointer associated with an effect instance.
 *
 * @param effect Pointer to the effect instance.
 * @return The user-set opaque pointer, or NULL if it was not set.
 */
BBXS_API void *bbxs_effect_get_userdata(bbxs_effect_s *effect);

/**
 * Associate an instrument with an opaque pointer.
 *
 * @param effect   Pointer to the effect instance.
 * @param userdata The pointer to associate with the effect instance.
 */
BBXS_API void bbxs_effect_set_userdata(bbxs_effect_s *effect, void *userdata);

/**
 * @brief Set or change the sample rate of an effect instance. May cause
 *        allocation.
 *
 * @note The synth instance should not be modified or examined by another thread
 *       while this function is running!
 *
 * @param effect      Pointer to the effect instance.
 * @param sample_rate The sample rate in Hz.
 */
BBXS_API void bbxs_effect_set_sample_rate(bbxs_effect_s *effect,
                                          double sample_rate);

// /**
//  * @brief Signal to an effect instance that transport playback has started.
//  *
//  * This function should be called whenever transport begins. Or in other
//  * words, whenever the song's track has started being played.
//  *
//  * @param effect Pointer to the effect instance.
//  * @param beat   The beat that the transport started on.
//  * @param bpm    The tempo of the transport, in beats per minute.
//  */
// BBXS_API void bbxs_effect_begin_transport(bbxs_effect_s *effect,
//                                                 double beat, double bpm);

/**
 * @brief Set an effect instance's parameter to an integer value.
 *
 * Set an effect instance's parameter to an integer value. The parameter can
 * have any underlying type.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param effect Pointer to the effect instance.
 * @param param  The BBXS_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param value  The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_effect_set_param_int(bbxs_effect_s *effect, uint32_t param,
                                       int value);

/**
 * @brief Set an effect's parameter to a floating-point value.
 *
 * Set an effect's parameter to a double-precision floating-point value. If the
 * underlying type of the parameter is an integer type, this function will
 * fail.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param effect Pointer to the effect instance.
 * @param param  The BBXS_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param value  The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_effect_set_param_double(bbxs_effect_s *inst, uint32_t param,
                                          double value);

/**
 * @brief Get the value of an effect instance's parameter as an integer value.
 *
 * Gets the value of an effect instance's parameter as an integer value. If the
 * underlying type of the param is a floating-point type, this function will
 * fail.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param      effect Pointer to the effect instance.
 * @param      param  The BBXS_{effect}_PARAM_* enum that identifies the
 *                    parameter.
 * @param[out] value  The output value.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_effect_get_param_int(const bbxs_effect_s *inst,
                                       uint32_t param, int *value);

/**
 * @brief Get the value of an effect instance's parameter as a floating-point
 * value.
 *
 * Gets the value of an effect instance's parameter as a double-
 * floating point value.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param      effect Pointer to the effect instance.
 * @param      param  The BBXS_{effect}_PARAM_* enum that identifies the
 * parameter.
 * @param[out] value  The output value.
 * @return 0 on success, and 1 on failure.
 */
BBXS_API int bbxs_effect_get_param_double(const bbxs_effect_s *effect,
                                          uint32_t param, double *value);

/**
 * @brief Clear any feedback/delay systems.
 *
 * Clears any feedback or delay systems the effect is using. It has the
 * effect of removing any potential tail that the effect might produce at
 * the moment of the call.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst Pointer to the instrument.
 */
BBXS_API void bbxs_effect_stop(bbxs_effect_s *inst);

/**
 * @brief Tick an effect instance.
 *
 * While computing audio, BeepBox effects need to be ticked at a rate dependent
 * on the tempo of the song. Values will be calculated on each tick and
 * interpolated along the individual frames of an audio render.
 *
 * To get the tick rate, use the @ref bbxs_calc_samples_per_tick function.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param inst     Pointer to the instrument.
 * @param tick_ctx Pointer to a bbxs_tick_ctx structure.
 */
BBXS_API void bbxs_effect_tick(bbxs_effect_s *effect,
                               const bbxs_tick_ctx_s *tick_ctx);
// mono output

/**
 * @brief Run an effect instance.
 *
 * This takes in a mono or stereo audio buffer and performs audio processing
 * in-place. If it is a mono effect, only the left channel is used. If it is
 * a stereo effect, both channels are used.
 *
 * @note It is only safe to call this function from the audio thread.
 *
 * @param effect      Pointer to the effect instance.
 * @param buffer      Pointer to two audio buffers, for the left and right
                      channels respectively.
 * @param frame_count The number of frames in both the input and output buffer.
 */
BBXS_API void bbxs_effect_run(bbxs_effect_s *effect, float **buffer,
                              size_t frame_count);

/**
 * @brief Get the name of an envelope.
 *
 * Given the index of an envelope target, this returns the name of the target as
 * a pointer to a a null-terminated UTF8-encoded string. The caller must not
 * modify this string.
 *
 * @param inst The BBXS_ENV_INDEX_* enum that identifies the envelope target.
 * @return The C string on success, and NULL on error.
 */
BBXS_API const char *
bbxs_envelope_index_name(bbxs_envelope_compute_index_e index);

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
BBXS_API const char **bbxs_envelope_curve_preset_names(void);

/**
 * @brief Calculates the number of audio frames per tick.
 *
 * This calculates the number of audio frames that need to be computed
 * in-between each call to bbxs_synth_tick, given the tempo of the transport
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
BBXS_API double bbxs_calc_samples_per_tick(double bpm, double sample_rate);

/**
 * @brief Get the number of ticks it takes for an instrument to fade out.
 *
 * This calculates the number of ticks it takes for an instrument to fade out,
 * given the value of the fade out parameter of an instrument.
 *
 * If you know the length of each note, and the result of this is a negative
 * value, call bbxs_synth_end_note that positive number of ticks before the
 * note actually ends.
 *
 * @param inst    Pointer to the instrument.
 * @param setting The value of the fade-out parameter of a (hypothetical)
 * instrument.
 * @return The signed fade-out length in ticks, as a double.
 */
BBXS_API double bbxs_ticks_fade_out(double setting);

/**
 * @brief Get the vibrato parameters of a vibrato preset.
 *
 * @param      preset The BBXS_VIBRATO_PRESET_* enum that identifies the
 * vibrato preset.
 * @param[out] params Pointer to an output structure where the vibrato
 * parameters will be written.
 */
BBXS_API void bbxs_vibrato_preset_params(bbxs_vibrato_preset_e preset,
                                         bbxs_vibrato_params_s *params);

/**
 * @brief Convert the frequency setting of a hypothetical filter to Hz.
 *
 * This converts the frequency units of BeepBox audio filters to Hz.
 *
 * @param freq_setting The frequency setting of a hypothetical BeepBox filter.
 * @return The frequency in Hz.
 */
BBXS_API double bbxs_freq_setting_to_hz(double freq_setting);

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
BBXS_API double bbxs_linear_gain_to_setting(double gain);

/**
 * @brief Analyze the frequency response of a filter.
 *
 * This returns the frequency response data of a control point of an EQ or note
 * filter effect at the given frequency, as a complex number. The linear gain at
 * that frequency is then represented by the magnitude of that complex number.
 *
 * @param      filter_type  The BBXS_FILTER_TYPE_* enum identifying the type
 *                          of the filter control point.
 * @param      freq_setting The frequency of the control point in the BeepBox
 *                          setting unit.
 * @param      gain_setting The gain/volume of the control point in the BeepBox
 *                          gain setting unit.
 * @param      hz           The frequency to analyze, in HZ.
 * @param      sample_rate  The sample rate to reference with.
 * @param[out] out          The output complex number
 */
BBXS_API void bbxs_analyze_freq_response(bbxs_filter_type_e filter_type,
                                         double freq_setting,
                                         double gain_setting, double hz,
                                         double sample_rate,
                                         bbxs_complex_s *out);

/**
 * @brief Obtain a null-terminated array of chip waveform names.
 *
 * @return The array of chip waveform names. The last element will always be
 *         NULL.
 */
BBXS_API const char *const *bbxs_get_chip_waveform_names(void);

/**
 * @brief Obtain a null-terminated array of noise waveform names.
 *
 * @return The array of noise waveform names. The last element will always be
 *         NULL.
 */
BBXS_API const char *const *bbxs_get_noise_waveform_names(void);

/**
 * @brief Obtain the data for a chip waveform.
 *
 * @note This function's behavior is undefined if the index given is invalid.
 *
 * @param       ctx        The BeepBox synth context.
 * @param       index      The chip waveform index.
 * @param       integrated True if the integrated wave should be fetch, false if
 *                         it should be the original.
 * @param [out] data       Pointer to waveform data structure which will be
 *                         filled.
 */
BBXS_API void bbxs_get_chip_waveform(bbxs_context_s *ctx, int index,
                                     int integrated,
                                     bbxs_waveform_data_s *data);

/**
 * @brief Obtain the data for a noise waveform.
 *
 * @note This function's behavior is undefined if the index given is invalid.
 *
 * @param       ctx        The BeepBox synth context.
 * @param       index      The noise waveform index.
 * @param [out] data       Pointer to waveform data structure which will be
 *                         filled.
 */
BBXS_API void bbxs_get_noise_waveform(bbxs_context_s *ctx, int index,
                                      bbxs_waveform_data_s *data);

#ifdef __cplusplus
}
#endif

#endif