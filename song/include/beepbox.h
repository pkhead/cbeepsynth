#ifndef _beepbox_song_h_
#define _beepbox_song_h_

#include <beepbox_synth.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#   if defined(BPBX_SHARED) && defined(BPBX_SHARED_IMPORT)
#       define BPBX_API __declspec(dllimport)
#   elif defined(BPBX_SHARED)
#       define BPBX_API __declspec(dllexport)
#   else
#       define BPBX_API
#   endif
#else
#   ifdef BPBX_SHARED
#       define BPBX_API __attribute__((visibility("default")))
#   else
#       define BPBX_API
#   endif
#endif

#include <stdbool.h>
#include <stddef.h>

#define BPBX_CHANNEL_MAX_INSTRUMENTS 10
#define BPBX_MAX_PITCH_CHANNEL_COUNT 60
#define BPBX_MAX_DRUM_CHANNEL_COUNT 60
#define BPBX_MAX_MOD_CHANNEL_COUNT 60

typedef enum {
    BPBX_CHANNEL_PITCH,
    BPBX_CHANNEL_NOISE,
    BPBX_CHANNEL_MOD
} bpbx_channel_type_e;

typedef struct bpbx_channel_inst_s {
    bpbx_synth_s *synth;
} bpbx_channel_inst_s;

typedef struct bpbx_pattern_s {
    uint8_t instrument_mask;
} bpbx_pattern_s;

typedef struct bpbx_channel_s {
    uint8_t channel_type; // BPBX_CHANNEL_*
    char name[16];
    uint8_t octave;

    size_t instrument_count;
    bpbx_channel_inst_s *instruments[BPBX_CHANNEL_MAX_INSTRUMENTS];

    bpbx_pattern_s *patterns; // length = song's pattern count
    uint16_t *track; // length = song's length 
} bpbx_channel_s;

typedef struct bpbx_song_s {
    uint8_t pitch_channel_count; // max channel count: 60
    uint8_t noise_channel_count; // max channel count: 60
    uint8_t mod_channel_count; // max channel count: 60

    uint8_t key;
    uint8_t beats_per_bar;

    uint16_t loop_start;
    uint16_t loop_end;
    uint16_t tempo;
    uint16_t length; // length of song in bars
    uint16_t pattern_count;

    bool layered_instruments;
    bool pattern_instruments;

    bpbx_channel_s *channels;
    char *title;
} bpbx_song_s;

typedef enum {
    BPBX_RESIZE_FROM_START,
    BPBX_RESIZE_FROM_END
} bpbx_resize_mode_e;

typedef enum {
    BPBX_STATUS_OK,
    BPBX_STATUS_WARNING,
    BPBX_STATUS_ERROR
} bpbx_song_load_status_e;

// for loading song data of a specific mod, since there may be 
// variant prefix conflicts. For now, there are none, so
// BPBX_VARIANT_AUTO should work perfectly fine.
typedef enum {
    BPBX_VARIANT_AUTO,
    BPBX_VARIANT_BEEPBOX,
    BPBX_VARIANT_JUMMBOX,
} bpbx_song_data_variant_e;

typedef int64_t (*bpbx_reader_f)(size_t size, void *out, void *userdata);

BPBX_API bool bpbx_song_init(bpbx_song_s *song,
    uint8_t pitch_channel_count, uint8_t drum_channel_count, uint8_t mod_channel_count,
    uint16_t length, uint16_t pattern_count);

BPBX_API bool bpbx_song_init_default(bpbx_song_s *song);
BPBX_API uint8_t bpbx_song_load_base64(bpbx_song_s *song, bpbx_song_data_variant_e variant, bpbx_reader_f reader, void *userdata);
BPBX_API void bpbx_song_free(bpbx_song_s *song);

BPBX_API uint16_t bpbx_song_channel_count(const bpbx_song_s *song);
BPBX_API bool bpbx_song_set_length(bpbx_song_s *song, uint16_t new_length, bpbx_resize_mode_e resize_mode);
BPBX_API bool bpbx_song_set_pitch_channel_count(bpbx_song_s *song, uint8_t new_count);
BPBX_API bool bpbx_song_set_pattern_count(bpbx_song_s *song, uint16_t new_count);

BPBX_API bool bpbx_pitch_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song);
BPBX_API bool bpbx_noise_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song);
BPBX_API bool bpbx_mod_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song);
BPBX_API void bpbx_channel_free(bpbx_channel_s *channel);


#ifdef __cplusplus
}
#endif

#endif