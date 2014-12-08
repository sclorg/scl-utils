#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

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

static scl_rc get_collection_path(const char *colname, char **_colpath)
{
    FILE *fp = NULL;
    char *file_path = NULL;
    char *prefix;
    char *colpath = NULL;
    struct stat st;
    scl_rc ret = EOK;

    xasprintf(&file_path, "%s%s", SCL_CONF_DIR, colname);

    if (stat(file_path, &st) != 0) {
        debug("Unable to get file status %s: %s\n", file_path, strerror(errno));
        ret = EDISK;
        goto exit;
    }

    fp = fopen(file_path, "r");
    if (fp == NULL) {
        debug("Unable to open file %s: %s\n", file_path, strerror(errno));
        ret = EDISK;
        goto exit;
    }

    prefix = xmalloc(st.st_size + 1);
    if (fscanf(fp, "%s", prefix) != 1) {
        debug("Unable to read file %s: %s\n", file_path, strerror(errno));
        ret = EDISK;
        goto exit;
    }

    if (prefix[strlen(prefix) - 1] == '/') {
        xasprintf(&colpath, "%s%s", prefix, colname);
    } else {
        xasprintf(&colpath, "%s/%s", prefix, colname);
    }

    *_colpath = colpath;

exit:
    if (ret != EOK) {
        colpath = _free(colpath);
    }

    if (fp != NULL) {
        fclose(fp);
    }
    file_path = _free(file_path);
    prefix = _free(prefix);

    return ret;
}

bool fallback_is_collection_enabled(const char *colname)
{
    char *X_SCLS;
    bool ret = false;
    char **enabled_cols, **cols;

    X_SCLS = getenv("X_SCLS");

    if (X_SCLS != NULL) {
        X_SCLS = xstrdup(X_SCLS);
        cols = enabled_cols = split(X_SCLS, ' ');

        while(*cols != NULL) {
            if (!strcmp(*cols, colname)) {
                ret = true;
                break;
            }
            cols++;
        }
        enabled_cols = _free(enabled_cols);
        X_SCLS = _free(X_SCLS);
    }

    return ret;
}

scl_rc fallback_run_command(char * const colnames[], const char *cmd)
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

    fprintf(tfp, "%s\n", cmd);
    fclose(tfp);
    tfp = NULL;

    xasprintf(&bash_cmd, "/bin/bash %s", tmp);
    status = system(bash_cmd);
    if (status == -1 || !WIFEXITED(status)) {
        debug("Problem with executing command \"%s\"\n", bash_cmd);
        ret = ERUN;
        goto exit;
    }
    ret = WEXITSTATUS(status);

exit:
    if (tfp != NULL) {
        fclose(tfp);
    }
    enable_path = _free(enable_path);
    colpath = _free(colpath);
    bash_cmd = _free(bash_cmd);

    return ret;
}
