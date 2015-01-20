#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"

inline static void vmefail()
{
    debug("Memory allocation failed.\n");
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size)
{
    register void *value;
    value = malloc(size);
    if (value == NULL)
        vmefail();
    return value;
}

void *xcalloc(size_t nmemb, size_t size)
{
    register void *value;
    value = calloc(nmemb, size);
    if (value == NULL)
        vmefail();
    return value;
}

void *xrealloc(void *ptr, size_t size)
{
    register void *value;
    value = realloc(ptr, size);
    if (value == NULL)
        vmefail();
    return value;
}

char *xstrdup(const char *str)
{
    size_t size = strlen(str) + 1;
    char *newstr = (char *) malloc(size);
    if (newstr == NULL)
        vmefail();
    strcpy(newstr, str);
    return newstr;
}

int xasprintf(char **strp, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vasprintf(strp, fmt, args);
    if (ret == -1)
        vmefail();
    va_end(args);

    return ret;
}

void *_free(void *ptr)
{
    free(ptr);
    return NULL;
}
