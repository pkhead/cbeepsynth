#include "eq.h"

#include <stdbool.h>
#include <math.h>
#include "audio.h"

void bpbxsyn_effect_init_eq(bpbxsyn_context_s *ctx, eq_effect_s *inst) {
    *inst = (eq_effect_s) {
        .base.type = BPBXSYN_EFFECT_EQ,
        .base.ctx = ctx
    };
}

void eq_destroy(bpbxsyn_effect_s *inst) {
    (void)inst;
}

void eq_stop(bpbxsyn_effect_s *p_inst) {
    eq_effect_s *const inst = (eq_effect_s*)p_inst;
    inst->filter_input[0] = 0.0;
    inst->filter_input[1] = 0.0;
}

// void panning_sample_rate_changed(bpbxsyn_effect_s *inst,
//                                  double old_sr, double new_sr);
void eq_tick(bpbxsyn_effect_s *p_inst, const bpbxsyn_tick_ctx_s *ctx) {
    eq_effect_s *const inst = (eq_effect_s*)p_inst;

    const double sample_rate = inst->base.sample_rate;
    const double rounded_samples_per_tick =
        ceil(calc_samples_per_tick(ctx->bpm, sample_rate));

    double eq_filter_volume = 1.0;
    for (int i = 0; i < FILTER_GROUP_COUNT; i++) {
        const filter_group_s *filter_group_start = &inst->old_params;
        const filter_group_s *filter_group_end = &inst->params;

        // If switching dot type, do it all at once and do not try to interpolate since no valid interpolation exists.
        if (filter_group_start->type[i] != filter_group_end->type[i]) {
            filter_group_start = filter_group_end;
        }

        if (filter_group_start->type[i] == BPBXSYN_FILTER_TYPE_OFF) {
            inst->filters[i].enabled = false;
        } else {
            inst->filters[i].enabled = true;

            filter_coefs_s start_coefs = filter_to_coefficients(
                filter_group_start, i,
                sample_rate,
                1.0,
                1.0);

            filter_coefs_s end_coefs = filter_to_coefficients(
                filter_group_end, i,
                sample_rate,
                1.0,
                1.0);
            
            dyn_biquad_load(&inst->filters[i],
                start_coefs, end_coefs, 1.0 / rounded_samples_per_tick,
                filter_group_start->type[i] == BPBXSYN_FILTER_TYPE_LP);

            eq_filter_volume *= filter_get_volume_compensation_mult(filter_group_start, i);
        }
    }

    if (eq_filter_volume > 3.0)
        eq_filter_volume = 3.0;

    // in synth.ts, volume appears to follow the interpolation "formula", but
    // the start and end values are always the same..? what's the point?
    inst->filter_volume = eq_filter_volume;
}

void eq_run(bpbxsyn_effect_s *p_inst, float **buffer, size_t frame_count) {
    eq_effect_s *const inst = (eq_effect_s*)p_inst;

    float *const audio = buffer[0];

    double x1 = inst->filter_input[0];
    double x2 = inst->filter_input[1];
    const double filter_volume = inst->filter_volume;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double sample = (double)audio[frame];

        double x0 = sample;
        sample = apply_filters(sample, x1, x2, inst->filters);
        x2 = x1;
        x1 = x0;

        sample *= filter_volume;

        audio[frame] = (float)sample;
    }

    inst->filter_input[0] = x1;
    inst->filter_input[1] = x2;
    sanitize_filters(inst->filters, FILTER_GROUP_COUNT);
}




/*
local template = [[{
    .id = "inEqTp@",
    .name = "EQ # Type",
    .group = "Effects/EQ",
    .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
    
    .type = BPBXSYN_PARAM_UINT8,
    .min_value = 0,
    .max_value = 3,
    .enum_values = filt_type_enum,
},
{
    .id = "inEqHz@",
    .name = "EQ # Freq.",
    .group = "Effects/EQ",
    
    .type = BPBXSYN_PARAM_DOUBLE,
    .min_value = 0,
    .max_value = BPBXSYN_FILTER_FREQ_MAX,
},
{
    .id = "inEqDb@",
    .name = "EQ # Gain",
    .group = "Effects/EQ",
    
    .type = BPBXSYN_PARAM_DOUBLE,
    .min_value = 0,
    .max_value = BPBXSYN_FILTER_GAIN_MAX,
},
]]

local build = {}
for i=1, 8 do
    local repl = template:gsub("#", tostring(i)):gsub("@", string.format("%02i", i))
    table.insert(build, repl)
end

local str = table.concat(build)

local p = assert(io.popen("clip.exe", "w"))
p:write(str)
p:close()
*/
static const char *filt_type_enum[] = {"Off", "Low pass", "High pass", "Notch"};
static const bpbxsyn_param_info_s param_info[BPBXSYN_EQ_PARAM_COUNT] = {
    {
        .id = "inEqTp01",
        .name = "EQ 1 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz01",
        .name = "EQ 1 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb01",
        .name = "EQ 1 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp02",
        .name = "EQ 2 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz02",
        .name = "EQ 2 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb02",
        .name = "EQ 2 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp03",
        .name = "EQ 3 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz03",
        .name = "EQ 3 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb03",
        .name = "EQ 3 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp04",
        .name = "EQ 4 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz04",
        .name = "EQ 4 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb04",
        .name = "EQ 4 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp05",
        .name = "EQ 5 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz05",
        .name = "EQ 5 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb05",
        .name = "EQ 5 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp06",
        .name = "EQ 6 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz06",
        .name = "EQ 6 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb06",
        .name = "EQ 6 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp07",
        .name = "EQ 7 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz07",
        .name = "EQ 7 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb07",
        .name = "EQ 7 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },
    {
        .id = "inEqTp08",
        .name = "EQ 8 Type",
        .group = "Effects/EQ",
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,
        
        .type = BPBXSYN_PARAM_UINT8,
        .min_value = 0,
        .max_value = 3,
        .enum_values = filt_type_enum,
    },
    {
        .id = "inEqHz08",
        .name = "EQ 8 Freq.",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_FREQ_MAX,
    },
    {
        .id = "inEqDb08",
        .name = "EQ 8 Gain",
        .group = "Effects/EQ",
        
        .type = BPBXSYN_PARAM_DOUBLE,
        .min_value = 0,
        .max_value = BPBXSYN_FILTER_GAIN_MAX,
    },

};

static const size_t param_addresses[BPBXSYN_EQ_PARAM_COUNT] = {
    offsetof(eq_effect_s, params.type    [0]),
    offsetof(eq_effect_s, params.freq_idx[0]),
    offsetof(eq_effect_s, params.gain_idx[0]),
    offsetof(eq_effect_s, params.type    [1]),
    offsetof(eq_effect_s, params.freq_idx[1]),
    offsetof(eq_effect_s, params.gain_idx[1]),
    offsetof(eq_effect_s, params.type    [2]),
    offsetof(eq_effect_s, params.freq_idx[2]),
    offsetof(eq_effect_s, params.gain_idx[2]),
    offsetof(eq_effect_s, params.type    [3]),
    offsetof(eq_effect_s, params.freq_idx[3]),
    offsetof(eq_effect_s, params.gain_idx[3]),
    offsetof(eq_effect_s, params.type    [4]),
    offsetof(eq_effect_s, params.freq_idx[4]),
    offsetof(eq_effect_s, params.gain_idx[4]),
    offsetof(eq_effect_s, params.type    [5]),
    offsetof(eq_effect_s, params.freq_idx[5]),
    offsetof(eq_effect_s, params.gain_idx[5]),
    offsetof(eq_effect_s, params.type    [6]),
    offsetof(eq_effect_s, params.freq_idx[6]),
    offsetof(eq_effect_s, params.gain_idx[6]),
    offsetof(eq_effect_s, params.type    [7]),
    offsetof(eq_effect_s, params.freq_idx[7]),
    offsetof(eq_effect_s, params.gain_idx[7]),
};

const effect_vtable_s effect_eq_vtable = {
    .struct_size = sizeof(eq_effect_s),
    .effect_init = (effect_init_f)bpbxsyn_effect_init_eq,
    .effect_destroy = eq_destroy,

    .input_channel_count = 1,
    .output_channel_count = 1,

    .param_count = BPBXSYN_EQ_PARAM_COUNT,
    .param_info = param_info,
    .param_addresses = param_addresses,

    .effect_stop = eq_stop,
    .effect_tick = eq_tick,
    .effect_run = eq_run
};