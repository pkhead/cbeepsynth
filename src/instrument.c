#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include "instrument.h"
#include "util.h"

#define VOLUME_LOG_SCALE 0.1428

double calc_samples_per_tick(double bpm, double sample_rate) {
    const double beats_per_sec = bpm / 60.0;
    const double parts_per_sec = PARTS_PER_BEAT * beats_per_sec;
    const double ticks_per_sec = TICKS_PER_PART * parts_per_sec;
    return sample_rate / ticks_per_sec;
}

double note_size_to_volume_mult(double size) {
    return pow(max(0.0, size) / NOTE_SIZE_MAX, 1.5);
}

double inst_volume_to_mult(double inst_volume) {
    if (inst_volume <= -25.0) return 0.0;
    return pow(2.0, VOLUME_LOG_SCALE * inst_volume);
}

const double vibrato_normal_periods_secs[1] = {0.14};
const double vibrato_shaky_periods_secs[3] = {0.11, 1.618 * 0.11, 3 * 0.11};

typedef struct {
    int size_periods_secs;
    const double *periods_secs;
} vibrato_type_def_s;

static vibrato_type_def_s vibrato_types[2] = {
    {
        .size_periods_secs = 1,
        .periods_secs = vibrato_normal_periods_secs
    },
    {
        .size_periods_secs = 3,
        .periods_secs = vibrato_shaky_periods_secs
    }
};

double get_lfo_amplitude(bpbx_vibrato_type_e type, double secs_into_bar) {
    assert(0 <= type && type <= 1);

    double effect = 0.0;
    vibrato_type_def_s type_config = vibrato_types[type];
    for (int i = 0; i < type_config.size_periods_secs; i++) {
        const double vibrato_period_secs = type_config.periods_secs[i];
        effect += sin(PI2 * secs_into_bar / vibrato_period_secs);
    }
    return effect;
}










////////////
//  DATA  //
////////////

static const char *bool_enum_values[] = {"Off", "On"};
static const char *transition_type_values[] = {"Normal", "Interrupt", "Continue", "Slide"};
static const char *filt_type_enum[] = {"Low pass", "High pass", "Notch"};
static const char *chord_type_values[] = {"Simultaneous", "Strum", "Arpeggio", "Custom Interval"};
static const char *vibrato_preset_values[] = {"None", "Light", "Delayed", "Heavy", "Shaky", "Custom"};
static const char *vibrato_values[] = {"Normal", "Shaky"};

#define FILTER_MAX_FREQ 33
#define FILTER_MAX_GAIN 14

bpbx_inst_param_info_s base_param_info[BPBX_BASE_PARAM_COUNT] = {
    // general
    {
        .name = "Volume",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -25.0,
        .max_value = 25.0,
        .default_value = 0.0
    },
    {
        .name = "Fade In",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 9.0,
        .default_value = 0.0
    },
    {
        .name = "Fade Out",
        .group = "General",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = FADE_OUT_MIN,
        .max_value = FADE_OUT_MAX,
        .default_value = 0.0
    },

    // modulation params
    {
        .name = "Modulation X",
        .group = "Modulation",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 1.0,
        .default_value = 0.0
    },
    {
        .name = "Modulation Y",
        .group = "Modulation",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0.0,
        .max_value = 1.0,
        .default_value = 0.0
    },

    // transition type
    {
        .name = "Transition Type Toggle",
        .group = "Effects/Transition Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Transition Type",
        .group = "Effects/Transition Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .default_value = 0,
        .enum_values = transition_type_values
    },

    // chord type
    {
        .name = "Chord Type Toggle",
        .group = "Effects/Chord Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Chord Type",
        .group = "Effects/Chord Type",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .default_value = 0,
        .enum_values = chord_type_values
    },

    // pitch shift
    {
        .name = "Pitch Shift Toggle",
        .group = "Effects/Pitch Shift",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Pitch Shift (st)",
        .group = "Effects/Pitch Shift",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -12,
        .max_value = 12,
        .default_value = 0,
    },

    // detune
    {
        .name = "Detune Toggle",
        .group = "Effects/Detune",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Detune (c)",
        .group = "Effects/Detune",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = -200,
        .max_value = 200,
        .default_value = 0,
    },

    // vibrato
    {
        .name = "Vibrato Toggle",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Vibrato Preset",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 5,
        .default_value = 0,
        .enum_values = vibrato_preset_values
    },
    {
        .name = "Vibrato Depth",
        .group = "Effects/Vibrato",

        // in beepbox code, this was quantized to increments of 0.04
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = 2,
        .default_value = 0,
        .enum_values = bool_enum_values
    },
    {
        .name = "Vibrato Speed",
        .group = "Effects/Vibrato",

        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = 3.0,
        .default_value = 1.0,
    },
    {
        .name = "Vibrato Delay",
        .group = "Effects/Vibrato",

        .type = BPBX_PARAM_INT,
        .min_value = 0,
        .max_value = 50,
        .default_value = 0,
    },
    {
        .name = "Vibrato Type",
        .group = "Effects/Vibrato",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,

        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .default_value = 0,
        .enum_values = vibrato_values
    },

    // note filter
    /*
    local template = [[{
        .name = "Note Filter # Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter # Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter # Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter # Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    ]]

    local build = {}
    for i=1, 8 do
        local repl = template:gsub("#", tostring(i))
        table.insert(build, repl)
    end

    local str = table.concat(build)

    local p = assert(io.popen("clip.exe", "w"))
    p:write(str)
    p:close()
    */
    {
        .name = "Note Filter Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 1 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 1 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 1 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 1 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 2 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 2 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 2 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 2 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 3 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 3 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 3 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 3 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 4 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 4 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 4 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 4 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 5 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 5 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 5 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 5 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 6 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 6 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 6 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 6 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 7 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 7 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 7 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 7 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "Note Filter 8 Toggle",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "Note Filter 8 Type",
        .group = "Effects/Note Filter",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "Note Filter 8 Freq.",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "Note Filter 8 Gain",
        .group = "Effects/Note Filter",
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    
    
    // eq filter
    {
        .name = "EQ 1 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 1 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 1 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 1 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 2 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 2 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 2 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 2 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 3 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 3 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 3 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 3 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 4 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 4 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 4 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 4 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 5 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 5 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 5 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 5 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 6 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 6 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 6 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 6 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 7 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 7 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 7 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 7 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },
    {
        .name = "EQ 8 Toggle",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 1,
        .enum_values = bool_enum_values,
    },
    {
        .name = "EQ 8 Type",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_UINT8,
        .min_value = 0,
        .max_value = 2,
        .enum_values = filt_type_enum,
    },
    {
        .name = "EQ 8 Freq.",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_FREQ,
    },
    {
        .name = "EQ 8 Gain",
        .group = "EQ",
        .flags = BPBX_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBX_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = FILTER_MAX_GAIN,
    },    
};

size_t base_param_offsets[BPBX_BASE_PARAM_COUNT] = {
    // general
    offsetof(bpbx_inst_s, volume),
    offsetof(bpbx_inst_s, fade_in),
    offsetof(bpbx_inst_s, fade_out),

    // modulation
    offsetof(bpbx_inst_s, mod_x),
    offsetof(bpbx_inst_s, mod_y),

    // transition type
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_TRANSITION_TYPE]),
    offsetof(bpbx_inst_s, transition_type),

    // chord type
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_CHORD_TYPE]),
    offsetof(bpbx_inst_s, chord_type),

    // pitch shift
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_PITCH_SHIFT]),
    offsetof(bpbx_inst_s, pitch_shift),

    // detune
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_DETUNE]),
    offsetof(bpbx_inst_s, detune),

    // vibrato
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_VIBRATO]),
    offsetof(bpbx_inst_s, vibrato_preset),
    offsetof(bpbx_inst_s, vibrato.depth),
    offsetof(bpbx_inst_s, vibrato.speed),
    offsetof(bpbx_inst_s, vibrato.delay),
    offsetof(bpbx_inst_s, vibrato.type),

    // note filter params
    offsetof(bpbx_inst_s, active_effects[BPBX_INSTFX_NOTE_FILTER]),
    offsetof(bpbx_inst_s, note_filter.enabled [0]),
    offsetof(bpbx_inst_s, note_filter.type    [0]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[0]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[0]),
    offsetof(bpbx_inst_s, note_filter.enabled [1]),
    offsetof(bpbx_inst_s, note_filter.type    [1]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[1]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[1]),
    offsetof(bpbx_inst_s, note_filter.enabled [2]),
    offsetof(bpbx_inst_s, note_filter.type    [2]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[2]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[2]),
    offsetof(bpbx_inst_s, note_filter.enabled [3]),
    offsetof(bpbx_inst_s, note_filter.type    [3]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[3]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[3]),
    offsetof(bpbx_inst_s, note_filter.enabled [4]),
    offsetof(bpbx_inst_s, note_filter.type    [4]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[4]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[4]),
    offsetof(bpbx_inst_s, note_filter.enabled [5]),
    offsetof(bpbx_inst_s, note_filter.type    [5]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[5]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[5]),
    offsetof(bpbx_inst_s, note_filter.enabled [6]),
    offsetof(bpbx_inst_s, note_filter.type    [6]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[6]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[6]),
    offsetof(bpbx_inst_s, note_filter.enabled [7]),
    offsetof(bpbx_inst_s, note_filter.type    [7]),
    offsetof(bpbx_inst_s, note_filter.freq_idx[7]),
    offsetof(bpbx_inst_s, note_filter.gain_idx[7]),

    // eq filter params
    offsetof(bpbx_inst_s, eq.enabled [0]),
    offsetof(bpbx_inst_s, eq.type    [0]),
    offsetof(bpbx_inst_s, eq.freq_idx[0]),
    offsetof(bpbx_inst_s, eq.gain_idx[0]),
    offsetof(bpbx_inst_s, eq.enabled [1]),
    offsetof(bpbx_inst_s, eq.type    [1]),
    offsetof(bpbx_inst_s, eq.freq_idx[1]),
    offsetof(bpbx_inst_s, eq.gain_idx[1]),
    offsetof(bpbx_inst_s, eq.enabled [2]),
    offsetof(bpbx_inst_s, eq.type    [2]),
    offsetof(bpbx_inst_s, eq.freq_idx[2]),
    offsetof(bpbx_inst_s, eq.gain_idx[2]),
    offsetof(bpbx_inst_s, eq.enabled [3]),
    offsetof(bpbx_inst_s, eq.type    [3]),
    offsetof(bpbx_inst_s, eq.freq_idx[3]),
    offsetof(bpbx_inst_s, eq.gain_idx[3]),
    offsetof(bpbx_inst_s, eq.enabled [4]),
    offsetof(bpbx_inst_s, eq.type    [4]),
    offsetof(bpbx_inst_s, eq.freq_idx[4]),
    offsetof(bpbx_inst_s, eq.gain_idx[4]),
    offsetof(bpbx_inst_s, eq.enabled [5]),
    offsetof(bpbx_inst_s, eq.type    [5]),
    offsetof(bpbx_inst_s, eq.freq_idx[5]),
    offsetof(bpbx_inst_s, eq.gain_idx[5]),
    offsetof(bpbx_inst_s, eq.enabled [6]),
    offsetof(bpbx_inst_s, eq.type    [6]),
    offsetof(bpbx_inst_s, eq.freq_idx[6]),
    offsetof(bpbx_inst_s, eq.gain_idx[6]),
    offsetof(bpbx_inst_s, eq.enabled [7]),
    offsetof(bpbx_inst_s, eq.type    [7]),
    offsetof(bpbx_inst_s, eq.freq_idx[7]),
    offsetof(bpbx_inst_s, eq.gain_idx[7]),
};