#include "../include/beepbox.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

bool bpbx_song_init(bpbx_song_s *song,
    uint8_t pitch_channel_count, uint8_t noise_channel_count, uint8_t mod_channel_count,
    uint16_t length, uint16_t pattern_count)
{
    *song = (bpbx_song_s) {
        .pitch_channel_count = pitch_channel_count,
        .noise_channel_count = noise_channel_count,
        .mod_channel_count = mod_channel_count,

        .beats_per_bar = 8,

        .loop_start = 0,
        .loop_end = 3,
        .tempo = 150,
        .length = length,
        .pattern_count = pattern_count,
    };

    uint16_t channel_count = pitch_channel_count + noise_channel_count + mod_channel_count;
    song->channels = malloc(channel_count * sizeof(bpbx_channel_s));
    if (!song->channels) {
        song->pitch_channel_count = 0;
        song->noise_channel_count = 0;
        song->mod_channel_count = 0;

        return false;
    } else {
        uint8_t ch = 0;
        for (uint8_t i = 0; i < song->pitch_channel_count; ++i) {
            bpbx_pitch_channel_init(&song->channels[ch++], song);
        }

        for (uint8_t i = 0; i < song->noise_channel_count; ++i) {
            bpbx_noise_channel_init(&song->channels[ch++], song);
        }

        for (uint8_t i = 0; i < song->mod_channel_count; ++i) {
            bpbx_mod_channel_init(&song->channels[ch++], song);
        }
    }

    return true;
}

bool bpbx_song_init_default(bpbx_song_s *song) {
    return bpbx_song_init(song, 3, 1, 0, 32, 8);
}

void bpbx_song_free(bpbx_song_s *song) {
    uint16_t channel_count = bpbx_song_channel_count(song);
    for (uint8_t i = 0; i < channel_count; ++i) {
        bpbx_channel_free(&song->channels[i]);
    }

    free(song->channels);
    song->channels = NULL;
    song->pitch_channel_count = 0;
    song->noise_channel_count = 0;
    song->mod_channel_count = 0;
}

uint16_t bpbx_song_channel_count(const bpbx_song_s *song) {
    return song->pitch_channel_count + song->noise_channel_count + song->mod_channel_count;
}

static bool channel_init(bpbx_channel_s *channel, const bpbx_song_s *song) {
    *channel = (bpbx_channel_s) {
        .name = "",
        .instrument_count = 1,

        .patterns = malloc(song->pattern_count * sizeof(bpbx_pattern_s)),
        .track = malloc(song->length * sizeof(uint16_t))
    };

    if (!channel->patterns) return false;
    if (!channel->track)    return false;
    return true;
}

bool bpbx_pitch_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song) {
    if (!channel_init(channel, song)) return false;
    channel->channel_type = BPBX_CHANNEL_PITCH;
    return true;
}

bool bpbx_noise_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song) {
    if (!channel_init(channel, song)) return false;
    channel->channel_type = BPBX_CHANNEL_NOISE;
    return true;
}

bool bpbx_mod_channel_init(bpbx_channel_s *channel, const bpbx_song_s *song) {
    if (!channel_init(channel, song)) return false;
    channel->channel_type = BPBX_CHANNEL_MOD;
    return true;
}

void bpbx_channel_free(bpbx_channel_s *channel) {
    free(channel->patterns);
    free(channel->track);
    channel->patterns = NULL;
    channel->track = NULL;
}

bool bpbx_song_set_length(bpbx_song_s *song, uint16_t new_length, bpbx_resize_mode_e resize_mode) {
    assert(false && "not yet implemented");
    return false;
}

BPBX_API bool bpbx_song_set_pitch_channel_count(bpbx_song_s *song, uint8_t new_count) {
    assert(false && "not yet implemented");
    return false;
}

BPBX_API bool bpbx_song_set_pattern_count(bpbx_song_s *song, uint16_t new_count) {
    assert(false && "not yet implemented");
    return false;
}

// bool bpbx_song_set_pitch_channel_count(bpbx_song_s *song, uint8_t new_count) {
//     if (new_count == song->pitch_channel_count)
//         return true;

//     if (new_count < song->pitch_channel_count) {
//         // shrink
//         for (uint8_t i = new_count; i < song->pitch_channel_count; ++i) {
//             bpbx_pitch_channel_free(&song->pitch_channels[i]);
//         }

//         bpbx_pitch_channel_s *new_alloc = malloc(new_count * sizeof(bpbx_pitch_channel_s));
//         if (!new_alloc) return false;
//         memcpy(new_alloc, song->pitch_channels, new_count * sizeof(bpbx_pitch_channel_s));

//         free(song->pitch_channels);
//         song->pitch_channel_count = new_count;
//         song->pitch_channels = new_alloc;
//     } else {
//         // expand
//         bpbx_pitch_channel_s *new_alloc = malloc(new_count * sizeof(bpbx_pitch_channel_s));
//         for (uint8_t i = song->pitch_channel_count; i < new_count; ++i) {
//             bpbx_pitch_channel_init(&new_alloc[i], song);
//         }
//         memcpy(new_alloc, song->pitch_channels, song->pitch_channel_count * sizeof(bpbx_pitch_channel_s));

//         free(song->pitch_channels);
//         song->pitch_channel_count = new_count;
//         song->pitch_channels = new_alloc;
//     }

//     return true;
// }