#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../include/beepbox.h"

typedef struct {
    const char *data;
    size_t strlen;
    size_t pos;
} reader_ud_s;

static int64_t reader_func(size_t size, void *out, void *p_userdata) {
    reader_ud_s *userdata = p_userdata;
    if (userdata->pos == userdata->strlen) return 0;

    int64_t count = size;
    if (userdata->pos + count > userdata->strlen) {
        count = userdata->strlen - userdata->pos;
    }

    memcpy(out, userdata->data + userdata->pos, count);
    userdata->pos += count;
    return count;
}

int main(void) {
    bpbx_song_s song;
    bool s;
    uint8_t stat;
    s = bpbx_song_init_default(&song);
    assert(s);

    reader_ud_s reader_data = {
        .data = "#j6N07Unnamedn310s0k0l00e03t2ma7g0fj07r1O_U00000000i0o321T1v0pud4f193q050Oa1d230A5F3BfQ0031Pfa35R0000E3c062263276T5v0puc9f151q050Oa1d230HXQRRJJHJAAArq8h0E0c0T0v0pu98f153q8720Oa71d4a0w5h2E0c0T2v0pu15f180q040Oad030w0E0c0b4h400000000h4g000000014h000000004h400000000p16000000",
        .pos = 0
    };
    reader_data.strlen = strlen(reader_data.data);
    stat = bpbx_song_load_base64(&song, BPBX_VARIANT_AUTO, reader_func, &reader_data);

    return 0;
}