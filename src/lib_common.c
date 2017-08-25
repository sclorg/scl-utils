#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include "errors.h"
#include "scllib.h"
#include "debug.h"
#include "sclmalloc.h"
#include "lib_common.h"

char *get_command_output(const char *path, char *const argv[], int fileno)
{
    int pid = 0, status;
    int outpipe[2] = {0, 0};
    FILE *fpipe = NULL;
    int c;
    char *buffer = NULL;
    int count = 0, alloced = 5;
    int error = 1;

    if (pipe(outpipe) == -1) {
        goto exit;
    }

    pid = fork();
    if (pid == -1) {
        goto exit;
    } else if (pid == 0) {/* Child */
        close(outpipe[0]);
        if (dup2(outpipe[1], fileno) == -1) {
            goto exit;
        }
        execv(path, argv);
        exit(EXIT_FAILURE);
    }

    close(outpipe[1]);
    fpipe = fdopen(outpipe[0], "r");
    if (fpipe == NULL){
        goto exit;
    }
    outpipe[0] = 0;
    buffer = xmalloc(alloced);

    while (1) {
        c = fgetc(fpipe);
        if (ferror(fpipe) != 0) {
            goto exit;
        }
        count++;
        if (count > alloced) {
            alloced <<=1;
            buffer = xrealloc(buffer, alloced);
        }
        if (c != EOF) {
            buffer[count - 1] = c;
        } else {
            buffer[count - 1] = '\0';
            break;
        }
    }
    error = 0;

exit:
    if (outpipe[0]) {
        close(outpipe[0]);
    }

    if (outpipe[1]) {
        close(outpipe[1]);
    }

    if (fpipe) {
        fclose(fpipe);
    }

    if (pid > 0) {
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status)) {
            debug("Program %s didn't terminate normally!\n", path);
            error = 1;
        }else if (WEXITSTATUS(status)) {
            debug("Program %s returned nonzero return code!\n", path);
            error = 1;
        }
    }

    if (error) {
        buffer = _free(buffer);
    }

    return buffer;
}

scl_rc prepare_args(const char *cmd, char ***_argv)
{
    wordexp_t p;
    char **argv;
    int i;

    if (wordexp(cmd, &p, 0) != 0) {
        debug("Problem occurred during parsing command!!!\n");
        return EINPUT;
    }
    argv = xmalloc((p.we_wordc + 1) * sizeof(*argv));

    for (i = 0; i < p.we_wordc; i++) {
        argv[i] = xstrdup(p.we_wordv[i]);
    }
    argv[i] = NULL;
    wordfree(&p);

    *_argv = argv;

    return EOK;
}

int count_words(const char *str, char ch)
{
    int count = 0;
    bool inside_word = false;

    while (*str != '\0') {
        if (*str != ch) {
            if (!inside_word) {
                count++;
            }
            inside_word = true;
        } else {
            inside_word = false;
        }
        str++;
    }

    return count;
}

void strip_trailing_chars(char *str, char char_to_strip)
{
    for (int i = strlen(str) - 1; i >=0; i--) {
        if (str[i] == char_to_strip) {
            str[i] = '\0';
        } else {
            break;
        }
    }
}

void unescape_string(char *str)
{
    int str_len = strlen(str);
    int si, ti = 0;

    for (si = 0; si < str_len; si++) {
        if (str[si] == '\\') {
            si++;
            if (si == str_len) {
                break;
            }
        }

        str[ti++] = str[si];
    }
    str[ti] = '\0';
}


char **split(char *str, char delim)
{
    char **parts, *p;
    int word_count, i = 0;
    char delim_str[2];

    delim_str[0] = delim;
    delim_str[1] = '\0';

    word_count = count_words(str, delim);
    parts = xmalloc((word_count + 1) * sizeof(*parts));

    p = strtok(str, delim_str);
    while (p != NULL) {
        parts[i++] = p;
        p = strtok(NULL, delim_str);
    }
    parts[i] = NULL;

    return parts;
}

/**
 * Prints content of the array and frees the memory.
 * @param   array   Array to print.
 */
void print_string_array(char *const *array) {
    int i = 0;
    if (array == NULL) {
        return;
    }
    while (array[i] != NULL) {
        printf("%s\n", array[i++]);
    }
}

int string_array_len(char *const *array)
{
    int i = 0;

    if (array == NULL) {
        return 0;
    }
    while (array[i] != NULL) {
        i++;
    }

    return i;
}

static int cmpstringp(const void *p1, const void *p2)
{
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

char **merge_string_arrays(char *const *array1, char *const *array2)
{
    char **merged_array = NULL;
    int len, ix = 0;
    char *const *arrays[2] = {array1, array2};
    int array_len[2];
    int  prev;

    array_len[0] = string_array_len(array1);
    array_len[1] = string_array_len(array2);
    len = array_len[0] + array_len[1];
    if (len == 0) {
        return NULL;
    }

    merged_array = xmalloc((len + 1) * sizeof(*merged_array));

    /* Put content of two arrays into one array */
    for (int i = 0; i < 2; i++) {
        /* If any of arrays is NULL then skip it */
        if (arrays[i] == NULL) {
            continue;
        }

        for (int i2 = 0; i2 < array_len[i]; i2++) {
            merged_array[ix++] = arrays[i][i2];
        }
    }

    /* Sort resulting array */
    if (len > 1) {
        qsort(merged_array, len, sizeof(*merged_array), cmpstringp);
    }

    /* Unique the array */
    prev = 0;
    for (int i = 1; i < len; i++) {
        if (strcmp(merged_array[prev], merged_array[i])) {
            merged_array[++prev] = merged_array[i];
        }
    }
    merged_array[++prev] = NULL;

    return merged_array;
}

/**
 * Frees string array
 * @param[in]   count   Number of items in array.
 * @param[out]  array   String array, that will be freed.
 */
void *free_string_array(char **array) {
    int i = 0;

    if (array != NULL) {
        while (array[i] != NULL) {
            free(array[i++]);
        }
        free(array);
    }
    return NULL;
}

char *directory_name(const char *_path)
{
    char *path, *c;

    path = xstrdup(_path);
    c = path + strlen(path) - 1;
    while ( *c != '/' ) {
        *c = '\0';
        c--;
    }

    return path;
}
