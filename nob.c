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

#define EMBED_MAX_FILE_BYTES (512u * 1024u)
#define EMBED_MAX_FILES 64u
#define EMBED_MAX_TOTAL_BYTES (2u * 1024u * 1024u)

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

typedef struct {
    char *url;
    char *fs_path;
    const char *ctype;
    size_t len;
} EmbedFile;

typedef struct {
    EmbedFile *items;
    size_t count;
    size_t capacity;
} EmbedFiles;

typedef struct {
    EmbedFiles *files;
    Nob_File_Paths *inputs;
    const char *root;
    size_t root_len;
    size_t total_bytes;
    int failed;
} EmbedWalkCtx;

static char *embed_xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

static const char *embed_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
#endif
    return slash ? slash + 1 : path;
}

static int embed_ctype_for(const char *path, const char **ctype_out)
{
    const char *base = embed_basename(path);
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot == base)
        return 0;
    if (0 == strcmp(dot, ".html") || 0 == strcmp(dot, ".htm"))
    {
        *ctype_out = "text/html; charset=utf-8";
        return 1;
    }
    if (0 == strcmp(dot, ".js"))
    {
        *ctype_out = "application/javascript";
        return 1;
    }
    if (0 == strcmp(dot, ".css"))
    {
        *ctype_out = "text/css";
        return 1;
    }
    if (0 == strcmp(dot, ".woff2"))
    {
        *ctype_out = "font/woff2";
        return 1;
    }
    return 0;
}

static int embed_url_ok(const char *url)
{
    if (url == NULL || url[0] != '/')
        return 0;
    if (strchr(url, '\\') != NULL)
        return 0;

    const char *p = url;
    while (*p == '/')
        p++;
    if (*p == '\0')
        return 0; /* "/" alone is not a file URL */

    while (*p != '\0')
    {
        if (*p == '/')
            return 0; /* empty segment */
        if (*p == '.')
        {
            if (p[1] == '\0' || p[1] == '/')
                return 0; /* "." */
            if (p[1] == '.' && (p[2] == '\0' || p[2] == '/'))
                return 0; /* ".." */
        }
        while (*p != '\0' && *p != '/')
        {
            if (*p == '\\')
                return 0;
            p++;
        }
        if (*p == '/')
        {
            p++;
            if (*p == '\0')
                return 0; /* trailing slash */
        }
    }
    return 1;
}

static int embed_files_cmp(const void *a, const void *b)
{
    const EmbedFile *fa = a;
    const EmbedFile *fb = b;
    return strcmp(fa->url, fb->url);
}

static void embed_files_free(EmbedFiles *files)
{
    for (size_t i = 0; i < files->count; i++)
    {
        free(files->items[i].url);
        free(files->items[i].fs_path);
    }
    free(files->items);
    files->items = NULL;
    files->count = 0;
    files->capacity = 0;
}

static bool embed_walk(Nob_Walk_Entry entry)
{
    EmbedWalkCtx *ctx = entry.data;
    const char *path = entry.path;
    const char *base = embed_basename(path);

    if (entry.level == 0)
        return true;

    if (base[0] == '.')
    {
        if (entry.type == NOB_FILE_DIRECTORY)
            *entry.action = NOB_WALK_SKIP;
        return true;
    }

    if (entry.type == NOB_FILE_SYMLINK)
    {
        nob_log(NOB_ERROR, "embed: symlink rejected: %s", path);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        return false;
    }

    if (entry.type == NOB_FILE_DIRECTORY)
        return true;

    if (entry.type != NOB_FILE_REGULAR)
    {
        nob_log(NOB_ERROR, "embed: unsupported file type: %s", path);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        return false;
    }

    if (strlen(path) < ctx->root_len || 0 != strncmp(path, ctx->root, ctx->root_len))
    {
        nob_log(NOB_ERROR, "embed: path outside root: %s", path);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        return false;
    }

    const char *rel = path + ctx->root_len;
    if (*rel == '/' || *rel == '\\')
        rel++;
    if (*rel == '\0')
        return true;

    Nob_String_Builder url_sb = {0};
    nob_sb_append_cstr(&url_sb, "/");
    for (const char *q = rel; *q != '\0'; q++)
    {
        if (*q == '\\')
        {
            nob_log(NOB_ERROR, "embed: backslash in path: %s", path);
            ctx->failed = 1;
            *entry.action = NOB_WALK_STOP;
            nob_sb_free(url_sb);
            return false;
        }
        nob_sb_append(&url_sb, *q);
    }
    nob_sb_append_null(&url_sb);

    if (!embed_url_ok(url_sb.items))
    {
        nob_log(NOB_ERROR, "embed: invalid URL path for %s -> %s", path, url_sb.items);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        return false;
    }

    const char *ctype = NULL;
    if (!embed_ctype_for(url_sb.items, &ctype))
    {
        nob_log(NOB_ERROR, "embed: unknown extension: %s", path);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        return false;
    }

    Nob_String_Builder file = {0};
    if (!nob_read_entire_file(path, &file))
    {
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        nob_sb_free(file);
        return false;
    }

    if (file.count > EMBED_MAX_FILE_BYTES)
    {
        nob_log(NOB_ERROR, "embed: file exceeds 512KiB: %s (%zu bytes)", path, file.count);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        nob_sb_free(file);
        return false;
    }

    if (ctx->files->count >= EMBED_MAX_FILES)
    {
        nob_log(NOB_ERROR, "embed: more than %u files", EMBED_MAX_FILES);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        nob_sb_free(file);
        return false;
    }

    if (ctx->total_bytes + file.count > EMBED_MAX_TOTAL_BYTES)
    {
        nob_log(NOB_ERROR, "embed: total payload exceeds 2MiB");
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        nob_sb_free(file);
        return false;
    }

    EmbedFile ef = {0};
    ef.url = embed_xstrdup(url_sb.items);
    ef.fs_path = embed_xstrdup(path);
    ef.ctype = ctype;
    ef.len = file.count;
    if (ef.url == NULL || ef.fs_path == NULL)
    {
        nob_log(NOB_ERROR, "embed: out of memory");
        free(ef.url);
        free(ef.fs_path);
        ctx->failed = 1;
        *entry.action = NOB_WALK_STOP;
        nob_sb_free(url_sb);
        nob_sb_free(file);
        return false;
    }

    nob_da_append(ctx->files, ef);
    ctx->total_bytes += file.count;
    if (ctx->inputs)
        nob_da_append(ctx->inputs, nob_temp_strdup(path));

    nob_sb_free(url_sb);
    nob_sb_free(file);
    return true;
}

static int embed_collect(const char *root, EmbedFiles *files, Nob_File_Paths *inputs)
{
    if (nob_file_exists(root) <= 0)
    {
        nob_log(NOB_ERROR, "embed: missing source dir %s", root);
        return 1;
    }

    Nob_File_Type root_type = nob_get_file_type(root);
    if (root_type == NOB_FILE_SYMLINK)
    {
        nob_log(NOB_ERROR, "embed: source dir is a symlink: %s", root);
        return 1;
    }
    if (root_type != NOB_FILE_DIRECTORY)
    {
        nob_log(NOB_ERROR, "embed: not a directory: %s", root);
        return 1;
    }

    EmbedWalkCtx ctx = {
        .files = files,
        .inputs = inputs,
        .root = root,
        .root_len = strlen(root),
        .total_bytes = 0,
        .failed = 0,
    };

    if (!nob_walk_dir(root, embed_walk, .data = &ctx))
        return 1;
    if (ctx.failed)
        return 1;

    if (files->count > 1)
        qsort(files->items, files->count, sizeof(files->items[0]), embed_files_cmp);

    return 0;
}

static void embed_append_c_string(Nob_String_Builder *out, const char *s)
{
    nob_sb_append(out, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++)
    {
        if (*p == '\\' || *p == '"')
        {
            nob_sb_append(out, '\\');
            nob_sb_append(out, (char)*p);
        }
        else if (*p == '\n')
            nob_sb_append_cstr(out, "\\n");
        else if (*p == '\r')
            nob_sb_append_cstr(out, "\\r");
        else if (*p < 0x20 || *p == 0x7f)
            nob_sb_appendf(out, "\\x%02x", (unsigned)*p);
        else
            nob_sb_append(out, (char)*p);
    }
    nob_sb_append(out, '"');
}

static int embed_write_header(const char *out_path, const EmbedFiles *files)
{
    int ret = 0;
    Nob_String_Builder out = {0};
    Nob_String_Builder file = {0};

    nob_sb_append_cstr(&out, "/* GENERATED — do not edit. */\n");
    nob_sb_append_cstr(&out, "#ifndef TP_EMBED_GENERATED_H_\n");
    nob_sb_append_cstr(&out, "#define TP_EMBED_GENERATED_H_\n");
    nob_sb_append_cstr(&out, "#include <stddef.h>\n");
    nob_sb_append_cstr(&out, "#include <string.h>\n");
    nob_sb_append_cstr(&out, "\n");
    nob_sb_append_cstr(&out, "typedef struct {\n");
    nob_sb_append_cstr(&out, "    const char *path;\n");
    nob_sb_append_cstr(&out, "    const char *ctype;\n");
    nob_sb_append_cstr(&out, "    const char *data;\n");
    nob_sb_append_cstr(&out, "    size_t len;\n");
    nob_sb_append_cstr(&out, "} tp_embed_file;\n");
    nob_sb_append_cstr(&out, "\n");

    for (size_t i = 0; i < files->count; i++)
    {
        file.count = 0;
        if (!nob_read_entire_file(files->items[i].fs_path, &file))
        {
            ret = 1;
            goto defer;
        }

        if (file.count == 0)
        {
            nob_sb_appendf(&out, "static const unsigned char blob_%zu[1] = {0};\n\n", i);
        }
        else
        {
            nob_sb_appendf(&out, "static const unsigned char blob_%zu[] = {", i);
            for (size_t j = 0; j < file.count; j++)
            {
                if (j % 12 == 0)
                    nob_sb_append_cstr(&out, "\n    ");
                nob_sb_appendf(&out, "0x%02x", (unsigned)(unsigned char)file.items[j]);
                if (j + 1 < file.count)
                    nob_sb_append_cstr(&out, ", ");
            }
            nob_sb_append_cstr(&out, "\n};\n\n");
        }
    }

    nob_sb_appendf(&out, "#define TP_EMBED_FILE_COUNT %zu\n", files->count);
    if (files->count == 0)
    {
        nob_sb_append_cstr(&out, "static const tp_embed_file tp_embed_files[1];\n");
    }
    else
    {
        nob_sb_append_cstr(&out, "static const tp_embed_file tp_embed_files[TP_EMBED_FILE_COUNT] = {\n");
        for (size_t i = 0; i < files->count; i++)
        {
            nob_sb_append_cstr(&out, "    {");
            embed_append_c_string(&out, files->items[i].url);
            nob_sb_append_cstr(&out, ", ");
            embed_append_c_string(&out, files->items[i].ctype);
            if (files->items[i].len == 0)
                nob_sb_appendf(&out, ", (const char *)blob_%zu, 0},\n", i);
            else
                nob_sb_appendf(&out, ", (const char *)blob_%zu, sizeof(blob_%zu)},\n", i, i);
        }
        nob_sb_append_cstr(&out, "};\n");
    }
    nob_sb_append_cstr(&out, "\n");
    nob_sb_append_cstr(&out, "static const tp_embed_file *tp_embed_find(const char *path)\n");
    nob_sb_append_cstr(&out, "{\n");
    nob_sb_append_cstr(&out, "    size_t n;\n");
    nob_sb_append_cstr(&out, "    size_t i;\n");
    nob_sb_append_cstr(&out, "    if (path == NULL)\n");
    nob_sb_append_cstr(&out, "        return NULL;\n");
    nob_sb_append_cstr(&out, "    n = 0;\n");
    nob_sb_append_cstr(&out, "    while (path[n] != '\\0' && path[n] != '?')\n");
    nob_sb_append_cstr(&out, "        n++;\n");
    nob_sb_append_cstr(&out, "    for (i = 0; i < TP_EMBED_FILE_COUNT; i++)\n");
    nob_sb_append_cstr(&out, "    {\n");
    nob_sb_append_cstr(&out, "        if (strncmp(tp_embed_files[i].path, path, n) == 0 && tp_embed_files[i].path[n] == '\\0')\n");
    nob_sb_append_cstr(&out, "            return &tp_embed_files[i];\n");
    nob_sb_append_cstr(&out, "    }\n");
    nob_sb_append_cstr(&out, "    return NULL;\n");
    nob_sb_append_cstr(&out, "}\n");
    nob_sb_append_cstr(&out, "\n");
    nob_sb_append_cstr(&out, "#endif /* TP_EMBED_GENERATED_H_ */\n");

    if (!nob_write_entire_file(out_path, out.items, out.count))
    {
        ret = 1;
        goto defer;
    }
    nob_log(NOB_INFO, "Generated %s (%zu files)", out_path, files->count);

defer:
    nob_sb_free(out);
    nob_sb_free(file);
    return ret;
}

static int embed_generate(const char *src_dir, const char *out_path)
{
    int ret = 0;
    EmbedFiles files = {0};

    if (0 != embed_collect(src_dir, &files, NULL))
    {
        ret = 1;
        goto defer;
    }
    if (0 != embed_write_header(out_path, &files))
    {
        ret = 1;
        goto defer;
    }

defer:
    embed_files_free(&files);
    return ret;
}

static int embed_if_stale(const char *src_dir, const char *out_path)
{
    /* Always regenerate. needs_rebuild on surviving file mtimes misses
     * deletions (and empty trees leave a stale header if the out exists).
     * Docs/fixture trees are small, so rewrite cost is negligible. */
    return embed_generate(src_dir, out_path);
}

static int embed_selfcheck_negative(void)
{
    const char *bad_dir = BUILD_DIR "embed_neg";
    const char *bad_out = BUILD_DIR "embed_neg.h";
    const char *bad_file = BUILD_DIR "embed_neg/bad.txt";

    if (!nob_mkdir_if_not_exists(bad_dir))
        return 1;
    if (!nob_write_entire_file(bad_file, "x", 1))
        return 1;

    if (0 == embed_generate(bad_dir, bad_out))
    {
        nob_log(NOB_ERROR, "embed self-check: expected unknown ext to fail");
        return 1;
    }
    nob_log(NOB_INFO, "embed self-check: unknown extension rejected");
    nob_delete_file(bad_file);
    return 0;
}

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
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "third_party/nob/nob.h");

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
