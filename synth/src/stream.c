#include "stream.h"

// some linux system library crap
#ifdef BIG_ENDIAN
#undef BIG_ENDIAN
#endif

#ifdef LITTLE_ENDIAN
#undef LITTLE_ENDIAN
#endif

#define BIG_ENDIAN 0
#define LITTLE_ENDIAN 1

static inline uint8_t endianness(void) {
    uint16_t endianness_test_value = 1;
    return ((uint8_t *)&endianness_test_value)[0];
}

// write primitive type in little-endian
int bbxs__stream_write_int(bbxs_writer_f writer, void *userdata,
                           uintptr_t value, size_t int_size) {
    if (endianness() == LITTLE_ENDIAN) {
        return writer(&value, int_size, userdata);
    } else {
        uint8_t *p = (uint8_t *)(&value) + int_size - 1;
        for (; int_size--;) {
            int stat = writer(p--, 1, userdata);
            if (stat) return stat;
        }
        return 0;
    }
}

// read primitive type in little-endian.
int bbxs__stream_read_int(bbxs_reader_f reader, void *userdata, void *value,
                          size_t int_size) {
    if (endianness() == LITTLE_ENDIAN) {
        return reader(value, int_size, userdata);
    } else {
        uint8_t *p = (uint8_t *)value + int_size - 1;
        for (; int_size--;) {
            int stat = reader(p--, 1, userdata);
            if (stat) return stat;
        }
        return 0;
    }
}