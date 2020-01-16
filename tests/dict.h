typedef struct {
    char *str;
    char *key;
    char *val;
} entry;

typedef entry** dict;

dict dict_init();
void dict_free(dict d);
int dict_put(dict *d, char *string);
char *dict_get(dict d, const char *key);
void dict_dump(dict d);
