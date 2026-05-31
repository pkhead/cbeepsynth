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

bpbxsyn_mcalloc_id bpbxsyn_mc_alloc(const bpbxsyn_context_s *ctx, size_t size,
                                    void **rw, const void **exec)
{
    if (!ctx->alloc.mc_alloc) {
        *rw = NULL;
        *exec = NULL;
        return BPBXSYN_MCALLOC_INVALID_ID;
    }

    return ctx->alloc.mc_alloc(size, ctx->alloc.mc_userdata, rw, exec);
}

void bpbxsyn_mc_free(const bpbxsyn_context_s *ctx, bpbxsyn_mcalloc_id handle) {
    if (!ctx->alloc.mc_free) return;
    if (handle == BPBXSYN_MCALLOC_INVALID_ID) return;
    
    ctx->alloc.mc_free(handle, ctx->alloc.mc_userdata);
}

void bbsyn_logmsgf(const bpbxsyn_context_s *ctx,
                   bpbxsyn_log_severity_e severity, const char *msg, ...) {
    if (ctx->log_func == NULL) return;

    char msg_buf[256];

    va_list va;
    va_start(va, msg);
    vsnprintf(msg_buf, 256, msg, va);
    va_end(va);

    msg_buf[sizeof(msg_buf)-1] = '\0';
    ctx->log_func(severity, msg_buf, ctx->log_userdata);
}