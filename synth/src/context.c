#include "context.h"
#include "alloc.h"
#include "log.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "../include/beepbox_synth.h"


void* bpbxsyn_malloc(const bpbxsyn_context_s *ctx, size_t size) {
    assert(ctx->alloc.alloc);
    return ctx->alloc.alloc(size, ctx->alloc.userdata);
}

void bpbxsyn_free(const bpbxsyn_context_s *ctx, void *ptr) {
    assert(ctx->alloc.free);
    if (ptr)
        ctx->alloc.free(ptr, ctx->alloc.userdata);
}

void logmsgf(const bpbxsyn_context_s *ctx, bpbxsyn_log_severity_e severity,
             const char *msg, ...)
{
    if (ctx->log_func == NULL) return;

    char msg_buf[256];

    va_list va;
    va_start(va, msg);
    vsnprintf(msg_buf, 256, msg, va);
    va_end(va);

    msg_buf[sizeof(msg_buf)-1] = '\0';
    ctx->log_func(severity, msg_buf, ctx->log_userdata);
}