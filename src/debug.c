#include <stdio.h>
#include <stdarg.h>

#include "debug.h"

FILE *debug_file;

void debug(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

    vfprintf(debug_file ? debug_file : stderr, format, ap);
    fflush(debug_file ? debug_file : stderr);

    va_end(ap);
}
