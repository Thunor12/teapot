#define NOB_IMPLEMENTATION
#include "nob.h"

#ifdef _WIN32
#define CC "gcc.exe"
#else
#define CC "gcc"
#endif

#define BUILD_DIR "./build/"
#define TEST_DIR "./tests/"

#define COMPILE_FLAGS "-O2", "-g",                                                                           \
                      "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Wconversion", "-Wimplicit-fallthrough", \
                      "-Wshadow", "-Wpointer-arith", "-Wcast-qual", "-Wstrict-prototypes",                   \
                      "-D_FORTIFY_SOURCE=2",                                                                 \
                      "-D_GLIBCXX_ASSERTIONS",                                                               \
                      "-fexceptions",                                                                        \
                      "-fstack-clash-protection",                                                            \
                      "-fstack-protector-strong",                                                            \
                      "-std=c17"

#ifdef _WIN32
#define LINK_FLAGS "-O2", "-lws2_32"
#else
#define LINK_FLAGS "-O2"
#endif

Nob_Procs procs = {0};

static int compile_exe(
    const char **source_files, size_t src_count, //
    const char **deps, size_t deps_count,        //
    const char *output_file                      //
)
{
    int ret = 0;
    Nob_Cmd cmd = {0};
    Nob_File_Paths dep_files = {0};

#ifdef _WIN32
    nob_cmd_append(&cmd, CC);
#else
    nob_cc(&cmd);
#endif

    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, COMPILE_FLAGS);

    nob_cc_output(&cmd, output_file);

    for (size_t i = 0; i < src_count; i++)
    {
        nob_cc_inputs(&cmd, source_files[i]);
        nob_da_append(&dep_files, source_files[i]);
    }

    for (size_t i = 0; i < deps_count; i++)
    {
        nob_da_append(&dep_files, deps[i]);
    }

    nob_cmd_append(&cmd, LINK_FLAGS);

    if (!nob_needs_rebuild(output_file, dep_files.items, dep_files.count))
    {
        nob_log(NOB_INFO, "%s up to date", output_file);
        ret = 0;
        goto defer;
    }

    // if (!nob_cmd_run(&cmd))
    if (!nob_cmd_run(&cmd, .async = &procs))
    {
        ret = 1;
        goto defer;
    }

defer:
    nob_cmd_free(cmd);
    nob_da_free(dep_files);
    return 0;
}

const char *tests[] = {
    TEST_DIR "low_level_test_stb_teapot.c",
    TEST_DIR "header_parse.c",
};

static int compile_tests(const char **tests, size_t test_count)
{
    int ret = 0;
    Nob_String_Builder sb = {0};

    for (size_t i = 0; i < test_count; i++)
    {
        char temp[260] = {0};
        memset(sb.items, 0, sb.count);
        sb.count = 0;

        const char *test_source = tests[i];
        const char *test_name = nob_path_name(test_source);
        sprintf(temp, "%s", test_name);

        char *exe = strtok(temp, ".c");

        nob_sb_appendf(&sb, BUILD_DIR "%s", exe);

#ifdef _WIN32
        nob_sb_append_cstr(&sb, ".exe");
#endif
        nob_sb_append_null(&sb);

        const char **deps = (const char *[]){
            "stb_teapot.h",
        };

        if (0 != compile_exe(&test_source, 1, deps, NOB_ARRAY_LEN(deps), sb.items))
        {
            ret = 1;
            goto defer;
        }
    }

defer:
    nob_procs_flush(&procs);
    nob_sb_free(sb);
    return ret;
}

int main(int argc, char **argv)
{
    int ret = 0;
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_mkdir_if_not_exists(BUILD_DIR);

    nob_shift_args(&argc, &argv); // Ignore program name

    // Parse args
    for (size_t i = 1; i <= argc; i++)
    {
        char *cmd_arg = nob_shift_args(&argc, &argv);

        if (0 == strcmp(cmd_arg, "clean"))
        {
            nob_log(NOB_INFO, "Cleaning...");
            nob_delete_file(BUILD_DIR);
            goto defer;
        }
    }

    if (0 != compile_tests(tests, NOB_ARRAY_LEN(tests)))
    {
        ret = 1;
    }

defer:
    return ret;
}
