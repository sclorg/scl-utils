#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmcli.h>
#include <errno.h>
#include <wordexp.h>

#include "config.h"
#include "errors.h"
#include "scllib.h"
#include "debug.h"
#include "lib_common.h"
#include "sclmalloc.h"

char **installed_collections = NULL;

static scl_rc initialize_env()
{

    if (getenv("MODULEPATH") == NULL) {
        if(putenv("MODULEPATH=" SCL_MODULES_PATH) != 0) {
            debug("Impossible to create environment variable "
                "MODULEPATH: %s\n", strerror(errno));
            return ESYS;
        }
    }

    return EOK;
}

static scl_rc get_env_vars(const char *colname, char ***_vars)
{
    char *argv[] = {MODULE_CMD, MODULE_CMD, "sh", "add", "", NULL};
    char *output = NULL;
    int i = 0;
    char **parts, **vars;
    scl_rc ret = EOK;

    ret = initialize_env();
    if (ret != EOK) {
        return ret;
    }

    argv[4] = (char *) colname;
    output = get_command_output(argv[0], argv + 1, STDOUT_FILENO);
    if (output == NULL) {
        debug("Problem with executing program %s: %s\n", argv[0],
            strerror(errno));
        ret = ERUN;
        goto exit;
    }

    /*
     * Expected format of string stored in variable output is following:
     * var1=value1 ;export value1 ; var2=value2 ;export value2;...
     */

    vars = parts = split(output, ';');

    /* Filter out strings without "=" i. e. strings with export. */
    while (*parts !=  NULL) {
        if (strchr(*parts, '=')) {
            strip_trailing_chars(*parts, ' ');
            vars[i++] = xstrdup(*parts);
        }
        parts++;
    }
    vars[i] = NULL;
    *_vars = vars;

exit:
    output = _free(output);
    return ret;
}

scl_rc get_installed_collections(char *const **_colnames)
{
    char *argv[] = {MODULE_CMD, "modulecmd", "sh", "-t", "avail", NULL};
    char *output = NULL, **lines = NULL;
    int i2 = 0, take = 0;
    scl_rc ret = EOK;

    if (installed_collections != NULL) {
        *_colnames = installed_collections;
        return EOK;
    }

    ret = initialize_env();
    if (ret != EOK) {
        return ret;
    }

    output = get_command_output(argv[0], argv + 1, STDERR_FILENO);
    if (output == NULL) {
        debug("Problem with executing program %s: %s\n", argv[0],
            strerror(errno));
        ret = ERUN;
        goto exit;
    }
    /*
     * Expected format of string stored in variable output is following:
     * /usr/share/Modules/modulefiles:
     * modulename1
     * modulename2
     * ...
     * /etc/scl/modulefiles:
     * sclmodulename1
     * sclmodulename2
     * ...
     */

    lines = split(output, '\n');

    for (int i = 0; lines[i] != NULL; i++) {
        /* Line with path to modules on following lines */
        if (strchr(lines[i], ':') != NULL) {

            /* We want only modules with path /etc/scl/modulefiles */
            if (!strcmp(SCL_MODULES_PATH ":", lines[i])) {
                take = 1;
            } else {
                take = 0;
            }

        /* Line with module name */
        } else {
            if (take) {
                lines[i2++] = xstrdup(lines[i]);
            }
        }
    }
    lines[i2] = NULL;

    installed_collections = lines;
    *_colnames = installed_collections;

exit:
    output = _free(output);

    return ret;
}

static scl_rc collection_exists(const char *colname, bool *_exists)
{
    int ret = EOK;
    char *const *collnames = NULL;
    bool exists = false;

    ret = get_installed_collections(&collnames);
    if (ret != EOK) {
        return ret;
    }

    for (int i = 0; collnames[i] != NULL; i++) {
        if (!strcmp(collnames[i], colname)) {
            exists = true;
            break;
        }
    }
    *_exists = exists;

    return ret;
}


static scl_rc get_collection_path(const char *colname, char **_colpath)
{
    char **env = NULL;
    scl_rc ret;

    ret = get_env_vars(colname, &env);
    if (ret != EOK) {
        return ret;
    }

    ret = ECONFIG;
    for (int i = 0; env[i] != NULL; i++) {
        if (!strncmp("COLPATH=", env[i], 8)) {
            *_colpath = xstrdup(env[i] + 8);
            ret = EOK;
            break;
        }
    }
    if (ret == ECONFIG) {
        debug("Collection %s has not defined variable COLPATH\n", colname);
    }

    env = free_string_array(env);
    return ret;
}

scl_rc run_command(char * const colnames[], const char *cmd, bool exec)
{
    char **argv = NULL, **envs = NULL, *env = NULL;
    bool exists;
    scl_rc ret = EOK;
    int status;

    ret = initialize_env();
    if (ret != EOK) {
        return ret;
    }

    while (*colnames != NULL) {
        ret = collection_exists(*colnames, &exists);
        if (ret != EOK) {
            goto exit;
        }
        if (!exists) {
            debug("Collection %s doesn't exists!\n", *colnames);
            ret = EINPUT;
            goto exit;
        }

        ret = get_env_vars(*colnames, &envs);
        if (ret != EOK) {
            goto exit;
        }

        for (int i = 0; envs[i] != NULL; i++) {
            env = xstrdup(envs[i]);
            if(putenv(env) != 0) {
                env = _free(env);
                debug("Impossible to create environment variable %s: %s\n",
                    envs[i]);
                ret = ESYS;
                goto exit;
            }
        }
        envs = free_string_array(envs);
        colnames++;
    }

    if (exec) {
        /* Use function system */

        ret = prepare_args(cmd, &argv);
        if (ret != EOK) {
            goto exit;
        }
        execvp(argv[0], argv);
        debug("Problem with executing program %s: %s\n", argv[0], strerror(errno));
        ret = ERUN;

    } else {
        /* Use function system */

        status = system(cmd);
        if (status == -1 || !WIFEXITED(status)) {
            debug("Problem with executing program \"%s\"\n", cmd);
            ret = ERUN;
            goto exit;
        }
        ret = WEXITSTATUS(status);
    }


exit:
    argv = free_string_array(argv);
    envs = free_string_array(envs);

    return ret;
}

scl_rc list_packages_in_collection(const char *colname, char ***_pkgnames)
{
    char **lines;
    int lines_allocated = 10;
    int lines_count = 0;
    rpmts ts = NULL;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    char *provide = NULL;
    scl_rc ret = EOK;
    bool exists;

    ret = collection_exists(colname, &exists);
    if (ret != EOK) {
        return ret;
    }
    if (!exists) {
        debug("Collection %s doesn't exists!\n", colname);
        return EINPUT;
    }

    if (rpmReadConfigFiles(NULL, NULL) == -1) {
        debug("Error occured in rpmlib!\n");
        return ERPMLIB;
    }

    lines = xmalloc(lines_allocated * sizeof(*lines));

    xasprintf(&provide, "scl-package(%s)", colname);

    ts = rpmtsCreate();
    mi = rpmtsInitIterator(ts, RPMDBI_PROVIDENAME, provide, 0);

    do {
        lines_count++;
        if (lines_count > lines_allocated) {
            lines_allocated <<= 1;
            lines = xrealloc(lines, lines_allocated * sizeof(*lines));
        }

        h = rpmdbNextIterator(mi);
        if (h != NULL) {
            lines[lines_count - 1] = headerGetAsString(h, RPMTAG_NEVRA);
        } else {
            lines[lines_count - 1] = NULL;
        }
    } while (h != NULL);

    *_pkgnames = lines;

    mi = rpmdbFreeIterator(mi);
    ts = rpmtsFree(ts);
    provide = _free(provide);

    return ret;
}

static scl_rc run_scriptlet(const char *program_dir, const char *program_name)
{
    char *program = NULL;
    int status, pid = 0;
    scl_rc ret = EOK;

    xasprintf(&program, "%s/%s", program_dir, program_name);

    if (!access(program, F_OK)) {
        pid = fork();
        if (pid == -1) {
            ret = ERUN;
            debug("Fork failed: %s\n", strerror(errno));
            goto exit;
        } else if (pid == 0) {/* Child */
            execl(program, program, NULL);
            exit(EXIT_FAILURE);
        }

        waitpid(pid, &status, 0);
        if (!WIFEXITED(status)) {
            debug("Program %s didn't terminate normally!\n", program);
            ret = ERUN;
            goto exit;
        }
        if (WEXITSTATUS(status)) {
            debug("Program %s returned nonzero return code!\n", program);
            ret = ERUN;
            goto exit;
        }
    }

exit:
    program = _free(program);
    return ret;
}

scl_rc register_collection(const char *_colpath)
{
    scl_rc ret = EOK;
    char *colname, *colpath = NULL, *colroot = NULL;
    char *module_file = NULL, *module_file_link = NULL;
    bool exists;

    colpath = xstrdup(_colpath);
    strip_trailing_chars(colpath, '/');
    colname = basename(colpath);

    xasprintf(&module_file, "%s/%s", colpath, colname);
    xasprintf(&module_file_link, SCL_MODULES_PATH "/%s", colname);
    xasprintf(&colroot, "%s/root", colpath);

    if (access(module_file, F_OK) == -1 || access(colroot, F_OK) == -1) {
        debug("Collection %s is not valid! File %s or file %s doesn't exists: %s\n",
            colname, module_file, colroot, strerror(errno));
        ret = EINPUT;
        goto exit;
    }

    ret = collection_exists(colname, &exists);
    if (ret != EOK) {
        goto exit;
    }
    if (exists) {
        debug("Collection %s has already been registered!\n", colname);
        ret = EINPUT;
        goto exit;
    }

    if (symlink(module_file, module_file_link) == -1) {
        debug("Unable to create symlink %s to file %s: %s\n",
            module_file_link, module_file, strerror(errno));
        ret = EDISK;
        goto exit;
    }

    ret = run_scriptlet(colpath, "register");
    if (ret != EOK) {
        if (unlink(module_file_link)) {
            debug("Unable to remove file %s: %s\n", module_file_link,
                strerror(errno));
            debug("Remove this file manualy before a new try to register collection\n");
        }
    }

exit:
    colpath = _free(colpath);
    module_file = _free(module_file);
    module_file_link = _free(module_file_link);
    return ret;
}

static scl_rc owned_by_package(const char *file_path, bool *_owned)
{
    rpmts ts;
    rpmdbMatchIterator mi;
    scl_rc ret = EOK;

    if (rpmReadConfigFiles(NULL, NULL) == -1) {
        debug("Error occured in rpmlib!\n");
        return ERPMLIB;
    }

    ts = rpmtsCreate();
    mi = rpmtsInitIterator(ts, RPMDBI_INSTFILENAMES, file_path, 0);

    *_owned = rpmdbGetIteratorCount(mi) > 0;

    mi = rpmdbFreeIterator(mi);
    ts = rpmtsFree(ts);

    return ret;
}

scl_rc deregister_collection(const char *colname, bool force)
{
    bool exists;
    scl_rc ret = EOK;
    char *module_file_link = NULL;
    bool owned;
    char *colpath;

    ret = collection_exists(colname, &exists);
    if (ret != EOK) {
        return ret;
    }
    if (!exists) {
        debug("Collection %s doesn't exists!\n", colname);
        return EINPUT;
    }

    ret = get_collection_path(colname, &colpath);
    if (ret != EOK) {
        return ret;
    }

    xasprintf(&module_file_link, SCL_MODULES_PATH "/%s", colname);

    if (!force) {
        ret = owned_by_package(module_file_link, &owned);
        if (ret != EOK) {
            goto exit;
        }

        if (owned) {
            debug("Unable to deregister collection %s: Collection was "
                "installed as a package, you can use '--force' to"
                "deregister it.\n", colname);
            ret = EINPUT;
            goto exit;
        }

    }

    ret = run_scriptlet(colpath, "deregister");
    if (ret != EOK) {
        goto exit;
    }

    if (unlink(module_file_link) == -1) {
        debug("Unable to remove file %s: %s\n", module_file_link,
            strerror(errno));
        ret = ESYS;
        goto exit;
    }


exit:
    module_file_link = _free(module_file_link);
    colpath = _free(colpath);
    return ret;
}

scl_rc show_man(const char *colname)
{
    scl_rc ret = EOK;
    char *colnames[] = {(char *) colname, NULL};
    char *cmd = NULL;
    bool exists;

    ret = collection_exists(colname, &exists);
    if (ret != EOK) {
        return ret;
    }
    if (!exists) {
        debug("Collection %s doesn't exists!\n", colname);
        return EINPUT;
    }

    xasprintf(&cmd, "man %s", colname);
    ret = run_command(colnames, cmd, true);

    cmd = _free(cmd);
    return ret;
}

void release_scllib_cache()
{
    installed_collections = free_string_array(installed_collections);
}
