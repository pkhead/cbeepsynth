#include "param_util.h"

const char *bbxs__bool_enum_values[2] = {"off", "on"};
const char *bbxs__yes_no_enum_values[2] = {"no", "yes"};
const char *bbxs__filter_type_enum_values[4] = {"off", "low pass", "high pass",
                                                "notch"};

const char *bbxs__unison_enum_values[BBXS_UNISON_COUNT] = {
    "none",  "shimmer", "hum",   "honky tonk", "dissonant",
    "fifth", "octave",  "bowed", "piano",      "warbled"};