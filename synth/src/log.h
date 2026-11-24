#ifndef _log_h_
#define _log_h_

#include "context.h"

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 3, 4)))
#endif
void bbxs__logmsgf(const bbxs_context_s *ctx, bbxs_log_severity_e severity,
                   const char *msg, ...);

#endif