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
#define BPBX_VERSION_MINOR 2
// to get the revision, call bpbx_version

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
    /**
     * The tempo of the transport, in beats per minute.
     *
     * @important Do not set this to zero or a very low value! The rate at which certain
     *            computations are done depends on the transport's tempo. See the documentation on
     *            @ref bpbx_inst_tick for more information.
     *
     * If you don't have tempo information, set it to a placeholder value like 150.
     **/
    double bpm;

    // beat must be continuously increasing in order for
    // the tremolo envelopes to work properly!
    // if you don't have that information or the song isn't playing,
    // simply constantly increase this by the bpm.

    /**
     * The current beat position of the transport.
     * @important This must be continuously increasing in order for the tremolo envelopes to work properly!
                  This must be accurately and constantly increased by the tempo of the transport.
     **/
    double beat;
    
    /**
     * This is basically essentially the "note size" parameter in BeepBox.
     * It is a value from 0 to 1. The resting value is 1.
     **/
    double mod_wheel;
} bpbx_tick_ctx_s;

typedef struct {
    double real;
    double imag;
} bpbx_complex_s;

typedef struct bpbx_inst_s bpbx_inst_s;

typedef void* (*bpbx_malloc_f)(size_t size, void *userdata);
typedef void (*bpbx_mfree_f)(void *ptr, void *userdata);

/**
 * @brief Obtain the version of the library.
 *
 * This will write the major, minor, and revision numbers of the current version
 * of cbeepsynth into the given output parameters.
 *
 * @param[out] major Major version number
 * @param[out] minor Minor version number
 * @param[out] revision Revision version number
 */
BEEPBOX_API void bpbx_version(uint32_t *major, uint32_t *minor, uint32_t *revision);

/**
 * @brief Set custom allocators.
 *
 * Use this function if you want cbeepsynth to use a custom allocator instead of
 * the one in the C standard library. Note that these allocator functions will only
 * be called in bpbx_inst_new and bpbx_inst_destroy, so they don't need to be thread-safe.
 *
 * @param alloc The function to allocate a new block of memory.
 * @param free The function to free an allocated block of memory.
 * @param userdata An opaque pointer passed to the two allocation functions.
 */
BEEPBOX_API void bpbx_set_allocator(bpbx_malloc_f alloc, bpbx_mfree_f free, void *userdata);

/**
 * @brief Obtain the number of parameters for a given instrument type.
 *
 * @param type The BPBX_INSTRUMENT_* enum that identifies the instrument type.
 * @return The number of parameters the instrument type has.
 */
BEEPBOX_API unsigned int bpbx_param_count(bpbx_inst_type_e type);

/**
 * @brief Obtain information for a specific parameter of an instrument type.
 *
 * @param type The BPBX_INSTRUMENT_* enum that identifies the instrument type.
 * @param index The BPBX_PARAM_* or BPBX_{inst}_PARAM_* enum that identifies the parameter.
 * @return The bpbx_inst_param_info_s struct containing information about the parameter.
 */
BEEPBOX_API const bpbx_inst_param_info_s* bpbx_param_info(bpbx_inst_type_e type, unsigned int index);

/**
 * @brief Allocate a new instrument of a given type.
 *
 * This allocates a new insturment of a given type. Since it uses memory allocation functions,
 * this function may return a failure notice.
 *
 * @param inst_type The BPBX_INSTRUMENT_* enum that identifies the instrument type.
 * @return Pointer to the newly allocated instrument. NULL on error.
 */
BEEPBOX_API bpbx_inst_s* bpbx_inst_new(bpbx_inst_type_e inst_type);

/**
 * @brief Free a previously allocated instrument.
 *
 * @param inst Pointer to the instrument struct to free. Does nothing if NULL is passed.
 */
BEEPBOX_API void bpbx_inst_destroy(bpbx_inst_s* inst);

/**
 * @brief Obtains the type of an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The BPBX_INSTRUMENT_* enum that identifies the instrument type.
 */
BEEPBOX_API bpbx_inst_type_e bpbx_inst_type(const bpbx_inst_s *inst);

/**
 * @brief Set the sample rate of an instrument.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BEEPBOX_API void bpbx_inst_set_sample_rate(bpbx_inst_s *inst, double sample_rate);

/**
 * @brief Signal to an instrument that transport playback has started.
 *
 * This function should be called whenever transport begins. Or in other words,
 * whenever the song's track has started being played.
 *
 * @param inst Pointer to the instrument.
 * @param beat The beat that the transport started on.
 * @param bpm The tempo of the transport, in beats per minute.
 */
BEEPBOX_API void bpbx_inst_begin_transport(bpbx_inst_s *inst, double beat, double bpm);

/**
 * @brief Set an instrument's parameter to an integer value.
 *
 * Set an instrument's parameter to an integer value. The parameter can have any
 * underlying type.
 *
 * @param inst Pointer to the instrument.
 * @param param The BPBX_PARAM_* or BPBX_{inst}_PARAM_* enum that identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BEEPBOX_API int bpbx_inst_set_param_int(bpbx_inst_s* inst, uint32_t param, int value);

/**
 * @brief Set an instrument's parameter to a floating-point value.
 *
 * Set an instrument's parameter to a double-precision floating-point value. If the underlying
 * type of the parameter is an integer type, this function will fail.
 *
 * @param inst Pointer to the instrument.
 * @param param The BPBX_PARAM_* or BPBX_{inst}_PARAM_* enum that identifies the parameter.
 * @param value The value to set the parameter to.
 * @return 0 on success, and 1 on failure.
 */
BEEPBOX_API int bpbx_inst_set_param_double(bpbx_inst_s* inst, uint32_t param, double value);

/**
 * @brief Get the value of an instrument's parameter as an integer value.
 *
 * Gets the value of an instrument's parameter as an integer value. If the underlying
 * type of the param is a floating-point type, this function will fail.
 *
 * @param      inst Pointer to the instrument.
 * @param      param The BPBX_PARAM_* or BPBX_{inst}_PARAM_* enum that identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BEEPBOX_API int bpbx_inst_get_param_int(const bpbx_inst_s* inst, uint32_t param, int *value);

/**
 * @brief Get the value of an instrument's parameter as a floating-point value.
 *
 * Gets the value of an instrument's parameter as a double-precision floating point value.
 *
 * @param      inst Pointer to the instrument.
 * @param      param The BPBX_PARAM_* or BPBX_{inst}_PARAM_* enum that identifies the parameter.
 * @param[out] value The output value.
 * @return 0 on success, and 1 on failure.
 */
BEEPBOX_API int bpbx_inst_get_param_double(const bpbx_inst_s* inst, uint32_t param, double *value);

/**
 * @brief Get the parameter index of a note effect toggle.
 *
 * This returns the instrument index of the toggle parameter for a given note effect.
 *
 * @param type The BPBX_INSTFX_* enum that identifies the note effect type.
 * @return The parameter index on success, and UINT32_MAX on failure.
 */
BEEPBOX_API uint32_t bpbx_effect_toggle_param(bpbx_instfx_type_e type);

/**
 * @brief Get the name of an envelope.
 *
 * Given the index of an envelope target, this returns the name of the target as
 * a pointer to a a null-terminated UTF8-encoded string. The caller must not
 * modify this string.
 *
 * @param inst The BPBX_ENV_INDEX_* enum that identifies the envelope target.
 * @return The C string on success, and NULL on error.
 */
BEEPBOX_API const char* bpbx_envelope_index_name(bpbx_envelope_compute_index_e index);

// returns the array of curve preset names.
// the array is terminated by a null pointer.

/**
 * @brief Get the null-terminated list of names for each curve preset.
 *
 * This returns a list containing the names of each envelope curve preset.
 * Each name is given with a pointer to a null-terminated UTF8-encoded string.
 * The end of the list is denoted by a NULL pointer instead of a pointer to a string.
 * The caller must not modify the return value of this function.
 *
 * @return The null-terminated list of names.
 */
BEEPBOX_API const char** bpbx_envelope_curve_preset_names(void);

/**
 * @brief Get the list of possible envelope targets for a given instrument type. *
 *
 * @param      type The BPBX_INSTRUMENT_* enum that identifies the instrument type.
 * @param[out] size Output parameter, giving the size of the output list.
 * @return List of envelope target indices.
 */
BEEPBOX_API const bpbx_envelope_compute_index_e* bpbx_envelope_targets(bpbx_inst_type_e type, int *size);

/**
 * @brief Get the number of active envelopes for an instrument.
 *
 * @param inst Pointer to the instrument.
 * @return The number of active envelopes.
 */
BEEPBOX_API uint8_t bpbx_inst_envelope_count(const bpbx_inst_s *inst);
// note: envelopes are stored contiguously and in order, so it is valid to treat the return value
// as an array.

/**
 * @brief Get the contiguous list of envelope configurations, starting from an index.
 *
 * This returns the pointer to the data for the configuration of an envelope of an
 * instrument. These are stored contiguously and in order, so it is valid to treat the
 * return value asn array.
 *
 * @param inst Pointer to the instrument.
 * @param index The index to the envelope configuration.
 */
BEEPBOX_API bpbx_envelope_s* bpbx_inst_get_envelope(bpbx_inst_s *inst, uint8_t index);

/**
 * @brief Add a new envelope to an instrument.
 *
 * This activates a new envelope for an instrument, and then returns the pointer
 * to its configuration structure.
 *
 * This function will return a failure notice if the number of envelopes will
 * exceed BPBX_MAX_ENVELOPE_COUNT.
 *
 * @param inst Pointer to the instrument.
 * @return On success, pointer to the envelope configuration structure. NULL on failure.
 */
BEEPBOX_API bpbx_envelope_s* bpbx_inst_add_envelope(bpbx_inst_s *inst);

/**
 * @brief Removes an instrument's envelope.
 *
 * This will remove an active envelope for an instrument, and shift the indices
 * of the other active envelopes to fill the spot.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The index of the envelope to remove
 */
BEEPBOX_API void bpbx_inst_remove_envelope(bpbx_inst_s *inst, uint8_t index);

/**
 * @brief Clears all the active envelopes of an instrument.
 *
 * @param inst Pointer to the instrument.
 */
BEEPBOX_API void bpbx_inst_clear_envelopes(bpbx_inst_s *inst);

/**
 * @brief Send a note on event to an instrument.
 *
 * @param inst Pointer to the instrument.
 * @param key The MIDI key of the note.
 * @param velocity The velocity of the note, from 0-127.
 */
BEEPBOX_API void bpbx_inst_begin_note(bpbx_inst_s *inst, int key, int velocity);

/**
 * @brief Send a note off event to an instrument.
 *
 * @note The velocity parameter is non-functional.
 * @param inst Pointer to the instrument.
 * @param key The MIDI key of the note to end.
 * @param velocity The velocity of the note, from 0-127.
 */
BEEPBOX_API void bpbx_inst_end_note(bpbx_inst_s *inst, int key, int velocity);

// if you know the length of each note, and the result of this is negative,
// call midi_off that positive number of ticks before the note actually ends.

/**
 * @brief Get the number of ticks it takes for an instrument to fade out.
 *
 * This calculates the number of ticks it takes for an instrument to fade out,
 * given the value of the fade out parameter of an instrument.
 *
 * If you know the length of each note, and the result of this is a negative value,
 * call bpbx_inst_end_note that positive number of ticks before the note actually ends.
 *
 * @param inst Pointer to the instrument.
 * @param setting The value of the fade-out parameter of a (hypothetical) instrument.
 * @return The signed fade-out length in ticks, as a double.
 */
BEEPBOX_API double bpbx_ticks_fade_out(double setting);

/**
 * @brief Get the vibrato parameters of a vibrato preset.
 *
 * @param      preset The BPBX_VIBRATO_PRESET_* enum that identifies the vibrato preset.
 * @param[out] params Pointer to an output structure where the vibrato parameters will be written.
 */
BEEPBOX_API void bpbx_vibrato_preset_params(bpbx_vibrato_preset_e preset, bpbx_vibrato_params_s *params);

/**
 * @brief Calculates the number of audio frames per tick.
 *
 * This calculates the number of audio frames that need to be computed in-between
 * each call to bpbx_inst_tick, given the tempo of the transport and the audio
 * output's sample rate.
 *
 * If you do not have a transport, or do not know its tempo, it is fine to use a
 * placeholder value such as 150 bpm.
 *
 * @param inst Pointer to the instrument.
 * @param bpm The tempo of the transport, in beats per minute.
 * @param sample_rate The sample rate of the audio output.
 * @return The samples per tick, as a double.
 */
BEEPBOX_API double bpbx_calc_samples_per_tick(double bpm, double sample_rate);

/**
 * @brief Tick an instrument.
 *
 * While computing audio, BeepBox instruments need to be ticked at a rate dependent
 * on the tempo of the song. On each tick, voices will be updated, and envelope/automation
 * values will be calculated, and interpolated across the frames of an audio render.
 *
 * To get the tick rate, use the bpbx_calc_samples_per_tick function.
 *
 * @param inst Pointer to the instrument.
 * @param sample_rate The sample rate in Hz.
 */
BEEPBOX_API void bpbx_inst_tick(bpbx_inst_s *inst, const bpbx_tick_ctx_s *tick_ctx);
// mono output

/**
 * @brief Render the audio of an instrument.
 *
 * This renders the audio of an instrument to a mono audio output buffer of a
 * given size.
 * 
 * @param inst Pointer to the instrument.
 * @param out_samples The mono audio output buffer.
 * @param frame_count The number of frames to render to the output buffer.
 */
BEEPBOX_API void bpbx_inst_run(bpbx_inst_s* inst, float *out_samples, size_t frame_count);

/**
 * @brief Convert the frequency setting of a hypothetical filter to Hz.
 *
 * This converts the frequency units of BeepBox audio filters to Hz.
 *
 * @param freq_setting The frequency setting of a hypothetical BeepBox filter.
 * @return The frequency in Hz.
 */
BEEPBOX_API double bpbx_freq_setting_to_hz(double freq_setting);

/**
 * @brief Convert a linear gain value to the units for the gain/volume setting of a filter.
 *
 * This converts a linear gain multiplier to the unit used for the gain/volume setting of
 * a BeepBox filter's control point.
 *
 * @param gain Linear gain.
 * @return The same gain represented in the unit of the gain/volume of a filter.
 */
BEEPBOX_API double bpbx_linear_gain_to_setting(double gain);

/**
 * @brief Analyze the frequency response of a filter.
 *
 * This returns the frequency response data of a control point of an EQ or note filter
 * effect at the given frequency, as a complex number. The linear gain at that frequency is then
 * represented by the magnitude of that complex number.
 *
 * @param      filter_type The BPBX_FILTER_TYPE_* enum identifying the type of the filter control point.
 * @param      freq_setting The frequency of the control point in the BeepBox frequency unit.
 * @param      gain_setting The gain/volume of the control point in the BeepBox gain unit.
 * @param      hz The frequency to analyze, in HZ.
 * @param      sample_rate The sample rate to reference with.
 * @param[out] The output complex number
 */
BEEPBOX_API void bpbx_analyze_freq_response(
    bpbx_filter_type_e filter_type, double freq_setting, double gain_setting,
    double hz, double sample_rate, bpbx_complex_s *out);

#ifdef __cplusplus
}
#endif

#endif