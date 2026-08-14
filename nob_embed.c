/* Docs/fixture embed header generator — included from nob.c (single TU). */
#ifndef EMBED_MAX_FILE_BYTES
#define EMBED_MAX_FILE_BYTES (512u * 1024u)
#endif
#ifndef EMBED_MAX_FILES
#define EMBED_MAX_FILES 64u
#endif
#ifndef EMBED_MAX_TOTAL_BYTES
#define EMBED_MAX_TOTAL_BYTES (2u * 1024u * 1024u)
#endif

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
