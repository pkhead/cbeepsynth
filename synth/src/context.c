#include "context.h"
#include "alloc.h"
#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/beepbox_synth.h"

void *bbxs_malloc(const bbxs_context_s *ctx, size_t size) {
    assert(ctx->alloc.alloc);
    return ctx->alloc.alloc(size, ctx->alloc.userdata);
}

void bbxs_free(const bbxs_context_s *ctx, void *ptr) {
    assert(ctx->alloc.free);
    if (ptr) ctx->alloc.free(ptr, ctx->alloc.userdata);
}

void bbsyn_logmsgf(const bbxs_context_s *ctx, bbxs_log_severity_e severity,
                   const char *msg, ...) {
    if (ctx->log_func == NULL) return;

    char msg_buf[256];

    va_list va;
    va_start(va, msg);
    vsnprintf(msg_buf, 256, msg, va);
    va_end(va);

    msg_buf[sizeof(msg_buf) - 1] = '\0';
    ctx->log_func(severity, msg_buf, ctx->log_userdata);
}