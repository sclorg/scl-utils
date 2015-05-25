#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"

#include "errors.h"
#include "scllib.h"
#include "args.h"
#include "lib_common.h"
#include "debug.h"
#include "fallback.h"

/**
 * Prints help on stderr.
 * @param   name    Name of the executed file.
 */
static void print_usage( const char *name ) {
    fprintf(stderr, "usage: %s enable|run [<collection>...] <command>\n", basename(name));
    fprintf(stderr, "       %s load|unload [<collection>...]\n", basename(name));
    fprintf(stderr, "       %s list-collections\n", basename(name));
    fprintf(stderr, "       %s list-packages|man|register|deregister <collection>\n", basename(name));
    fprintf(stderr, "       %s --help\n\n", basename(name));

    fprintf(stderr,
                 "description:\n"
                 "    enable|run            run given command from Software Collection \n"
                 "    load|unload           enable/disable collection in current shell\n"
                 "    list-collections      list installed Software Collections\n"
                 "    list-enabled          list Software Collections enabled in current shell\n"
                 "    list-packages         list pacages in Sofware Collection\n"
                 "    man                   show manual paga about Software Collection\n"
                 "    register|deregister   register/deregister Sofware Collection\n"
                 "    --help                show this help\n"
                 "\nUse '-' as <command> to read the command from standard input.\n");
}


int main(int argc, char *argv[]) {
	int ret = EOK;
    struct scl_args *args;
    char **pkgs = NULL;
    char **colls = NULL, **colls2 = NULL, **colls_merged = NULL;

    ret = scl_args_get(argc, argv, &args);
    if (ret == EINPUT) {
        fprintf(stderr, "Wrong arguments specified!\n\n");
        print_usage(argv[0]);

        return ret;
    } else if (ret != EOK) {
        fprintf(stderr, "Unspecified error when parsing arguments!\n\n");

        return ret;
    }


    switch (args->action) {
        case ACTION_NONE:
            print_usage(argv[0]);
            break;

        case ACTION_LIST_COLLECTIONS:
            ret = fallback_get_installed_collections(&colls);
            if (ret == EOK) {
                print_string_array(colls);
            }
            break;

        case ACTION_LIST_PACKAGES:
            ret = list_packages_in_collection(args->collections[0], &pkgs);
            if (ret == EOK) {
                print_string_array(pkgs);
            }
            break;

        case ACTION_COMMAND:
            if (has_old_collection(args->collections)) {
                ret = fallback_run_command(args->collections, args->command, args->exec_flag);
            } else {
                ret = run_command(args->collections, args->command, args->exec_flag);
            }
            break;
        case ACTION_REGISTER:
            ret = register_collection(args->colpaths[0]);
            break;
        case ACTION_DEREGISTER:
            ret = deregister_collection(args->collections[0], args->force_flag);
            break;
        case ACTION_MAN:
            ret = show_man(args->collections[0]);
            break;
        case ACTION_LOAD:
        case ACTION_UNLOAD:
            debug("Missing function scl in your environment!!!\n");
            ret = ECONFIG;
            break;
        case ACTION_VERSION:
            printf("%s\n", get_version());
            break;
        case ACTION_LIST_ENABLED:
            /* Get enabled collections of new type */
            ret = get_enabled_collections(&colls);
            if (ret != EOK) {
                break;
            }
            /* Get enabled collections of old type */
            ret = fallback_get_enabled_collections(&colls2);
            if (ret != EOK) {
                break;
            }

            /*
             * Some new collections can behave like old collections by defining
             * variable X_SCLS. There may be intersection between colls and
             * colls2. So merge them.
             */
            colls_merged = merge_string_arrays(colls, colls2);
            print_string_array(colls_merged);

            break;
    }

    scl_args_free(args);
    free(args);
    free_string_array(pkgs);
    free_string_array(colls);
    free_string_array(colls2);
    free_string_array(colls_merged);
    release_scllib_cache();
    return ret;
}
