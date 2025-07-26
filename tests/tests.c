#include "munit/munit.h"
#include <beepbox.h>

static void* song_default_setup(const MunitParameter params[], void* user_data) {
    bpbx_song_s *song = munit_malloc(sizeof(bpbx_song_s));
    bpbx_song_init_default(song);
    return song;
}

static void song_default_teardown(void* user_data) {
    bpbx_song_s *song = user_data;
    bpbx_song_free(song);
}

MunitResult test_song_init(const MunitParameter parmas[], void *ud) {
    bpbx_song_s *song = ud;
    munit_assert(song->pitch_channel_count == 3);

    return MUNIT_OK;
}

static MunitTest song_tests[] = {
    {
        "/song/init",
        test_song_init,
        song_default_setup,
        song_default_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    // end of array
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
    "/song-tests",
    song_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}