#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef _WIN32
#define CC "gcc.exe"
#else
#define CC "gcc"
#endif

#define BUILD_DIR "./build/"
#define TEST_DIR "./tests/"

#ifdef _WIN32
#define LINK_FLAGS "-lws2_32"
#else
#define LINK_FLAGS ""
#endif

static int compile_exe(const char **source_files, size_t src_count, const char *output_file)
{
    int ret = 0;
    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, CC);
    nob_cc_flags(&cmd);

    nob_cc_output(&cmd, output_file);

    for (size_t i = 0; i < src_count; i++)
    {
        nob_cc_inputs(&cmd, source_files[i]);
    }

    nob_cmd_append(&cmd, LINK_FLAGS);

    if (!nob_needs_rebuild(output_file, source_files, src_count))
    {
        nob_log(NOB_INFO, "%s up to date", output_file);
        ret = 1;
        goto defer;
    }

    if (!nob_cmd_run(&cmd))
    {
        ret = 1;
        goto defer;
    }

defer:
    nob_cmd_free(cmd);
    return 0;
}

const char *tests[] = {
    TEST_DIR "low_level_test.c",
    TEST_DIR "low_level_test_2.c",
    TEST_DIR "low_level_test_stb_teapot.c",
};

static int compile_tests(const char **tests, size_t test_count)
{
    int ret = 0;

    for (size_t i = 0; i < test_count; i++)
    {
        const char *test_source = tests[i];
        const char *test_name = nob_path_name(test_source);
        char output_path[MAX_PATH];

#ifdef _WIN32
        snprintf(output_path, sizeof(output_path), "%s%s.exe", BUILD_DIR, test_name);
#else
        snprintf(output_path, sizeof(output_path), "%s%s", BUILD_DIR, test_name);
#endif

        if (0 != compile_exe(&test_source, 1, output_path))
        {
            ret = 1;
            goto defer;
        }
    }

defer:
    return ret;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_mkdir_if_not_exists(BUILD_DIR);

    if (0 != compile_tests(tests, sizeof(tests) / sizeof(tests[0])))
    {
        return 1;
    }

    return 0;
}
