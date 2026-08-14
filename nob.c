#define NOB_IMPLEMENTATION
#include "third_party/nob/nob.h"

#ifdef _WIN32
#define CC "gcc.exe"
#else
#define CC "gcc"
#endif

#define BUILD_DIR "./build/"
#define TEST_DIR "./tests/"
#define EXAMPLE_DIR "./examples/"

#define DOCS_EMBED_SRC EXAMPLE_DIR "docs"
#define DOCS_EMBED_OUT BUILD_DIR "docs_embed.h"
#define TEST_EMBED_SRC TEST_DIR "fixtures/embed_docs"
#define TEST_EMBED_OUT BUILD_DIR "test_embed.h"

#define COMPILE_FLAGS "-O2", "-g",                                                                           \
                      "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Wconversion", "-Wimplicit-fallthrough", \
                      "-Wshadow", "-Wpointer-arith", "-Wcast-qual", "-Wstrict-prototypes",                   \
                      "-Wno-missing-field-initializers",                                                     \
                      "-D_FORTIFY_SOURCE=2",                                                                 \
                      "-D_GLIBCXX_ASSERTIONS",                                                               \
                      "-fexceptions",                                                                        \
                      "-fstack-clash-protection",                                                            \
                      "-fstack-protector-strong",                                                            \
                      "-std=c17"

#ifdef _WIN32
#define LINK_FLAGS "-O2", "-lws2_32"
#else
#define LINK_FLAGS "-O2", "-pthread"
#endif

Nob_Procs procs = {0};

static void strip_c_suffix(char *name)
{
    size_t n = strlen(name);
    if (n >= 2 && name[n - 2] == '.' && name[n - 1] == 'c')
    {
        name[n - 2] = '\0';
    }
}

static int compile_exe(
    const char **source_files, size_t src_count, //
    const char **deps, size_t deps_count,        //
    const char *output_file,                     //
    int with_build_include                       //
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
    if (with_build_include)
        nob_cmd_append(&cmd, "-I", "build");

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

    if (!nob_cmd_run(&cmd, .async = &procs))
    {
        ret = 1;
        goto defer;
    }

defer:
    nob_cmd_free(cmd);
    nob_da_free(dep_files);
    return ret;
}

const char *tests_and_examples[] = {
    TEST_DIR "unit_test_headers.c",
    TEST_DIR "unit_test_request.c",
    TEST_DIR "unit_test_response.c",
    TEST_DIR "unit_test_timeout.c",
    TEST_DIR "unit_test_send_timeout.c",
    TEST_DIR "unit_test_listen.c",
    TEST_DIR "unit_test_run.c",
#ifdef __linux__
    TEST_DIR "unit_test_run_epoll.c",
#endif
    TEST_DIR "unit_test_embed.c",
    EXAMPLE_DIR "basic_server.c",
#ifdef __linux__
    EXAMPLE_DIR "epoll_server.c",
#endif
    EXAMPLE_DIR "threaded_server.c",
    EXAMPLE_DIR "thread_pool_server_crossplat.c",
};

static const char *unit_tests[] = {
    BUILD_DIR "unit_test_headers",
    BUILD_DIR "unit_test_request",
    BUILD_DIR "unit_test_response",
    BUILD_DIR "unit_test_timeout",
    BUILD_DIR "unit_test_send_timeout",
    BUILD_DIR "unit_test_listen",
    BUILD_DIR "unit_test_run",
#ifdef __linux__
    BUILD_DIR "unit_test_run_epoll",
#endif
    BUILD_DIR "unit_test_embed",
};

static int compile_all_exe(const char **exes, size_t test_count)
{
    int ret = 0;
    Nob_String_Builder sb = {0};

    for (size_t i = 0; i < test_count; i++)
    {
        char temp[260] = {0};
        memset(sb.items, 0, sb.count);
        sb.count = 0;

        const char *exe_source = exes[i];
        const char *exe_name = nob_path_name(exe_source);
        sprintf(temp, "%s", exe_name);
        strip_c_suffix(temp);

        nob_sb_appendf(&sb, BUILD_DIR "%s", temp);

#ifdef _WIN32
        nob_sb_append_cstr(&sb, ".exe");
#endif
        nob_sb_append_null(&sb);

        const char *deps_default[] = {
            "stb_teapot.h",
        };
        const char *deps_embed[] = {
            "stb_teapot.h",
            TEST_EMBED_OUT,
        };
        int is_embed = 0 == strcmp(temp, "unit_test_embed");
        const char **deps = is_embed ? deps_embed : deps_default;
        size_t deps_count = is_embed ? NOB_ARRAY_LEN(deps_embed) : NOB_ARRAY_LEN(deps_default);

        if (0 != compile_exe(&exe_source, 1, deps, deps_count, sb.items, 0))
        {
            ret = 1;
            goto defer;
        }
    }

defer:
    if (!nob_procs_flush(&procs))
    {
        ret = 1;
    }
    nob_sb_free(sb);
    return ret;
}

/* OS-gated interactive docs binaries. Compiled but never run from ./nob. */
static const char *docs_examples[] = {
#ifdef __linux__
    EXAMPLE_DIR "teapot_docs_epoll.c",
#endif
#if !defined(_WIN32)
    EXAMPLE_DIR "teapot_docs_poll.c",
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
    EXAMPLE_DIR "teapot_docs_kqueue.c",
#endif
#ifdef _WIN32
    EXAMPLE_DIR "teapot_docs_wsapoll.c",
    EXAMPLE_DIR "teapot_docs_wfmo.c",
#endif
};

static int compile_docs_examples(void)
{
    int ret = 0;
    Nob_String_Builder sb = {0};
    const char *deps[] = {
        "stb_teapot.h",
        DOCS_EMBED_OUT,
        EXAMPLE_DIR "docs_app.h",
    };

    for (size_t i = 0; i < NOB_ARRAY_LEN(docs_examples); i++)
    {
        char temp[260] = {0};
        memset(sb.items, 0, sb.count);
        sb.count = 0;

        const char *exe_source = docs_examples[i];
        const char *exe_name = nob_path_name(exe_source);
        sprintf(temp, "%s", exe_name);
        strip_c_suffix(temp);

        nob_sb_appendf(&sb, BUILD_DIR "%s", temp);
#ifdef _WIN32
        nob_sb_append_cstr(&sb, ".exe");
#endif
        nob_sb_append_null(&sb);

        if (0 != compile_exe(&exe_source, 1, deps, NOB_ARRAY_LEN(deps), sb.items, 1))
        {
            ret = 1;
            goto defer;
        }
    }

defer:
    if (!nob_procs_flush(&procs))
        ret = 1;
    nob_sb_free(sb);
    return ret;
}

static int have_valgrind(void)
{
#ifdef _WIN32
    return 0;
#else
    return nob_file_exists("/usr/bin/valgrind") > 0 || nob_file_exists("/usr/local/bin/valgrind") > 0;
#endif
}

static int path_has_cmd(const char *name)
{
    const char *path = getenv("PATH");
    if (path == NULL)
        return 0;

    char buf[512];
    const char *p = path;
    while (*p != '\0')
    {
        const char *colon = strchr(p, ':');
        size_t n = colon ? (size_t)(colon - p) : strlen(p);
        if (n > 0 && n < 400)
        {
            snprintf(buf, sizeof(buf), "%.*s/%s", (int)n, p, name);
            if (nob_file_exists(buf) > 0)
                return 1;
        }
        if (colon == NULL)
            break;
        p = colon + 1;
    }
    return 0;
}

static int have_clang(void)
{
#ifdef _WIN32
    return 0;
#else
    return nob_file_exists("/usr/bin/clang") > 0 || nob_file_exists("/usr/local/bin/clang") > 0 || path_has_cmd("clang");
#endif
}

#define AMALGAM_BANNER "/* GENERATED — do not edit. Source: src/ */\n"
#define AMALGAM_HEADER "src/teapot.h"
#define AMALGAM_OUT "stb_teapot.h"

static const char *amalgam_c_files[] = {
    "src/tp_platform.c",
    "src/tp_parse.c",
    "src/tp_http.c",
    "src/tp_conn.c",
    "src/tp_listen.c",
    "src/tp_wait_poll.c",
    "src/tp_wait_epoll.c",
    "src/tp_wait_kqueue.c",
    "src/tp_wait_wsapoll.c",
    "src/tp_wait_wfmo.c",
    "src/tp_run.c",
};

static int amalgamate_skip_include(const char *line, size_t n)
{
    size_t i = 0;
    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if (i + 8 > n || memcmp(line + i, "#include", 8) != 0)
        return 0;
    i += 8;
    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if (i >= n || line[i] != '"')
        return 0;
    i++;
    size_t start = i;
    while (i < n && line[i] != '"')
        i++;
    if (i >= n)
        return 0;
    const char *path = line + start;
    size_t path_n = i - start;
    size_t b = 0;
    for (size_t k = 0; k < path_n; k++)
    {
        if (path[k] == '/' || path[k] == '\\')
            b = k + 1;
    }
    const char *base = path + b;
    size_t bn = path_n - b;
    if (bn == 8 && memcmp(base, "teapot.h", 8) == 0)
        return 1;
    if (bn >= 5 && memcmp(base, "tp_", 3) == 0 && base[bn - 2] == '.' && base[bn - 1] == 'h')
        return 1;
    return 0;
}

static int amalgamate_append_c(Nob_String_Builder *out, const char *path)
{
    Nob_String_Builder file = {0};
    if (!nob_read_entire_file(path, &file))
    {
        nob_sb_free(file);
        return 1;
    }

    size_t i = 0;
    while (i < file.count)
    {
        size_t line_start = i;
        while (i < file.count && file.items[i] != '\n')
            i++;
        int have_nl = i < file.count;
        size_t raw_n = i - line_start;
        if (have_nl)
            i++;
        size_t cmp_n = raw_n;
        if (cmp_n > 0 && file.items[line_start + cmp_n - 1] == '\r')
            cmp_n--;
        if (amalgamate_skip_include(file.items + line_start, cmp_n))
            continue;
        nob_sb_append_buf(out, file.items + line_start, raw_n);
        if (have_nl)
            nob_sb_append_cstr(out, "\n");
    }

    if (out->count == 0 || out->items[out->count - 1] != '\n')
        nob_sb_append_cstr(out, "\n");

    nob_sb_free(file);
    return 0;
}

static int amalgamate(void)
{
    int ret = 0;
    Nob_String_Builder out = {0};

    nob_sb_append_cstr(&out, AMALGAM_BANNER);

    if (!nob_read_entire_file(AMALGAM_HEADER, &out))
    {
        ret = 1;
        goto defer;
    }
    if (out.count == 0 || out.items[out.count - 1] != '\n')
        nob_sb_append_cstr(&out, "\n");

    nob_sb_append_cstr(&out, "#ifdef STB_TEAPOT_IMPLEMENTATION\n");

    for (size_t i = 0; i < NOB_ARRAY_LEN(amalgam_c_files); i++)
    {
        if (0 != amalgamate_append_c(&out, amalgam_c_files[i]))
        {
            ret = 1;
            goto defer;
        }
    }

    nob_sb_append_cstr(&out, "#endif // STB_TEAPOT_IMPLEMENTATION\n");
    nob_sb_append_cstr(&out, "\n");
    nob_sb_append_cstr(&out, "#ifdef __cplusplus\n");
    nob_sb_append_cstr(&out, "}\n");
    nob_sb_append_cstr(&out, "#endif\n");
    nob_sb_append_cstr(&out, "#endif // STB_TEAPOT_H\n");

    if (!nob_write_entire_file(AMALGAM_OUT, out.items, out.count))
    {
        ret = 1;
        goto defer;
    }

    nob_log(NOB_INFO, "Generated %s", AMALGAM_OUT);

defer:
    nob_sb_free(out);
    return ret;
}

#include "nob_embed.c"


static int run_fuzz(int argc, char **argv)
{
#ifdef _WIN32
    (void)argc;
    (void)argv;
    nob_log(NOB_ERROR, "fuzzing is POSIX-only");
    return 1;
#else
    int ret = 0;
    Nob_Cmd cmd = {0};

    if (!have_clang())
    {
        nob_log(NOB_ERROR, "clang not found. On Ubuntu: sudo apt-get install clang");
        return 1;
    }

    if (!nob_mkdir_if_not_exists(BUILD_DIR))
        return 1;
    if (!nob_mkdir_if_not_exists(BUILD_DIR "fuzz_corpus"))
        return 1;

    nob_cmd_append(&cmd, "cp", "-a", TEST_DIR "fuzz_corpus/.", BUILD_DIR "fuzz_corpus/");
    if (!nob_cmd_run(&cmd))
    {
        ret = 1;
        goto defer;
    }
    nob_cmd_free(cmd);
    cmd = (Nob_Cmd){0};

    nob_cmd_append(&cmd, "clang", "-O1", "-g", "-std=c17", "-fsanitize=fuzzer,address,undefined", "-o",
                   BUILD_DIR "fuzz_serve", TEST_DIR "fuzz_serve.c");
    if (!nob_cmd_run(&cmd))
    {
        ret = 1;
        goto defer;
    }
    nob_cmd_free(cmd);
    cmd = (Nob_Cmd){0};

    nob_cmd_append(&cmd, BUILD_DIR "fuzz_serve", BUILD_DIR "fuzz_corpus", "-max_total_time=30", "-timeout=2",
                   "-artifact_prefix=" BUILD_DIR "fuzz-");
    for (int i = 0; i < argc; i++)
        nob_cmd_append(&cmd, argv[i]);
    if (!nob_cmd_run(&cmd))
        ret = 1;

defer:
    nob_cmd_free(cmd);
    return ret;
#endif
}

static int run_unit_tests(void)
{
    int ret = 0;
    int use_valgrind = have_valgrind();
    if (use_valgrind)
    {
        nob_log(NOB_INFO, "Running unit tests under valgrind --leak-check=full");
    }
    for (size_t i = 0; i < NOB_ARRAY_LEN(unit_tests); i++)
    {
        Nob_Cmd cmd = {0};
        char path[260];
#ifdef _WIN32
        snprintf(path, sizeof(path), "%s.exe", unit_tests[i]);
#else
        snprintf(path, sizeof(path), "%s", unit_tests[i]);
#endif
        if (use_valgrind)
        {
            nob_cmd_append(&cmd, "valgrind", "--leak-check=full", "--error-exitcode=99", "--quiet", path);
        }
        else
        {
            nob_cmd_append(&cmd, path);
        }
        if (!nob_cmd_run(&cmd))
        {
            ret = 1;
        }
        nob_cmd_free(cmd);
    }
    return ret;
}

int main(int argc, char **argv)
{
    int ret = 0;
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "third_party/nob/nob.h", "nob_embed.c");

    if (!nob_mkdir_if_not_exists(BUILD_DIR))
    {
        ret = 1;
        goto defer;
    }

    nob_shift_args(&argc, &argv); // Ignore program name

    if (argc > 0 && 0 == strcmp(argv[0], "amalgamate"))
    {
        ret = amalgamate();
        goto defer;
    }

    if (argc > 0 && 0 == strcmp(argv[0], "embed"))
    {
        ret = embed_generate(DOCS_EMBED_SRC, DOCS_EMBED_OUT);
        goto defer;
    }

    if (argc > 0 && 0 == strcmp(argv[0], "fuzz"))
    {
        nob_shift_args(&argc, &argv);
        if (0 != amalgamate())
        {
            ret = 1;
            goto defer;
        }
        ret = run_fuzz(argc, argv);
        goto defer;
    }

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

    if (0 != amalgamate())
    {
        ret = 1;
        goto defer;
    }

    if (0 != embed_if_stale(DOCS_EMBED_SRC, DOCS_EMBED_OUT))
    {
        ret = 1;
        goto defer;
    }
    if (0 != embed_if_stale(TEST_EMBED_SRC, TEST_EMBED_OUT))
    {
        ret = 1;
        goto defer;
    }
    if (0 != embed_selfcheck_negative())
    {
        ret = 1;
        goto defer;
    }

    if (0 != compile_all_exe(tests_and_examples, NOB_ARRAY_LEN(tests_and_examples)))
    {
        ret = 1;
        goto defer;
    }

    if (0 != compile_docs_examples())
    {
        ret = 1;
        goto defer;
    }

    if (0 != run_unit_tests())
    {
        ret = 1;
        goto defer;
    }

defer:
    return ret;
}
