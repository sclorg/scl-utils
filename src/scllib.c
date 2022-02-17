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
#include <signal.h>

#include "config.h"
#include "errors.h"
#include "scllib.h"
#include "debug.h"
#include "lib_common.h"
#include "sclmalloc.h"
#include "fallback.h"
#include "ctype.h"

char **installed_collections = NULL;

static scl_rc initialize_env()
{
    char *module_path = getenv("MODULEPATH");
    char *new_module_env;

    if (!module_path || !strstr(module_path, SCL_MODULES_PATH)) {

        if (module_path) {
            xasprintf(&new_module_env, "MODULEPATH=" SCL_MODULES_PATH ":%s",
                module_path);
        } else {
            xasprintf(&new_module_env, "MODULEPATH=" SCL_MODULES_PATH);
        }

        if(putenv(new_module_env) != 0) {
            debug("Impossible to create environment variable %s: %s\n",
                new_module_env, strerror(errno));
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
    char **parts, *part, **vars;
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
     * var1=value1 ;export value1 ; var2=value2 ;export value2;
     * var3=value\ with\ spaces
     * NOTE: Newer (tcl-based) versions of MODULE_CMD put a newline after each
     * export command so we need to take that into account.
     */

    vars = parts = split(output, ';');

    /* Filter out strings without "=" i. e. strings with export. */
    while (*parts != NULL) {
        part = *parts;
        if (part[0] == '\n')
            part++;
        if (strchr(part, '=')) {
            strip_trailing_chars(part, ' ');
            unescape_string(part);
            vars[i++] = xstrdup(part);
        }
        parts++;
    }
    vars[i] = NULL;
    *_vars = vars;

exit:
    output = _free(output);
    return ret;
}
scl_rc get_enabled_collections(char ***_enabled_collections)
{
    char **enabled_collections = NULL;
    char *lm_files = getenv("_LMFILES_");

    if (lm_files != NULL) {
        lm_files = xstrdup(lm_files);
        enabled_collections = split(lm_files, ':');

        for (int i = 0; enabled_collections[i] != NULL; i++) {
            if (!strncmp(SCL_MODULES_PATH, enabled_collections[i],
                sizeof(SCL_MODULES_PATH - 1))){

                enabled_collections[i] += sizeof(SCL_MODULES_PATH);
            }
            enabled_collections[i] = xstrdup(enabled_collections[i]);
        }

    }
    lm_files = _free(lm_files);
    *_enabled_collections = enabled_collections;
    return EOK;
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

/*
 * Return true in output parameter _exists if a collection given by parameter
 * colname exists. This function works only for new type of collections i. e.
 * collections containing module file. In other words it returns true only
 * when a given collection exists and the collection is collection of new
 * type otherwise it returns false.
 *
 * There is also function fallback_collection_exists() which
 * returns true for all existing collections no matter of their types.
 */
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

scl_rc get_collection_path(const char *colname, char **_colpath)
{
    FILE *fp = NULL;
    char *file_path = NULL;
    char *prefix = NULL;
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

    prefix = xcalloc(st.st_size + 1, 1);
    if (fread(prefix, st.st_size, 1, fp) != 1) {
        debug("Unable to read file %s: %s\n", file_path, strerror(errno));
        ret = EDISK;
        goto exit;
    }

    for (int i = st.st_size - 1; i >= 0; i--) {
        if (isspace(prefix[i]) || prefix[i] == '/') {
            prefix[i] = '\0';
        } else {
            break;
        }
    }

    xasprintf(&colpath, "%s/%s", prefix, colname);

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
        if (fallback_is_collection_enabled(*colnames)) {
            colnames++;
            continue;
        }

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
            if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
                goto exit;
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
    char **srpms, **rpms;
    int srpms_allocated = 10, rpms_allocated = 10;
    int srpms_count = 0, rpms_count = 0;

    rpmts ts = NULL;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    char *provide = NULL;
    scl_rc ret = EOK;
    bool exists;
    const char *srpm;

    ret = fallback_collection_exists(colname, &exists);
    if (ret != EOK) {
        return ret;
    }

    if (!exists) {
        debug("Collection %s doesn't exists!\n", colname);
        return EINPUT;
    }

    if (rpmReadConfigFiles(NULL, NULL) == -1) {
        debug("Error occurred in rpmlib!\n");
        return ERPMLIB;
    }

    srpms = xmalloc(srpms_allocated * sizeof(*srpms));

    xasprintf(&provide, "scl-package(%s)", colname);

    ts = rpmtsCreate();
    mi = rpmtsInitIterator(ts, RPMDBI_PROVIDENAME, provide, 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {

        srpms[srpms_count++] = headerGetAsString(h, RPMTAG_SOURCERPM);

        if (srpms_count == srpms_allocated) {
            srpms_allocated <<= 1;
            srpms = xrealloc(srpms, srpms_allocated * sizeof(*srpms));
        }
    }
    srpms[srpms_count] = NULL;
    mi = rpmdbFreeIterator(mi);
    provide = _free(provide);

    rpms = xmalloc(rpms_allocated * sizeof(*rpms));
    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {

        srpm = headerGetString(h, RPMTAG_SOURCERPM);

        for (int i = 0; i < srpms_count; i++) {
            if (!strcmp(srpm, srpms[i])) {
                rpms[rpms_count++] = headerGetAsString(h, RPMTAG_NEVRA);
                break;
            }
        }

        if (rpms_count == rpms_allocated) {
            rpms_allocated <<= 1;
            rpms = xrealloc(rpms, rpms_allocated * sizeof(*rpms));
        }
    }
    rpms[rpms_count] = NULL;
    mi = rpmdbFreeIterator(mi);
    ts = rpmtsFree(ts);
    srpms = free_string_array(srpms);

    *_pkgnames = rpms;
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
    char *enable_script = NULL, *conf_file = NULL;
    char *prefix = NULL;
    FILE *f = NULL;
    bool exists;

    colpath = xstrdup(_colpath);
    strip_trailing_chars(colpath, '/');
    colname = basename(colpath);
    prefix = directory_name(colpath);

    xasprintf(&module_file, "%s/%s", colpath, colname);
    xasprintf(&enable_script, "%s/enable", colpath);
    xasprintf(&module_file_link, SCL_MODULES_PATH "/%s", colname);
    xasprintf(&conf_file, SCL_CONF_DIR "/%s", colname);
    xasprintf(&colroot, "%s/root", colpath);

    if (access(enable_script, F_OK) == -1 || access(colroot, F_OK) == -1) {
        debug("Collection %s is not valid! File %s or file %s doesn't exists: %s\n",
            colname, enable_script, colroot, strerror(errno));
        ret = EINPUT;
        goto exit;
    }

    ret = fallback_collection_exists(colname, &exists);
    if (ret != EOK) {
        goto exit;
    }
    if (exists) {
        debug("Collection %s has already been registered!\n", colname);
        ret = EINPUT;
        goto exit;
    }

    f = fopen(conf_file, "w+");
    if (f == NULL) {
        debug("Unable to open file %s: %s\n", conf_file, strerror(errno));
        ret = EDISK;
        goto exit;
    }
    if (fprintf(f, "%s\n", prefix) < 0) {
        debug("Unable to write to file %s: %s\n", conf_file, strerror(errno));
        ret = EDISK;
        goto exit;
    }
    fclose(f);
    f = NULL;

    if (access(module_file, F_OK) == 0) {
        if (symlink(module_file, module_file_link) == -1) {
            debug("Unable to create symlink %s to file %s: %s\n",
                module_file_link, module_file, strerror(errno));
            ret = EDISK;
            goto exit;
        }
    }

    ret = run_scriptlet(colpath, "register");
    if (ret != EOK) {
        if (unlink(conf_file)) {
            debug("Unable to remove file %s: %s\n", conf_file,
                strerror(errno));
            debug("Remove this file manually before a new try to register collection\n");
        }

        if (access(module_file_link, F_OK) == 0) {
            if (unlink(module_file_link)) {
                debug("Unable to remove file %s: %s\n", module_file_link,
                    strerror(errno));
                debug("Remove this file manually before a new try to register collection\n");
            }
        }
    }

exit:
    if (f != NULL) {
        fclose(f);
        f = NULL;
    }
    colpath = _free(colpath);
    module_file = _free(module_file);
    module_file_link = _free(module_file_link);
    enable_script = _free(enable_script);
    prefix = _free(prefix);

    return ret;
}

static scl_rc owned_by_package(const char *file_path, bool *_owned)
{
    rpmts ts;
    rpmdbMatchIterator mi;
    scl_rc ret = EOK;

    if (rpmReadConfigFiles(NULL, NULL) == -1) {
        debug("Error occurred in rpmlib!\n");
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
    char *module_file_link = NULL, *conf_file = NULL;
    bool conf_file_owned = false, module_file_owned = false;
    char *colpath;

    ret = fallback_collection_exists(colname, &exists);
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
    xasprintf(&conf_file, SCL_CONF_DIR "/%s", colname);

    if (!force) {
        ret = owned_by_package(conf_file, &conf_file_owned);
        if (ret != EOK) {
            goto exit;
        }

        ret = owned_by_package(module_file_link, &module_file_owned);
        if (ret != EOK) {
            goto exit;
        }

        if (module_file_owned || conf_file_owned) {
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

    if (unlink(conf_file) == -1) {
        debug("Unable to remove file %s: %s\n", conf_file,
            strerror(errno));
        ret = ESYS;
        goto exit;
    }

    if (access(module_file_link, F_OK) == 0) {
        if (unlink(module_file_link) == -1) {
            debug("Unable to remove file %s: %s\n", module_file_link,
                strerror(errno));
            ret = ESYS;
            goto exit;
        }
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
    bool need_fallback = false;

    ret = collection_exists(colname, &exists);
    if (ret != EOK) {
        return ret;
    }

    if (!exists) {
        ret = fallback_collection_exists(colname, &exists);
        if (ret != EOK) {
            return ret;
        }
        need_fallback = true;
    }

    if (!exists) {
        debug("Collection %s doesn't exists!\n", colname);
        return EINPUT;
    }

    xasprintf(&cmd, "man %s", colname);

    if (need_fallback) {
        ret = fallback_run_command(colnames, cmd, true);
    } else {
        ret = run_command(colnames, cmd, true);
    }

    cmd = _free(cmd);
    return ret;
}

const char* get_version() {
    return SCL_VERSION;
}

void release_scllib_cache()
{
    installed_collections = free_string_array(installed_collections);
}
