#include "../include/beepbox.h"
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define SONG_TAG_BEAT_COUNT             'a' // added in BeepBox URL version 2
#define SONG_TAG_BARS                   'b' // added in BeepBox URL version 2
#define SONG_TAG_VIBRATO                'c' // added in BeepBox URL version 2, DEPRECATED
#define SONG_TAG_FADE_IN_OUT            'd' // added in BeepBox URL version 3 for transition, switched to fadeInOut in 9
#define SONG_TAG_LOOP_END               'e' // added in BeepBox URL version 2
#define SONG_TAG_EQ_FILTER              'f' // added in BeepBox URL version 3
#define SONG_TAG_BAR_COUNT              'g' // added in BeepBox URL version 3
#define SONG_TAG_UNISON                 'h' // added in BeepBox URL version 2
#define SONG_TAG_INSTRUMENT_COUNT       'i' // added in BeepBox URL version 3
#define SONG_TAG_PATTERN_COUNT          'j' // added in BeepBox URL version 3
#define SONG_TAG_KEY                    'k' // added in BeepBox URL version 2
#define SONG_TAG_LOOP_START             'l' // added in BeepBox URL version 2
#define SONG_TAG_REVERB                 'm' // added in BeepBox URL version 5, DEPRECATED
#define SONG_TAG_CHANNEL_COUNT          'n' // added in BeepBox URL version 6
#define SONG_TAG_CHANNEL_OCTAVE         'o' // added in BeepBox URL version 3
#define SONG_TAG_PATTERNS               'p' // added in BeepBox URL version 2
#define SONG_TAG_EFFECTS                'q' // added in BeepBox URL version 7
#define SONG_TAG_RHYTHM                 'r' // added in BeepBox URL version 2
#define SONG_TAG_SCALE                  's' // added in BeepBox URL version 2
#define SONG_TAG_TEMPO                  't' // added in BeepBox URL version 2
#define SONG_TAG_PRESET                 'u' // added in BeepBox URL version 7
#define SONG_TAG_VOLUME                 'v' // added in BeepBox URL version 2
#define SONG_TAG_WAVE                   'w' // added in BeepBox URL version 2
#define SONG_TAG_SUPERSAW               'x' // added in BeepBox URL version 9
#define SONG_TAG_FILTER_RESONANCE       'y' // added in BeepBox URL version 7, DEPRECATED
#define SONG_TAG_DRUMSET_ENVELOPES      'z' // added in BeepBox URL version 7 for filter envelopes, still used for drumset envelopes
#define SONG_TAG_ALGORITHN              'A' // added in BeepBox URL version 6
#define SONG_TAG_FEEDBACK_AMPLITUDE     'B' // added in BeepBox URL version 6
#define SONG_TAG_CHORD                  'C' // added in BeepBox URL version 7, DEPRECATED
#define SONG_TAG_DETUNE                 'D' // added in JummBox URL version 3(?) for detune, DEPRECATED
#define SONG_TAG_ENVELOPES              'E' // added in BeepBox URL version 6 for FM operator envelopes, repurposed in 9 for general envelopes.
#define SONG_TAG_FEEDBACK_TYPE          'F' // added in BeepBox URL version 6
#define SONG_TAG_ARPEGGIO_SPEED         'G' // added in JummBox URL version 3 for arpeggioSpeed, DEPRECATED
#define SONG_TAG_HARMONICS              'H' // added in BeepBox URL version 7
#define SONG_TAG_STRING_SUSTAIN         'I' // added in BeepBox URL version 9
#define SONG_TAG_PAN                    'L' // added between 8 and 9, DEPRECATED
#define SONG_TAG_CUSTOM_CHIP_WAVE       'M' // added in JummBox URL version 1(?) for customChipWave
#define SONG_TAG_SONG_TITLE             'N' // added in JummBox URL version 1(?) for songTitle
#define SONG_TAG_LIMITER_SETTINGS       'O' // added in JummBox URL version 3(?) for limiterSettings
#define SONG_TAG_OPERATOR_AMPLITUDES    'P' // added in BeepBox URL version 6
#define SONG_TAG_OPERATOR_FREQUENCIES   'Q' // added in BeepBox URL version 6
#define SONG_TAG_OPERATOR_WAVES         'R' // added in JummBox URL version 4 for operatorWaves
#define SONG_TAG_SPECTRUM               'S' // added in BeepBox URL version 7
#define SONG_TAG_START_INSTRUMENT       'T' // added in BeepBox URL version 6
#define SONG_TAG_CHANNEL_NAMES          'U' // added in JummBox URL version 4(?) for channelNames
#define SONG_TAG_FEEDBACK_ENVELOPE      'V' // added in BeepBox URL version 6, DEPRECATED
#define SONG_TAG_PULSE_WIDTH            'W' // added in BeepBox URL version 7
#define SONG_TAG_ALIASES                'X' // added in JummBox URL version 4 for aliases, DEPRECATED

#define OLDEST_BEEPBOX_VERSION 2
#define LATEST_BEEPBOX_VERSION 9
#define OLDEST_JUMMBOX_VERSION 1
#define LATEST_JUMMBOX_VERSION 6

#define BPBX_LAYERED_INST_MAX_COUNT 4
#define BPBX_PATTERN_INST_MAX_COUNT 10
#define BPBX_PITCH_OCTAVES 8

static uint8_t bpbx_song_max_insts_per_channel(const bpbx_song_s *song) {
    uint8_t v0 = song->layered_instruments ? BPBX_LAYERED_INST_MAX_COUNT : 1;
    uint8_t v1 = song->pattern_instruments ? BPBX_PATTERN_INST_MAX_COUNT : 1;
    return v1 > v0 ? v1 : v0;
}

typedef struct {
    bpbx_reader_f read;
    void *userdata;
} reader_data_s;

enum {
    READER_STATUS_OK,
    READER_STATUS_EOF,
    READER_STATUS_ERR
};

static bool reader_read(void *out, size_t size, const reader_data_s *reader) {
    while (size > 0) {
        int64_t bytes_read = reader->read(size, out, reader->userdata);
        if (bytes_read == 0) return READER_STATUS_EOF;
        if (bytes_read == -1) return READER_STATUS_ERR;
        assert(bytes_read >= 0);
        assert(bytes_read <= size);
        if (bytes_read > size) return READER_STATUS_ERR;

        size -= bytes_read;
        out = ((uint8_t*)out + bytes_read);
    }

    return READER_STATUS_OK;
}

static inline uint8_t b64_to_int(char c) {
    static const uint8_t lookup[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 62, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 63, 0, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0, 0, 0, 0, 0}; // 62 could be represented by either "-" or "." for historical reasons. New songs should use "-".
    return lookup[c];
}

// #define log_error(msg, ...) fprintf(stderr, msg, __VA_ARGS__)
static void log_error(const char *msg, ...) {
    va_list va;
    va_start(va, msg);
    vfprintf(stderr, msg, va);
    va_end(va);
}
#define ERRCHK(v) if ((v) != READER_STATUS_OK) goto error

BPBX_API
uint8_t bpbx_song_load_base64(bpbx_song_s *song, bpbx_song_data_variant_e variant, bpbx_reader_f reader_func, void *reader_userdata) {
    reader_data_s reader = {
        .read = reader_func,
        .userdata = reader_userdata
    };

    uint8_t return_status = BPBX_STATUS_OK;
    char c;
    int status;

    // check if this is a json, and also
    // skip any irrelevant but likely symbols
    while (true) {
        ERRCHK(reader_read(&c, 1, &reader));

        // skip whitespace
        if (isspace(c)) continue;
        // skip hash symbol
        if (c == '#') continue;
        // curly brace: error, since json is not yet supported
        if (c == '{') {
            log_error("JSON loading is not yet implemented");
            goto error;
        }

        break;
    }

    // Detect variant here. If version doesn't match known variant, assume it is a vanilla string which does not report variant.
    bool from_beepbox, from_jummbox;
    switch (variant) {
        case BPBX_VARIANT_AUTO:
            if (c == 'j') {
                from_jummbox = true;
                from_beepbox = false;
            } else {
                from_beepbox = true;
                from_jummbox = false;
            }
            break;

        case BPBX_VARIANT_JUMMBOX:
            if (c != 'j') {
                log_error("JummBox specified, but got %c for variant instead of j", c);
                goto error;
            }

            from_beepbox = false;
            from_jummbox = true;
            break;

        case BPBX_VARIANT_BEEPBOX:
            from_beepbox = true;
            from_jummbox = false;
            break;
    }

    if (!from_beepbox) ERRCHK(reader_read(&c, 1, &reader));
    uint8_t version = b64_to_int(c);

    if (from_beepbox && (version > LATEST_BEEPBOX_VERSION || version < OLDEST_BEEPBOX_VERSION)) {
        log_error("BeepBox version %i is not supported", version);
    }

    if (from_jummbox && (version > LATEST_JUMMBOX_VERSION || version < OLDEST_JUMMBOX_VERSION)) {
        log_error("JummBox version %i is not supported", version);
    }

    bpbx_song_free(song);
    bpbx_song_init_default(song);

    while (true) {
        status = reader_read(&c, 1, &reader);
        if (status == READER_STATUS_EOF) break;
        if (status != READER_STATUS_OK) goto error;

        switch (c) {
            case SONG_TAG_SONG_TITLE: {
                char lc[2];
                ERRCHK(reader_read(lc, 2, &reader));
                uint8_t song_name_length = (b64_to_int(lc[0]) << 6) + b64_to_int(lc[1]);

                // TODO: decodeURIComponents
                char *song_name_str = malloc(song_name_length + 1);
                if (!song_name_str) {
                    log_error("could not read song title: error while allocating memory");
                    return_status = BPBX_STATUS_WARNING;
                    break;
                }

                ERRCHK(reader_read(song_name_str, song_name_length, &reader));
                song_name_str[song_name_length] = '\0';

                free(song->title);
                song->title = song_name_str;

                break;
            }

            case SONG_TAG_CHANNEL_COUNT: {
                char cc[2];
                ERRCHK(reader_read(cc, 2, &reader));
                uint8_t pitch_count = b64_to_int(cc[0]);
                uint8_t noise_count = b64_to_int(cc[1]);
                uint8_t mod_count = 0;

                // No mod channel support before jummbox v2
                if (from_beepbox || version >= 2) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    mod_count = b64_to_int(c);
                }

                bool can_load = true;
                
                if (pitch_count < 1 || pitch_count >= BPBX_MAX_PITCH_CHANNEL_COUNT) {
                    log_error("invalid pitch channel count range");
                    can_load = false;
                }

                if (noise_count >= BPBX_MAX_DRUM_CHANNEL_COUNT) {
                    log_error("invalid noise channel count range");
                    can_load = false;
                }

                if (mod_count >= BPBX_MAX_MOD_CHANNEL_COUNT) {
                    log_error("invalid mod channel count range");
                    can_load = false;
                }

                if (!can_load) {
                    status = BPBX_STATUS_WARNING;
                    break;
                }

                bpbx_song_set_pitch_channel_count(song, pitch_count);
                // TODO: set channel counts of noise and mod channels
                break;
            }

            case SONG_TAG_SCALE:
                // TODO: save scale data?
                ERRCHK(reader_read(&c, 1, &reader));
                break;

            case SONG_TAG_KEY:
                // TODO: save key data
                ERRCHK(reader_read(&c, 1, &reader));

                if (version < 7 && from_beepbox) {
                    song->key = 11 - b64_to_int(c);
                } else {
                    song->key = b64_to_int(c);
                }
                break;

            case SONG_TAG_LOOP_START:
                if (version < 5 && from_beepbox) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    song->loop_start = b64_to_int(c);
                } else {
                    char cc[2];
                    ERRCHK(reader_read(cc, 2, &reader));
                    song->loop_start = (b64_to_int(cc[0]) << 6) + b64_to_int(cc[1]);
                }
                break;

            case SONG_TAG_LOOP_END:
                if (version < 5 && from_beepbox) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    song->loop_end = b64_to_int(c);
                } else {
                    char cc[2];
                    ERRCHK(reader_read(cc, 2, &reader));
                    song->loop_end = (b64_to_int(cc[0]) << 6) + b64_to_int(cc[1]);
                }
                break;

            case SONG_TAG_TEMPO: {
                uint16_t tempo;

                if (version < 4 && from_beepbox) {
                    static const uint16_t tempo_values[] = {95, 120, 151, 190};
                    ERRCHK(reader_read(&c, 1, &reader));
                    uint8_t index = b64_to_int(c);
                    if (index >= 4) {
                        log_error("Tempo index is out of range");
                        status = BPBX_STATUS_WARNING;
                        break;
                    }

                    tempo = tempo_values[index];
                }
                else if (version < 7 && from_beepbox) {
                    static const uint16_t tempo_values[] = {88, 95, 103, 111, 120, 130, 140, 151, 163, 176, 190, 206, 222, 240, 259};
                    ERRCHK(reader_read(&c, 1, &reader));
                    uint8_t index = b64_to_int(c);
                    if (index >= sizeof(tempo_values)/sizeof(*tempo_values)) {
                        log_error("Tempo index is out of range");
                        status = BPBX_STATUS_WARNING;
                        break;
                    }

                    tempo = tempo_values[index];
                } else {
                    char cc[2];
                    ERRCHK(reader_read(cc, 2, &reader));
                    tempo = (b64_to_int(cc[0]) << 6) + b64_to_int(cc[1]);
                }

                // clamp tempo
                if (tempo < 30) tempo = 30;
                else if (tempo > 321) tempo = 321;

                song->tempo = tempo;
                break;
            }

            case SONG_TAG_BEAT_COUNT:
                ERRCHK(reader_read(&c, 1, &reader));

                if (version < 3 && from_beepbox) {
                    static const uint8_t values[] = {6, 7, 8, 9, 10};
                    uint8_t index = b64_to_int(c);
                    if (index >= 5) {
                        log_error("Beat count is out of range");
                        status = BPBX_STATUS_WARNING;
                        break;
                    }

                    song->beats_per_bar = values[index];
                } else {
                    song->beats_per_bar = b64_to_int(c) + 1;
                }
                break;

            case SONG_TAG_BAR_COUNT: {
                char cc[2];
                ERRCHK(reader_read(cc, 2, &reader));

                uint16_t bar_count = (b64_to_int(cc[0]) << 6) + b64_to_int(cc[1]) + 1;
                if (bar_count < 1 || bar_count > 256) {
                    log_error("Bar count is out of range");
                    goto error;
                }

                bpbx_song_set_length(song, bar_count, BPBX_RESIZE_FROM_END);

                // set the first four patterns of each channel track
                // to be 1, and the rest 0.
                uint16_t channel_count = bpbx_song_channel_count(song);
                for (uint16_t ch = 0; ch < channel_count; ++ch) {
                    for (uint16_t i = 0; i < bar_count; ++i) {
                        song->channels[ch].track[i] = (i < 4) ? 1 : 0;
                    }
                }
                break;
            }

            case SONG_TAG_PATTERN_COUNT: {
                uint16_t count;
                if (version < 8 && from_beepbox) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    count = b64_to_int(c) + 1;
                } else {
                    char cc[2];
                    ERRCHK(reader_read(cc, 2, &reader));
                    count = (b64_to_int(cc[0]) << 6) + b64_to_int(cc[1]) + 1;
                }

                if (count < 1 || count > 256) {
                    log_error("Pattern count is out of range");
                    goto error;
                }

                bpbx_song_set_pattern_count(song, count);
                break;
            }

            case SONG_TAG_INSTRUMENT_COUNT:
                if ((version < 9 || from_beepbox) || (version < 5 && from_jummbox)) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    uint8_t inst_per_channel = b64_to_int(c) + 1;
                    if (inst_per_channel > BPBX_PATTERN_INST_MAX_COUNT) {
                        log_error("Instrument count is out of range");
                        goto error;
                    }

                    uint16_t channel_count = bpbx_song_channel_count(song);
                    for (uint16_t i = 0; i < channel_count; ++i) {
                        bpbx_channel_s *channel = &song->channels[i];
                        channel->instrument_count = inst_per_channel;
                        // TODO: initialize instrument when loading instrument count
                    }
                } else {
                    uint8_t inst_flags = b64_to_int(c);
                    bool layered_instruments = (inst_flags & (1 << 1));
                    bool pattern_instruments = (inst_flags & (1 << 0));
                    
                    uint16_t channel_count = bpbx_song_channel_count(song);
                    for (uint16_t i = 0; i < channel_count; ++i) {
                        uint8_t inst_count = 1;
                        if (layered_instruments || pattern_instruments) {
                            ERRCHK(reader_read(&c, 1, &reader));
                            inst_count = b64_to_int(c) + 1;

                            if (inst_count < 1 || inst_count > bpbx_song_max_insts_per_channel(song)) {
                                log_error("Instrument count is out of range");
                                goto error;
                            }
                        }

                        bpbx_channel_s *channel = &song->channels[i];
                        channel->instrument_count = inst_count;
                        // TODO: initialize instrument when loading instrument count
                    }
                }

                break;

            case SONG_TAG_RHYTHM:
                // not relevant for playback
                ERRCHK(reader_read(&c, 1, &reader));
                break;

            case SONG_TAG_CHANNEL_OCTAVE:
                if (version < 3 && from_beepbox) {
                    ERRCHK(reader_read(&c, 1, &reader));
                    uint16_t channel_index = b64_to_int(c);

                    ERRCHK(reader_read(&c, 1, &reader));
                    uint8_t octave = b64_to_int(c) + 1;
                    song->channels[channel_index].octave = octave > BPBX_PITCH_OCTAVES ? BPBX_PITCH_OCTAVES : octave;
                } else if ((version < 9 && from_beepbox) || (version < 5 && from_jummbox)) {
                    uint16_t channel_count = bpbx_song_channel_count(song);
                    for (uint16_t i = 0; i < channel_count; ++i) {
                        ERRCHK(reader_read(&c, 1, &reader));
                        uint8_t octave = b64_to_int(c) + 1;
                        if (octave > BPBX_PITCH_OCTAVES) octave = BPBX_PITCH_OCTAVES;

                        bpbx_channel_s *channel = &song->channels[i];
                        if (channel->channel_type == BPBX_CHANNEL_PITCH) {
                            channel->octave = octave;
                        } else {
                            channel->octave = 0;
                        }
                    }
                } else {
                    for (uint8_t i = 0; i < song->pitch_channel_count; ++i) {
                        ERRCHK(reader_read(&c, 1, &reader));
                        uint8_t octave = b64_to_int(c) + 1;
                        if (octave > BPBX_PITCH_OCTAVES) octave = BPBX_PITCH_OCTAVES;
                        song->channels[i].octave = octave;
                    }

                    uint16_t ch_count = bpbx_song_channel_count(song);
                    for (uint16_t i = song->pitch_channel_count; i < ch_count; ++i) {
                        song->channels[i].octave = 0;
                    }
                }
                break;

            default: {
                log_error("Unrecognized song tag code %c", c);
                goto error;
            }
        }
    }

    return return_status;
    error:
    return BPBX_STATUS_ERROR;
}