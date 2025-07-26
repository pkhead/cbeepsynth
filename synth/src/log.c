#ifndef _bpbx_log_h_
#define _bpbx_log_h_

#include <stdarg.h>
#include <stdio.h>
#include "../include/beepbox_synth.h"

static bpbx_log_f s_log_function = NULL;
static void *s_log_userdata = NULL;

void bpbx_set_log_func(bpbx_log_f log_func, void *userdata) {
    s_log_function = log_func;
    s_log_userdata = userdata;
}

void logmsgf(bpbx_log_severity_e severity, const char *msg, ...) {
    if (s_log_function == NULL) return;

    char msg_buf[256];

    va_list va;
    va_start(va, msg);
    vsnprintf(msg_buf, 256, msg, va);
    va_end(va);

    msg_buf[sizeof(msg_buf)-1] = '\0';
    s_log_function(severity, msg_buf, s_log_userdata);
}

#endif