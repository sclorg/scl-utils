#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "test_common.h"
#include "dict.h"
#include "../src/scllib.h"
#include "../src/errors.h"

extern int __real_putenv();
extern char *__real_getenv();
dict env;

int __wrap_putenv(char *string)
{
    if (env)
        return dict_put(&env, string);
    return __real_putenv(string);
}

char *__wrap_getenv(const char *name)
{
    char *value;
    if (env && (value = dict_get(env, name)))
        return value;
    return __real_getenv(name);
}

char *__wrap_get_command_output(const char *path, char *const argv[], int fileno)
{
    return mock_ptr_type(char *);
}

typedef struct {
    char *cmd_output; /* value that will be set as return value of get_command_output() */
    char **expected_colnames; /* expected output value from get_installed_collections() */
    scl_rc ret; /* expected return value from get_installed_collections() */
} inst_col_test_case;

static void test_get_installed_collections(void **state)
{
    (void) state; /* unused */
    char *const *colnames;
    char *output_allocated;
    scl_rc ret;


    inst_col_test_case testcases[] = {
        /* Collections are in the end of output */
        {
            .cmd_output =
                "/usr/share/Modules/modulefiles:\n"
                "modulename1\n"
                "modulename2\n"
                "modulename3\n"
                "/etc/scl/modulefiles:\n"
                "scl1\n"
                "scl2\n"
                "scl3\n"
                "scl4\n",

            .expected_colnames = (char *[]){
                "scl1",
                "scl2",
                "scl3",
                "scl4",
                NULL,
            },

            .ret = EOK,
        },

        /* Collections are in the middle of output */
        {
            .cmd_output =
                "/usr/share/Modules/modulefiles:\n"
                "modulename1\n"
                "modulename2\n"
                "modulename3\n"
                "/etc/scl/modulefiles:\n"
                "sclA\n"
                "sclB\n"
                "sclC\n"
                "/etc/modulefiles:\n"
                "modulename4\n"
                "modulename5\n",

            .expected_colnames = (char *[]){
                "sclA",
                "sclB",
                "sclC",
                NULL,
            },

            .ret = EOK,
        },

        /* Collections are in the beginning of output */
        {
            .cmd_output =
                "/etc/scl/modulefiles:\n"
                "sclX\n"
                "sclY\n"
                "/etc/modulefiles:\n"
                "modulename4\n"
                "modulename5\n",

            .expected_colnames = (char *[]){
                "sclX",
                "sclY",
                NULL,
            },

            .ret = EOK,
        },

        /* No collection is installed */
        {
            .cmd_output =
                "/usr/share/Modules/modulefiles:\n"
                "modulename1\n"
                "modulename2\n"
                "modulename3\n"
                "/etc/scl/modulefiles:\n",

            .expected_colnames = (char *[]){
                NULL,
            },

            .ret = EOK,
        },

        /* get_command_output returns NULL (indicate error) */
        {
            .cmd_output = NULL,
            .expected_colnames = NULL,
            .ret = ERUN,
        },

    };

    int tc_count = sizeof(testcases) / sizeof(testcases[0]);


    for (int i = 0; i < tc_count; i++) {

        release_scllib_cache();

        if (testcases[i].cmd_output != NULL) {
            output_allocated = malloc(strlen(testcases[i].cmd_output) + 1);
            strcpy(output_allocated, testcases[i].cmd_output);
        } else {
            output_allocated = NULL;
        }

        will_return(__wrap_get_command_output, output_allocated);
        ret = get_installed_collections(&colnames);

        assert_int_equal(ret, testcases[i].ret);
        if (ret == EOK) {
            assert_true(compare_string_arrays(colnames,
                testcases[i].expected_colnames));
        }

    }
}

int  __wrap_system(const char *command)
{
    char *env_path;

    env_path = getenv("PATH");
    assert_true(strstr(env_path, mock_ptr_type(char *)) != NULL);


    return 0;
}

typedef struct {
    char *col_list;
    char *env_vars;
    char **collections;
    char *expected_env_path;
    scl_rc ret;
} run_command_testcase;

static void test_run_command(void **state)
{
    (void) state; /* unused */
    scl_rc ret;

    run_command_testcase testcases[] = {
        /* One collection enabled to run program */
        {
            .col_list =
                "/usr/share/Modules/modulefiles:\n"
                "modulename1\n"
                "modulename2\n"
                "modulename3\n"
                "/etc/scl/modulefiles:\n"
                "scl1\n"
                "scl2\n"
                "scl3\n"
                "scl4\n",

            .env_vars = "PATH=/opt/rh/scl1/root/usr/bin ;export PATH",
            .expected_env_path = "/opt/rh/scl1/root/usr/bin",
            .collections = (char *[]) {"scl1", NULL},
            .ret = EOK,
        },

        /* Try to enable non-existing collection */
        {
            .col_list =
                "/usr/share/Modules/modulefiles:\n"
                "modulename1\n"
                "modulename2\n"
                "modulename3\n"
                "/etc/scl/modulefiles:\n"
                "scl1\n"
                "scl2\n"
                "scl3\n"
                "scl4\n",

            .env_vars = NULL,
            .expected_env_path = NULL,
            .collections = (char *[]) {"scl5", NULL},
            .ret = EINPUT,
        },
    };

    int tc_count = sizeof(testcases) / sizeof(testcases[0]);

    for (int i = 0; i < tc_count; i++) {
        env = dict_init();
        release_scllib_cache();

        if (testcases[i].col_list)
            will_return(__wrap_get_command_output, strdup(testcases[i].col_list));

        if (testcases[i].env_vars)
            will_return(__wrap_get_command_output, strdup(testcases[i].env_vars));

        if (testcases[i].expected_env_path)
            will_return(__wrap_system, testcases[i].expected_env_path);


        ret = run_command(testcases[i].collections, "test_cmd", false);
        assert_int_equal(ret, testcases[i].ret);
        dict_free(env);
    }

}

int main(void)
{
    const UnitTest tests[] = {
        unit_test(test_get_installed_collections),
        unit_test(test_run_command),
    };

    return run_tests(tests);
}
