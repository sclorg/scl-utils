#ifndef __LIB_COMMON_H__
#define __LIB_COMMON_H__

#include "errors.h"

char *get_command_output(const char *path, char *const argv[], int fileno);
scl_rc prepare_args(const char *cmd, char ***_argv);
int count_words(const char *str, char ch);
void unescape_string(char *str);
void strip_trailing_chars(char *str, char char_to_strip);
char **split(char *str, char delim);
void print_string_array(char *const *array);
void *free_string_array(char **array);
char *strip_trailing_slashes(const char *path_to_strip);
char *directory_name(const char *_path);

#endif
