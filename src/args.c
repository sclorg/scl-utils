#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "errors.h"
#include "args.h"
#include "scllib.h"
#include "sclmalloc.h"
#include "lib_common.h"
#include "debug.h"

/**
 * Procedure to parse shebang line and transform it to standard command
 *
 * In general, the shebang line has n parts at the beginnning:
 * argv[0] => the name of the utility stated in shebang
 * argv[1] => all the arguments as one string
 * argv[2] => the script which was executed
 * argv[n] => argument for executed script
 *
 * Considering the following shebang line in script ./test.py:
 * #!/usr/bin/scl enable collectionX
 * argv[0] = "/usr/bin/scl"
 * argv[1] = "enable collectionX"
 * argv[2] = "./test.py"
 * argv[n] = "argn"
 */
static int parse_shebang(int argc, char *argv[],
                  int *_shebang_argc, char ***_shebang_argv) {
    int i = 0;
    char *p;
    int shebang_argc;
    char **shebang_argv;

    shebang_argc = count_words(argv[1], ' ') + argc - 1;
    shebang_argv = (char **)xmalloc(sizeof(char *) * shebang_argc);

    shebang_argv[i++] = argv[0];

    p = strtok(argv[1], " ");
    while (p != NULL) {
        shebang_argv[i++] = p;
        p = strtok(NULL, " ");
    }

    while (i < shebang_argc) {
        shebang_argv[i] = argv[i - (shebang_argc - argc)];
        i++;
    }

    *_shebang_argc = shebang_argc;
    *_shebang_argv = shebang_argv;

    return EOK;
}

static int extract_colpaths(int argc, char **argv, struct scl_args *args)
{
    int i;

    if (argc < 3) {
        return EINPUT;
    }

    /* two skipped, but one terminator*/
    args->colpaths = xcalloc(argc - 2 + 1, sizeof(*args->colpaths));

    /* argv[0] is program name, argv[1] is a command -> skip those */
    for (i = 0; i < argc - 2; i++) {
        args->colpaths[i] = argv[i + 2];
        if (args->colpaths[i][0] != '/') {
            debug("Collection must be specified with absolute path!\n\n");
            return EINPUT;
        }
    }
    args->colpaths[i] = NULL;

    return EOK;
}

static int extract_collections(int argc, char **argv, struct scl_args *args)
{
    int i;

    if (argc < 3) {
        return EINPUT;
    }

    /* two skipped, but one terminator*/
    args->collections = xcalloc(argc - 2 + 1, sizeof(*args->collections));

    /* argv[0] is program name, argv[1] is a command -> skip those */
    for (i = 0; i < argc - 2; i++) {
        args->collections[i] = argv[i + 2];
    }
    args->collections[i] = NULL;

    return EOK;
}

static int extract_command(int argc, char **argv, int command_pos,
                           struct scl_args *args) {
    int i;
    int chunk_len, len = 0;
    char *command = NULL;

    for (i = command_pos; i < argc; i++) {
        len += strlen(argv[i])+3; /* +1 for additional space, +2 for additional quotes */
    }
    len -= 3; /* The first part doesn't need neither quotes, nor space before */

    command = xmalloc((len+1)*sizeof(char));

    /* The first one is without quotes */
    len = strlen(argv[command_pos]);
    memcpy(command, argv[command_pos], len);
    for (i = command_pos+1; i < argc; i++) {
        command[len++] = ' ';
        command[len++] = '"';
        chunk_len = strlen(argv[i]);
        memcpy(command+len, argv[i], chunk_len);
        len += chunk_len;
        command[len++] = '"';
    }
    command[len] = '\0';

    args->command = command;
    return EOK;
}

static int extract_command_stdin(struct scl_args *args)
{
    size_t len;
    size_t r;
    char *command = NULL;

    command = xmalloc(BUFSIZ+1);

    len = 0;
    while ((r = fread(command+len, 1, BUFSIZ, stdin)) == BUFSIZ) {
        command = xrealloc(command, len+BUFSIZ+1);
        len += r;
    }

    if (feof(stdin)) {
        if (r < BUFSIZ) {
            len += r;
        }
        command[len] = '\0';
    } else {
        free(command);
        return EDISK;
    }

    args->command = command;
    return EOK;
}

static int parse_run_args(int argc, char **argv, struct scl_args *args)
{
    int i, ret;
    /* This initialization is important for the condition below */
    int separator_pos = argc-1;

    /* Find the separator (skip the argv[0] and [1]) */
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            separator_pos = i;
            break;
        }
    }
    if (separator_pos == 2) {
        /* No collections specified */
        return EINPUT;
    } else {
        ret = extract_collections(separator_pos, argv, args);
        if (ret != EOK) {
            return ret;
        }

        if (separator_pos > 2 && separator_pos < argc-1) {
            /* Collections and command separated */
            ret = extract_command(argc, argv, separator_pos+1, args);
            if (ret != EOK) {
                goto fail;
            }
        } else {
            /* One of two situations could have happened:
             * a) the separator is the last argument
             * b) the separator was not found
             * => re-test the last argument again
             */
            if (strcmp(argv[separator_pos], "--") != 0) {
                /* Separator not found - the last arg is command */
                args->command = xstrdup(argv[argc-1]);
            } else {
                /* Separator is the last arg -> command from stdin */
                args->command = xstrdup("-");
            }
        }

        if (strcmp(args->command, "-") == 0) {
            free(args->command);
            args->command = NULL; /* In case the subsequent call fails */
            ret = extract_command_stdin(args);
            if (ret != EOK) {
                goto fail;
            }
        }
    }

    return EOK;

fail:
    scl_args_free(args);
    return ret;
}

/**
 * Frees struct scl_args
 */
void scl_args_free(struct scl_args *args)
{
    if (args->collections != NULL) {
        free(args->collections);
    }
    if (args->colpaths != NULL) {
        free(args->colpaths);
    }
    if (args->command != NULL) {
        free(args->command);
        args->command = NULL;
    }
}

int scl_args_get(int argc, char *argv[], struct scl_args **_args)
{
    struct scl_args *args = NULL;
    char **shebang_argv = NULL;
    int shebang_argc;
    int ret;

    if (argc < 2) {
        return EINPUT;
    }

    if (argc >= 3 && strchr(argv[1], ' ') != NULL) {
        /* Apparently a shebang line */

        ret = parse_shebang(argc, argv, &shebang_argc, &shebang_argv);
        if (ret != EOK) {
            goto fail;
        }

        argc = shebang_argc;
        argv = shebang_argv;
    }

    args = xcalloc(1, sizeof(*args));

    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        args->action = ACTION_NONE;
    } else if ( argc == 2 && (
        !strcmp(argv[1], "list-collections") ||
        !strcmp(argv[1], "--list") ||
        !strcmp(argv[1], "-l"))) {

        if (strcmp(argv[1], "list-collections")) {
            debug("You use deprecated syntax \"-l|--list\", use \"list-collections\" instead.\n");
        }
        args->action = ACTION_LIST_COLLECTIONS;

    } else if (!strcmp(argv[1], "list-packages") ||
        !strcmp(argv[1], "--list") ||
        !strcmp(argv[1], "-l")) {

        if (strcmp(argv[1], "list-packages")) {
            debug("You use deprecated syntax \"-l|--list\", use \"list-packages\" instead.\n");
        }

        if (argc < 3) {
            ret = EINPUT;
            goto fail;
        } else {
            args->action = ACTION_LIST_PACKAGES;
            ret = extract_collections(argc, argv, args);
            if (ret != EOK) {
                goto fail;
            }
        }
    } else if (!strcmp(argv[1], "register")) {
        args->action = ACTION_REGISTER;
        if (argc < 3) {
            ret = EINPUT;
            goto fail;
        }
        ret = extract_colpaths(argc, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "deregister")) {
        int i, i2;

        args->action = ACTION_DEREGISTER;

        for (i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--force") || !strcmp(argv[i], "-f")) {
                args->force_flag = true;
                break;
            }
        }

        /* Remove -f / --force from argv */
        if (args->force_flag) {
            for (i2 = i; i2 < argc - 1; i2++) {
                argv[i2] = argv[i2 + 1];
            }
            argc--;
        }

        if (argc < 3) {
            ret = EINPUT;
            goto fail;
        }

        ret = extract_collections(argc, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "run") || !strcmp(argv[1], "enable")) {
        int i, i2;

        args->action = ACTION_COMMAND;

        for (i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--exec") || !strcmp(argv[i], "-x")) {
                args->exec_flag = true;
                break;
            }
        }

        /* Remove -x / --exec from argv */
        if (args->exec_flag) {
            for (i2 = i; i2 < argc - 1; i2++) {
                argv[i2] = argv[i2 + 1];
            }
            argc--;
        }

        if (argc < 4) {
            ret = EINPUT;
            goto fail;
        }

        ret = parse_run_args(argc, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "load")) {
        /* Just a list of collections */
        args->action = ACTION_LOAD;
        ret = extract_collections(argc, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "unload")) {
        /* Just a list of collections */
        args->action = ACTION_UNLOAD;
        ret = extract_collections(argc, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "man")) {
        /* Just one collection */
        args->action = ACTION_MAN;
        if (argc < 3) {
            ret = EINPUT;
            goto fail;
        } else if (argc > 3) {
            debug("Only the first collection will be taken into account!\n");
        }
        ret = extract_collections(3, argv, args);
        if (ret != EOK) {
            goto fail;
        }
    } else if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V")) {
        args->action = ACTION_VERSION;
    } else if (!strcmp(argv[1], "list-enabled")) {
        args->action = ACTION_LIST_ENABLED;
    } else {
        ret = EINPUT;
        goto fail;
    }

    free(shebang_argv);
    *_args = args;

    return EOK;

fail:
    free(args);
    free(shebang_argv);
    return ret;
}
