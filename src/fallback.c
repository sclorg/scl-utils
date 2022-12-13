#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#include "scllib.h"
#include "sclmalloc.h"
#include "config.h"
#include "errors.h"
#include "debug.h"
#include "lib_common.h"

bool has_old_collection(char * const colnames[])
{
    char *module_file_link = NULL;

    while (*colnames != NULL) {
        xasprintf(&module_file_link, SCL_MODULES_PATH "/%s", *colnames);
        if (access(module_file_link, F_OK)) {
            module_file_link = _free(module_file_link);
            return true;
        }
        module_file_link = _free(module_file_link);
        colnames++;
    }
    return false;
}

scl_rc fallback_get_enabled_collections(char ***_enabled_collections)
{
    char **enabled_collections = NULL;
    char *X_SCLS = getenv("X_SCLS");

    if (X_SCLS != NULL) {
        X_SCLS = xstrdup(X_SCLS);
        enabled_collections = split(X_SCLS, ' ');

        for (int i = 0; enabled_collections[i] != NULL; i++) {
            enabled_collections[i] = xstrdup(enabled_collections[i]);
        }
    }
    X_SCLS = _free(X_SCLS);
    *_enabled_collections = enabled_collections;

    return EOK;
}

bool fallback_is_collection_enabled(const char *colname)
{
    bool ret = false;
    char **enabled_collections = NULL;

    ret = fallback_get_enabled_collections(&enabled_collections);

    if (ret == EOK && enabled_collections != NULL) {
        for (int i = 0; enabled_collections[i] != NULL; i++) {
            if (!strcmp(enabled_collections[i], colname)) {
                ret = true;
                break;
            }
        }

    }
    enabled_collections = free_string_array(enabled_collections);

    return ret;
}

/*
 * See function collection_exists()
 */
scl_rc fallback_collection_exists(const char *colname, bool *_exists)
{
    char *col_path = NULL;
    char *conf_file = NULL;
    scl_rc ret = EOK;

    xasprintf(&conf_file, SCL_CONF_DIR "/%s", colname);
    *_exists = !access(conf_file, F_OK);

    if (*_exists) {
        ret = get_collection_path(colname, &col_path);
        if (ret != EOK) {
            return ret;
        }

        *_exists = !access(col_path, F_OK);

    }

    conf_file = _free(conf_file);
    col_path = _free(col_path);

    return EOK;
}

scl_rc fallback_get_installed_collections(char ***_colnames)
{
    struct dirent **nl;
    int n, i, i2 = 0;
    char **colnames;
    bool col_exists;
    scl_rc ret = EOK;

    n = scandir(SCL_CONF_DIR, &nl, 0, alphasort);
    if (n < 0) {
        debug("Cannot list directory %s: %s\n", SCL_CONF_DIR, strerror(errno));
        return EDISK;
    }

    /* Add one item for terminator */
    colnames = xcalloc(n + 1, sizeof(*colnames));

    for (i = 0; i < n; i++) {
        if (nl[i]->d_name[0] !=  '.') {
            ret = fallback_collection_exists(nl[i]->d_name, &col_exists);
            if (ret != EOK) {
                goto exit;
            }

            if (col_exists) {
                colnames[i2++] = nl[i]->d_name;
            }
        }
    }

    for (i = 0; i < i2; i++) {
        colnames[i] = xstrdup(colnames[i]);
    }

    colnames[i2] = NULL;
    *_colnames = colnames;

exit:
    if (ret != EOK) {
        for (i = 0; i < i2; i++) {
            colnames[i] = _free(colnames[i]);
        }
        colnames = _free(colnames);
    }

    for (i = 0; i < n; i++) {
        nl[i] = _free(nl[i]);
    }
    nl = _free(nl);

    return ret;
}

scl_rc fallback_run_command(char * const colnames[], const char *cmd, bool exec)
{
    scl_rc ret = EOK;
    char tmp[] = "/var/tmp/sclXXXXXX";
    int tfd;
    int status;
    FILE *tfp = NULL;
    char *colname;
    char *colpath = NULL;
    char *enable_path = NULL;
    char *bash_cmd = NULL;;

    const char *script_body =
        "SCLS+=(%s)\n"
        "export X_SCLS=$(printf '%%q ' \"${SCLS[@]}\")\n"
        ". %s\n";

    tfd = mkstemp(tmp);
    if (tfd < 0) {
        debug("Cannot create a temporary file %s: %s\n", tmp, strerror(errno));
        return EDISK;
    }
    tfp = fdopen(tfd, "w");
    if (tfp == NULL) {
        debug("Cannot open a temporary file %s: %s\n", tmp, strerror(errno));
        close(tfd);
        return EDISK;
    }

    if (fprintf(tfp, "eval \"SCLS=( ${X_SCLS[*]} )\"\n") < 0) {
        debug("Cannot write to a temporary file %s: %s\n", tmp,
            strerror(errno));
        ret = EDISK;
        goto exit;
    }

    while (*colnames != NULL) {
        colname = *colnames;

        if (fallback_is_collection_enabled(colname)) {
            colnames++;
            continue;
        }

        ret = get_collection_path(colname, &colpath);
        if (ret != EOK) {
            goto exit;
        }
        xasprintf(&enable_path, "%s/enable", colpath);

        if (fprintf(tfp, script_body, colname, enable_path) < 0) {
            debug("Cannot write to a temporary file %s: %s\n", tmp,
                strerror(errno));
            ret = EDISK;
            goto exit;
        }
        enable_path = _free(enable_path);
        colpath = _free(colpath);
        colnames++;
    }

    if (exec) {
        fprintf(tfp, "exec %s\n", cmd);
    } else {
        fprintf(tfp, "%s\n", cmd);
    }
    fclose(tfp);
    tfp = NULL;

    if (exec) {
        execl("/bin/bash", "/bin/bash", tmp, NULL);
        debug("Problem with executing program %s: %s\n", "/bin/bash", strerror(errno));
        ret = ERUN;

    } else {
        xasprintf(&bash_cmd, "/bin/bash %s", tmp);
        status = system(bash_cmd);
        if (status == -1 || !WIFEXITED(status)) {
            if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
                goto exit;
            debug("Problem with executing command \"%s\"\n", bash_cmd);
            ret = ERUN;
            goto exit;
        }
        ret = WEXITSTATUS(status);
    }

exit:
    if (tfp != NULL) {
        fclose(tfp);
    }
    enable_path = _free(enable_path);
    colpath = _free(colpath);
    bash_cmd = _free(bash_cmd);
    unlink(tmp);

    return ret;
}
