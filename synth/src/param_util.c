#include "param_util.h"

const char *bbsyn_bool_enum_values[2] = {"off", "on"};
const char *bbsyn_yes_no_enum_values[2] = {"no", "yes"};
const char *bbsyn_filter_type_enum_values[4] = {
    "off", "low pass",
    "high pass", "notch"
};

const char *bbsyn_unison_enum_values[BPBXSYN_UNISON_COUNT] = {
    "none", "shimmer", "hum", "honky tonk", "dissonant",
    "fifth", "octave", "bowed", "piano", "warbled"
};