#ifndef BBSYN_MCODE_ARCH_H
#define BBSYN_MCODE_ARCH_H

#if defined(__x86_64__) || defined(_M_X64)
#define BBSYN_ARCH_X64
#ifdef _WIN64
#define BBSYN_CCONV_WIN64
#else
#define BBSYN_CCONV_SYSV64
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#define BBSYN_ARCH_ARM64
#define BBSYN_CCONV_ARM64
#endif

#endif