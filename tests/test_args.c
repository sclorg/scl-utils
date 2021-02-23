#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdbool.h>

#include "test_common.h"
#include "../src/args.h"
#include "../src/errors.h"


static void test_scl_args_get_basic_args(void **state)
{
    (void) state; /* unused */
    int argc;
    char **argv;
    struct scl_args *args;
    scl_rc ret;

    /* test list-collections argument */
    argv = (char *[]) {"scl", "list-collections"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_LIST_COLLECTIONS);
    scl_args_free(args);

    /* test --help argument */
    argv = (char *[]) {"scl", "--help"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_NONE);
    scl_args_free(args);

    /* test list-packages without defining collection, it should return EINPUT */
    argv = (char *[]) {"scl", "list-packages"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test list-packages argument */
    argv = (char *[]) {"scl", "list-packages", "collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_LIST_PACKAGES);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection", NULL}));
    scl_args_free(args);

    /* test man without defining collection, it should return EINPUT */
    argv = (char *[]) {"scl", "man"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test man argument */
    argv = (char *[]) {"scl", "man", "collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_MAN);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection", NULL}));
    scl_args_free(args);
}

static void test_scl_args_get_register_deregister(void **state)
{
    (void) state; /* unused */
    int argc;
    char **argv;
    struct scl_args *args;
    scl_rc ret;

    /* test deregister without defining collection. It should return EINPUT */
    argv = (char *[]) {"scl", "deregister"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test deregister argument */
    argv = (char *[]) {"scl", "deregister", "collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_DEREGISTER);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection", NULL}));
    scl_args_free(args);

    /* test "deregister -f" without defining collection. It should return EINPUT */
    argv = (char *[]) {"scl", "deregister", "-f"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test "deregister -f" arguments */
    argv = (char *[]) {"scl", "deregister", "-f", "collection"};
    argc = 4;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_DEREGISTER);
    assert_true(args->force_flag);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection", NULL}));
    scl_args_free(args);

    /* test deregister without defining path to collection. It should return EINPUT */
    argv = (char *[]) {"scl", "register"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test deregister with relative path. It should return EINPUT */
    argv = (char *[]) {"scl", "register", "colection_dir/collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* test deregister argument */
    argv = (char *[]) {"scl", "register", "/path/to/collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_REGISTER);
    assert_true(compare_string_arrays(args->colpaths,
        (char *[]) {"/path/to/collection", NULL}));
    scl_args_free(args);
}

static void test_scl_args_get_run_command(void **state)
{
    (void) state; /* unused */
    int argc;
    char **argv;
    struct scl_args *args;
    scl_rc ret;

    /* Test wrong argument for scl run */
    argv = (char *[]) {"scl", "run"};
    argc = 2;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    argv = (char *[]) {"scl", "run", "collection"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    argv = (char *[]) {"scl", "run", "-x", "collection"};
    argc = 4;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EINPUT);

    /* Test run with one collection */
    argv = (char *[]) {"scl", "run", "collection", "cmd"};
    argc = 4;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection", NULL}));
    assert_string_equal(args->command, "cmd");
    scl_args_free(args);

    /* Test run with multiple collections */
    argv = (char *[]) {"scl", "run", "collection1", "collection2",
        "collection3", "cmd"};
    argc = 6;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", "collection3", NULL}));
    assert_string_equal(args->command, "cmd");
    scl_args_free(args);

    /* Test run with compound command */
    argv = (char *[]) {"scl", "run", "collection1", "collection2",
        "cmd arg1 arg2"};
    argc = 5;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", NULL}));
    assert_string_equal(args->command, "cmd arg1 arg2");
    scl_args_free(args);

    /* Test run with separator */
    argv = (char *[]) {"scl", "run", "collection1", "collection2",
        "--", "cmd", "arg1 with spaces", "arg2"};
    argc = 8;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", NULL}));
    assert_string_equal(args->command, "cmd \"arg1 with spaces\" \"arg2\"");
    scl_args_free(args); 

    /* Test run in format of shebang */
    argv = (char *[]) {"scl", strdup("run collection1 collection2 -- python"), "test.py"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", NULL}));
    assert_string_equal(args->command, "python \"test.py\"");
    free(argv[1]);
    scl_args_free(args);

    argv = (char *[]) {"scl", strdup("run collection1 collection2 -- python -O"), "test.py"};
    argc = 3;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", NULL}));
    assert_string_equal(args->command, "python \"-O\" \"test.py\"");
    free(argv[1]);
    scl_args_free(args);

    argv = (char *[]) {"scl", strdup("run collection1 collection2 -- python"),
        "test.py", "argv1", "argv2"};
    argc = 5;
    ret = scl_args_get(argc, argv, &args);
    assert_int_equal(ret, EOK);
    assert_int_equal(args->action, ACTION_COMMAND);
    assert_true(compare_string_arrays(args->collections,
        (char *[]) {"collection1", "collection2", NULL}));
    assert_string_equal(args->command, "python \"test.py\" \"argv1\" \"argv2\"");
    free(argv[1]);
    scl_args_free(args);
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_scl_args_get_basic_args),
        cmocka_unit_test(test_scl_args_get_register_deregister),
        cmocka_unit_test(test_scl_args_get_run_command),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
