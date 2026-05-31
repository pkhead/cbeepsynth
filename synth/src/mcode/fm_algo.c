#include "../synth/fm.h"

// since, apparently, CMake is unable to reliably detect the target
// architecture, i will just use the C preprocessor to evaluate the correct
// source file.

#ifdef BBSYN_SUPPORT_FMGEN
#   if defined(__x86_64__) || defined(_M_X64)
#       include "fm_algo_x64.c"
#   else
#       error FM dynamic code generator is not supported on this platform!
#   endif
#endif