#ifndef _log_h_
#define _log_h_

#include "context.h"

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 3, 4)))
#endif
void logmsgf(const bpbxsyn_context_s *ctx, bpbxsyn_log_severity_e severity,
             const char *msg, ...);

#endif