/* A simple key-value store */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dict.h"
#include "../src/sclmalloc.h"
#include "../src/lib_common.h"

dict dict_init()
{
    return calloc(1, sizeof(entry*));
}

void dict_free(dict d)
{
    int i = 0;
    entry *e;
    while ((e = d[i++])) {
        free(e->str);
        free(e);
    }
    free(d);
}

int dict_put(dict *d, char *string)
{
    int i = 0;
    int size = 0;
    char **parts;
    char *key;
    char *val;
    bool update = false;
    entry *e;

    parts = split(string, '=');
    key = parts[0];
    val = parts[1];
    free(parts);

    while ((e = (*d)[i++])) {
        if (strcmp(e->key, key))
            continue;
        update = true;
        break;
    }

    if (update) {
        free(e->str);
    } else {
        size = i + 1;
        *d = realloc(*d, size * sizeof(entry*));
        e = malloc(sizeof(entry));
        (*d)[size - 1] = NULL;
        (*d)[size - 2] = e;
    }

    e->str = string;
    e->key = key;
    e->val = val;

    return 0;
}

char *dict_get(dict d, const char *key)
{
    int i = 0;
    entry *e;
    while ((e = d[i++]))
        if (strcmp(e->key, key) == 0)
            return e->val;
    return NULL;
}

void dict_dump(dict d)
{
    int i = 0;
    entry *e;
    while ((e = d[i++]))
        printf("%s=%s\n", e->key, e->val);
}
