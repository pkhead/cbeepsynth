#include "../include/beepbox.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static uint8_t channel_init(bpbx_channel_s *channel, const bpbx_song_s *song) {
    *channel = (bpbx_channel_s) {
        .name = "",
        .instrument_count = 1,

        .patterns = malloc(song->pattern_count * sizeof(bpbx_pattern_s)),
        .track = malloc(song->length * sizeof(uint16_t))
    };

    if (!channel->patterns) return BPBX_STATUS_MEMERR;
    if (!channel->track)    return BPBX_STATUS_MEMERR;
    return BPBX_STATUS_OK;
}

static void channel_free(bpbx_channel_s *channel) {
    free(channel->patterns);
    free(channel->track);
    channel->patterns = NULL;
    channel->track = NULL;
}

uint8_t bpbx_song_init(bpbx_song_s *song,
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
        return BPBX_STATUS_MEMERR;
    } else {
        uint8_t ch = 0;
        for (uint8_t i = 0; i < song->pitch_channel_count; ++i) {
            if (channel_init(&song->channels[ch], song) == BPBX_STATUS_MEMERR)
                return BPBX_STATUS_MEMERR;
            song->channels[ch].channel_type = BPBX_CHANNEL_PITCH;
            ++ch;
        }

        for (uint8_t i = 0; i < song->noise_channel_count; ++i) {
            if (channel_init(&song->channels[ch], song) == BPBX_STATUS_MEMERR)
                return BPBX_STATUS_MEMERR;
            song->channels[ch].channel_type = BPBX_CHANNEL_NOISE;
            ++ch;
        }

        for (uint8_t i = 0; i < song->mod_channel_count; ++i) {
            if (channel_init(&song->channels[ch], song) == BPBX_STATUS_MEMERR)
                return BPBX_STATUS_MEMERR;
            song->channels[ch].channel_type = BPBX_CHANNEL_MOD;
            ++ch;
        }
    }

    return BPBX_STATUS_OK;
}

uint8_t bpbx_song_init_default(bpbx_song_s *song) {
    return bpbx_song_init(song, 3, 1, 0, 32, 8);
}

void bpbx_song_free(bpbx_song_s *song) {
    uint16_t channel_count = bpbx_song_channel_count(song);
    for (uint8_t i = 0; i < channel_count; ++i) {
        channel_free(&song->channels[i]);
    }

    free(song->channels);
    song->channels = NULL;
    song->pitch_channel_count = 0;
    song->noise_channel_count = 0;
    song->mod_channel_count = 0;
}

uint16_t bpbx_song_channel_count(const bpbx_song_s *song) {
    return (uint16_t)song->pitch_channel_count +
        song->noise_channel_count +
        song->mod_channel_count;
}

uint8_t bpbx_song_set_length(bpbx_song_s *song, uint16_t new_length, bpbx_resize_mode_e resize_mode) {
    if (new_length == song->length)
        return BPBX_STATUS_OK;

    for (uint16_t i = 0; i < bpbx_song_channel_count(song); ++i) {
        bpbx_channel_s *channel = &song->channels[i];

        uint16_t *new_track = malloc(new_length * sizeof(uint16_t));
        assert(new_track);
        if (new_track == NULL)
            return BPBX_STATUS_MEMERR;

        switch (resize_mode) {
            case BPBX_RESIZE_FROM_START:
                if (new_length < song->length) {
                    memcpy(new_track, channel->track + song->length - new_length, new_length);
                } else {
                    uint16_t diff = new_length - song->length;
                    memcpy(new_track + diff, channel->track, song->length);
                    memset(new_track, 0, diff);
                }
                break;
            
            case BPBX_RESIZE_FROM_END:
                if (new_length < song->length) {
                    memcpy(new_track, channel->track, new_length);
                } else {
                    memcpy(new_track, channel->track, song->length);
                    memset(new_track + song->length, 0, new_length - song->length);
                }
                break;
        }
        
        free(channel->track);
        channel->track = new_track;
    }

    song->length = new_length;
    return BPBX_STATUS_OK;
}

static inline void resize_channel_group(
    const bpbx_song_s *song,
    bpbx_channel_s *old_list, bpbx_channel_s *new_list,
    uint8_t old_channel_count, uint8_t new_channel_count,
    uint8_t channel_init_type
) {
    if (new_channel_count > old_channel_count) {
        // expand
        memcpy(new_list, old_list, old_channel_count);

        for (uint8_t i = old_channel_count; i < new_channel_count; ++i) {
            channel_init(&new_list[i], song);
            new_list[i].channel_type = channel_init_type;
        }
    } else {
        // shrink
        memcpy(new_list, song->channels, new_channel_count);

        for (uint8_t i = new_channel_count; i < old_channel_count; ++i) {
            channel_free(&song->channels[i]);
        }
    }
}

uint8_t bpbx_song_set_channel_count(
    bpbx_song_s *song,
    uint8_t pitch_channel_count, uint8_t drum_channel_count, uint8_t mod_channel_count)
{
    uint16_t noise_start = (uint16_t) song->pitch_channel_count;
    uint16_t mod_start = noise_start + (uint16_t) song->noise_channel_count;

    uint16_t total_channel_count = (uint16_t)pitch_channel_count + drum_channel_count + mod_channel_count;
    bpbx_channel_s *new_list = malloc(total_channel_count * sizeof(bpbx_channel_s));
    if (!new_list)
        return BPBX_STATUS_MEMERR;

    // pitch channels
    resize_channel_group(
        song,
        song->channels, new_list,
        song->pitch_channel_count, pitch_channel_count,
        BPBX_CHANNEL_PITCH
    );

    // noise channels
    resize_channel_group(
        song,
        song->channels + pitch_channel_count, new_list + noise_start,
        song->noise_channel_count, drum_channel_count,
        BPBX_CHANNEL_NOISE
    );

    // mod channels
    resize_channel_group(
        song,
        song->channels + pitch_channel_count + drum_channel_count, new_list + mod_start,
        song->mod_channel_count, mod_channel_count,
        BPBX_CHANNEL_MOD
    );

    free(song->channels);
    song->channels = new_list;
    song->pitch_channel_count = pitch_channel_count;
    song->noise_channel_count = drum_channel_count;
    song->mod_channel_count = mod_channel_count;
    return BPBX_STATUS_OK;
}

uint8_t bpbx_song_set_pattern_count(bpbx_song_s *song, uint16_t new_count) {
    assert(false && "not yet implemented");
    return BPBX_STATUS_FAIL;
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