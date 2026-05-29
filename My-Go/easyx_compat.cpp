#include <stdio.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

extern "C" FILE* __cdecl __acrt_iob_func(unsigned int index);

extern "C" FILE* __cdecl __iob_func(void) {
    return __acrt_iob_func(0);
}

extern "C" FILE* (__cdecl * __imp___iob_func)(void) = __iob_func;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif