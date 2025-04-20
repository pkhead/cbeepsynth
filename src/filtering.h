#ifndef _filtering_h_
#define _filtering_h_

#include <stddef.h>
#include <stdint.h>

#define FILTER_GROUP_COUNT 8

typedef struct {
    uint8_t enabled[FILTER_GROUP_COUNT];
    uint8_t type[FILTER_GROUP_COUNT];
    double freq_idx[FILTER_GROUP_COUNT];
    double gain_idx[FILTER_GROUP_COUNT];
} filter_group_s;

#endif