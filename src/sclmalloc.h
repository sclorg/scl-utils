#ifndef __SCLMALLOC_H__
#define __SCLMALLOC_H__

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void *_free(void *ptr);
char *xstrdup(const char *str);
int xasprintf(char **strp, const char *fmt, ...);

#endif
