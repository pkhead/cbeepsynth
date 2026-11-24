#ifndef STREAM_H
#define STREAM_H

#include <beepbox_synth.h>

// write primitive type in little-endian
int bbsyn_stream_write_int(bbxs_writer_f writer, void *userdata,
                           uintptr_t value, size_t int_size);
// read primitive type in little-endian.
int bbsyn_stream_read_int(bbxs_reader_f reader, void *userdata, void *value,
                          size_t int_size);

#endif