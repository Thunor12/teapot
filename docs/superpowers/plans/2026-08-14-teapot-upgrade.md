# Teapot Upgrade Implementation Plan

> **Status: superseded (2026-08-14).** Do not execute this plan.
> Successor: [`2026-08-14-teapot-http-runtime.md`](./2026-08-14-teapot-http-runtime.md)
> Spec: [`../specs/2026-08-14-teapot-listener-builder-design.md`](../specs/2026-08-14-teapot-listener-builder-design.md)
>
> Kept for history: Cursor branch ranking / thermo findings that informed the runtime rewrite.
> Obsolete constraints here (do **not** revive): single-file-only library, amalgam &lt;1000 lines, “do not split into `.c`”.

---

# Teapot Upgrade Implementation Plan (historical)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn `stb_teapot.h` into one linear HTTP parse→complete-body→route→send pipeline, under 1000 lines, with tests that actually run — without merging any of the 59 open Cursor branches as-is.

**Architecture:** Stay a single-header stb-style library. Delete the dual `sscanf`/`strstr` parser, the unused strtok helper, and the `#if 0` / TODO wall. One header walk produces `Content-Length`. `teapot_serve_client` never closes the socket; `teapot_handle_client_connection` takes ownership and closes. Send is `tp_sb_appendf` + `write_all`, not a 256-byte stack buffer. Cherry-pick the real bugfixes from `origin/cursor/critical-bug-investigation-d5e3`; throw the branch away.

**Tech Stack:** C17, `stb_teapot.h` (library), `nob.c`/`nob.h` (build), POSIX `socketpair` tests, GitHub Actions (`cc nob.c -o nob && ./nob`).

## Global Constraints

- Stay a single-header library. Do not split into a `.c`. Do not absorb the thread pool into the header.
- `stb_teapot.h` must finish **under 1000 lines**. Target 650–850. Main is already 1075; `d5e3` grows it to 1232 and is refused.
- Do **not** merge `origin/cursor/critical-bug-investigation-d5e3` or any of the other 58 Cursor branches. Re-implement the useful fixes on a fresh branch from `main`.
- HTTP/1.1 subset: no header folding, no pipelining. Oversize headers and oversize `Content-Length` are 400, not silent truncate.
- `teapot_*` is the public prefix. New public symbols must use it. Do not churn `tp_*` types in the same change as the parser rewrite.
- Tests run from `./nob` and from CI. A test that is a blocking server is not a unit test.
- Existing example designated initializers `{TEAPOT_GET, "/hello", handler}` must keep compiling.
- Every task ends with `./nob` and the unit-test binaries exiting 0.

---

## Branch ranking (59 remotes off `main`)

None of these have open GitHub PRs (`gh` is not configured on this machine; branches exist only on `origin`). **All are 0 behind `main`.** Almost all are repeated Cursor Cloud “critical bug investigation” runs of the same campaign.

### Do not merge any of them

The thermo-nuclear review of [Thermo-nuclear Code Quality Review](15b5cfc7-f573-4a4c-b74e-82582d7ca90e) is: **do not approve `d5e3`**. It has real fixes stuffed into a larger, still-messy header. This plan cherry-picks the fixes and deletes the rest of the complexity.

### Tier A — one canonical patch, 18 identical copies (highest *content* value, still refused as a merge)

Net diff vs `main` is **bit-identical** across these 18 branches (sha256 `69f108dfcd`). Newest tip: **`origin/cursor/critical-bug-investigation-d5e3`** (2026-08-13).

| Branch | Last subject (often a later cosmetic commit) |
|---|---|
| `cursor/critical-bug-investigation-d5e3` | Fix critical HTTP framing and response safety bugs **← use as the read-only reference** |
| `cursor/critical-bug-investigation-9e33` | same patch |
| `cursor/critical-bug-investigation-f697` | same patch |
| `cursor/critical-bug-investigation-0a1d` | same patch |
| `cursor/critical-bug-investigation-4295` | same patch |
| `cursor/critical-bug-investigation-8fb0` | same patch |
| `cursor/critical-bug-investigation-d09a` | same patch |
| `cursor/critical-bug-investigation-e4d6` | same patch |
| `cursor/critical-bug-investigation-3f90` | same patch |
| `cursor/critical-bug-investigation-4645` | same patch |
| `cursor/critical-bug-investigation-f8ad` | same patch |
| `cursor/critical-bug-investigation-e691` | same patch |
| `cursor/critical-bug-investigation-c4be` | same patch |
| `cursor/critical-bug-investigation-34d4` | same patch |
| `cursor/critical-bug-investigation-851a` | same patch |
| `cursor/critical-bug-investigation-908f` | same patch |
| `cursor/critical-bug-investigation-595e` | last commit is “double-close / enum”; **tree matches the 18** |
| `cursor/critical-bug-investigation-5961` | last commit is “empty header span”; **tree matches the 18** |

**What is actually good in this patch (cherry-pick into this plan, do not merge the branch):**

- `strcmp` for methods (stops `GETTING` matching `GET`)
- advance the header-name pointer when trimming; reject empty names
- `teapot_write_all` / full `Content-Type` in the response line
- skip `memcpy` of 0 bytes in `tp_da_append_many`
- `compile_exe` returns `ret`; `nob_procs_flush` failure is detected
- drop the extra `teapot_close` in `threaded_server.c`
- request/response socketpair tests (`tests/unit_test_request.c`, `tests/unit_test_response.c`)

**What is refused:** 1075 → 1232 lines; nested-ternary `*` route matcher; a 70-line Content-Length parser while body completion still re-reads the header; `snprintf(NULL,0)` send path instead of `tp_sb_appendf`; leaving `tp_chop_by_delim_into_array`, `#if 0`, and the 10-TODO wall in place.

### Tier B — older unique trees of the same campaign (do not merge)

Each of these still changes `stb_teapot.h` and is an incomplete ancestor of Tier A. Last commit messages are mostly “header name trimming”, “zero-length append”, or “normalize newlines”. **Superseded.** Delete after this upgrade lands.

| Importance inside the pile | Branches | Why they existed |
|---|---|---|
| Header-trim pointer bug | `3564`, `3b66`, `8cca`, `a24b`, `f6b4`, `47a9`, `bf16`, `aa5a`, `8710`, `da04` | `tp_trim_ws` returns a length but callers keep the original pointer. Task 3 of this plan. |
| Zero-length `memcpy` | `8d85`, `d0ac` | Empty body `tp_da_append_many`. One-line guard in Task 2. |
| Partial HTTP parse | `67ef`, `7bba`, `d348`, `927c`, `62d9` | Header over-read, structured Content-Length, enum warnings. Subsets of Tier A. |
| Newline / warning noise on top of older HTTP diffs | `0bc7`, `11f2`, `1e91`, `3c8c`, `5638`, `5ae3`, `61eb`, `72e8`, `73fc`, `762c`, `7b55`, `96f4`, `9d11`, `a25e`, `a434`, `ab81`, `cc2b`, `da01`, `e190`, `1373`, `5730`, `b0de` | Last commit is EOF newline; the tree still carries older teapot edits. Discard. |
| Test-output / build | `b14d` | “Avoid duplicate forked test output”; nob.c overlap with Tier A. |

Prefix all names above with `origin/cursor/critical-bug-investigation-` or `origin/cursor/critical-correctness-bugs-` as listed in `git branch -r`.

### Tier C — keep the *idea*, not the branch

| Branch | Why it is low product-code value but useful notes |
|---|---|
| `origin/cursor/dev-environment-setup-4d6c` | Adds `AGENTS.md` only (teapot.h unchanged). Useful facts: clang `-Wsign-conversion` breaks `-Werror`; `./nob` does not run tests; `unit_test_headers` has 3 failing assertions; `low_level_test_stb_teapot` is a blocking server. Fold a short version into `README.md` in Task 10; do not merge the branch as a Cloud-only doc. |

### Recommended remote cleanup (after this plan is on `main`)

Keep: `origin/main` and the upgrade branch.

Delete: all 59 `origin/cursor/*` branches listed above. They are agent debris. 18 of them are literal duplicates.

---

## Thermo-nuclear findings (current `main`)

Verified by building and running, 2026-08-14:

```
./build/unit_test_headers → 3 TEST(S) FAILED
  FAIL trim name == X-Hello
  FAIL empty name ignored -> 1 header
  FAIL Good==v
./nob log: ./build/thread_pool_server_ up to date   ← strtok(".c") treats '.' and 'c' as delimiters
```

Priority order:

1. **Three parsers for one HTTP message.** `parse_request` does `sscanf` + `strstr` for `Content-Type`/`Content-Length`/`\r\n\r\n`, then `tp_extract_header_keyval` walks the same bytes. `teapot_handle_client_connection` then `atol`s `Content-Length` again. The `Content-Type` sscanf buffer is never read.
2. **`stb_teapot.h` is already 1075 lines.** Dead code (`tp_chop_by_delim_into_array` + `strtok`, `#if 0` trim, unused DA macros, 10-TODO wall) is how it got there. Delete first.
3. **Socket ownership is inverted.** `teapot_handle_client_connection` always closes, including when `server` is NULL. `threaded_server.c` closes again (fd reuse on POSIX). `teapot_close` is `static` inside the implementation block; examples only compile because they `#define STB_TEAPOT_IMPLEMENTATION`.
4. **`teapot_send_response` is a 256-byte stack bomb.** `snprintf` into 256 bytes, then `teapot_write(..., header_len)` with the *untruncated* length. Partial writes ignored.
5. **Header trim is wrong.** `tp_trim_ws` returns `end - start`; callers keep the original pointer. `"  X-Hello  "` becomes `"  X-Hel"`. Empty names are still appended.
6. **`nob.c` lies.** `compile_exe` sets `ret` then `return 0`. Tests are compiled and never run. CI is compile-only.
7. **`*` glob in `teapot_find_handler`** is an unnamed dialect of matching. Do not add d5e3’s nested ternary on “does the prefix already end in `/`”.
8. **hello/echo handlers are copy-pasted three times**, which is how `tp_sb_append_null` (NUL counted in `Content-Length`) survived in examples while `handle_client_connection` comments “do not append null”.

Target pipeline:

```
recv → split request-line / headers / body-prefix
     → parse_request_line
     → parse_headers (one walker)
     → content_length = headers_get("Content-Length") or 0
     → complete_body
     → find_route
     → resp = handler(req)
     → sb_appendf status + headers; write_all(sb); write_all(body)
```

---

## File structure

| File | Responsibility after the upgrade |
|---|---|
| `stb_teapot.h` | Public types + API; `static` implementation of parse/headers/send/listen. Under 1000 lines. |
| `nob.c` | Compile **and run** unit tests. Honest exit codes. Correct output names. |
| `tests/unit_test_headers.c` | Header walker only. Must pass. |
| `tests/unit_test_request.c` | **Create.** socketpair tests for parse/body/method/400. |
| `tests/unit_test_response.c` | **Create.** socketpair tests for long Content-Type and CRLF rejection. |
| `tests/header_parse.c` | Keep as a tiny printf smoke, or delete if redundant after unit tests. Prefer delete in Task 8 if `unit_test_headers` covers it. |
| `tests/low_level_test_stb_teapot.c` | Rename conceptually to an example: it is a blocking server. Stop listing it as a test. Move to `examples/basic_server.c` in Task 8. |
| `examples/demo_handlers.h` | **Create.** hello/echo once. |
| `examples/threaded_server.c` | Include demo handlers. Do not double-close. |
| `examples/thread_pool_server_crossplat.c` | Include demo handlers. Do not double-close the listen socket. Stay an example. |
| `.github/workflows/nob.yml` | Build **and run** unit tests. |
| `README.md` | Real usage, test commands, HTTP subset, socket-ownership contract. |

**Public API after Task 6** (later tasks consume these names):

```c
int teapot_socket_ok(stb_teapot_socket_t s);
void teapot_close(stb_teapot_socket_t s);

int teapot_listener_open(teapot_server *server, stb_teapot_socket_t *out_listen_sock);
stb_teapot_socket_t teapot_listener_accept(stb_teapot_socket_t listen_sock);
void teapot_listener_close(stb_teapot_socket_t listen_sock);

/* parse, complete body, route, send. Does NOT close client. */
int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client);

/* teapot_serve_client + teapot_close. Takes ownership of client. */
int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client);

int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp);

typedef struct {
    teapot_method method;
    const char *path;
    teapot_handler handler;
    int prefix; /* 0 = exact match (zero-init keeps current call sites working) */
} teapot_route;
```

**Deleted from the header:** `tp_chop_by_delim_into_array`, `tp_string_array`, `tp_sa_*`, `tp_da_last`, `tp_da_remove_unordered`, `#if 0` `tp_trim_trailing_ws`, unused `Content-Type`/`Content-Length` `strstr`/`sscanf`, `socket_ok` (replaced by `teapot_socket_ok`).

---

### Task 1: Honest `nob` + actually run tests

**Files:**
- Modify: `nob.c:32-84` (`compile_exe` return), `nob.c:86-92` (test list), `nob.c:94-133` (`compile_all_exe` flush + output name), `nob.c:135-169` (`main` runs tests)
- Test: existing `tests/unit_test_headers.c` (expected to still fail until Task 3 — Task 1 must **propagate** that failure)

**Interfaces:**
- Consumes: current `compile_exe` / `compile_all_exe`
- Produces: `./nob` exits non-zero if compile **or** a unit test fails; binaries named from the source stem with only the trailing `.c` stripped

- [x] **Step 1: Write a failing check for the strtok output name**

Create `tests/nob_output_name_check.sh` is unnecessary. Reproduce first:

```bash
cc nob.c -o nob && ./nob
ls -l build/thread_pool_server_crossplat build/thread_pool_server_ 2>&1
```

Expected: `build/thread_pool_server_` exists (bug), `build/thread_pool_server_crossplat` does not.

- [x] **Step 2: Fix output-name stripping and `compile_exe` return**

In `nob.c`, replace `strtok(temp, ".c")` with a suffix strip:

```c
static void strip_c_suffix(char *name)
{
    size_t n = strlen(name);
    if (n >= 2 && name[n - 2] == '.' && name[n - 1] == 'c')
    {
        name[n - 2] = '\0';
    }
}
```

Call `strip_c_suffix(temp)` instead of `strtok`.

In `compile_exe`, change the final `return 0;` to `return ret;`.

In `compile_all_exe`, after the loop:

```c
if (!nob_procs_flush(&procs))
{
    ret = 1;
}
```

(Remove the unconditional `nob_procs_flush` that ignored the result.)

- [x] **Step 3: Run unit-test binaries from `main` after a successful compile**

Keep compiling examples. After `compile_all_exe` succeeds, run only the real unit tests:

```c
static const char *unit_tests[] = {
    BUILD_DIR "unit_test_headers",
};

static int run_unit_tests(void)
{
    int ret = 0;
    for (size_t i = 0; i < NOB_ARRAY_LEN(unit_tests); i++)
    {
        Nob_Cmd cmd = {0};
        char path[260];
#ifdef _WIN32
        snprintf(path, sizeof(path), "%s.exe", unit_tests[i]);
#else
        snprintf(path, sizeof(path), "%s", unit_tests[i]);
#endif
        nob_cmd_append(&cmd, path);
        if (!nob_cmd_run(&cmd))
        {
            ret = 1;
        }
        nob_cmd_free(cmd);
    }
    return ret;
}
```

After Task 7 adds `unit_test_request` and `unit_test_response`, append those names to `unit_tests[]` and to `tests_and_examples[]`.

Do **not** run `low_level_test_stb_teapot` — it is a blocking server.

- [x] **Step 4: Run `./nob` and confirm it fails because header tests fail**

Run: `cc nob.c -o nob && ./nob ; echo exit=$?`

Expected: `exit=1`, log contains `[FAIL] trim name == X-Hello`, and `ls build/thread_pool_server_crossplat` succeeds.

- [x] **Step 5: Commit**

```bash
git add nob.c
git commit -m "$(cat <<'EOF'
fix: make nob report compile failures and run unit tests

EOF
)"
```

---

### Task 2: Delete dead code so the header can shrink

**Files:**
- Modify: `stb_teapot.h` (delete unused helpers/macros; add the zero-length `memcpy` guard)
- Test: `tests/unit_test_headers.c` (behavior unchanged; still 3 failures)

**Interfaces:**
- Consumes: current DA macros used by headers/bodies (`tp_da_reserve`, `tp_da_append`, `tp_da_append_many`, `tp_da_free`, `tp_sb_*`)
- Produces: same public types; **gone:** `tp_string_array`, `tp_sa_append_str`, `tp_sa_free`, `tp_chop_by_delim_into_array`, `tp_da_last`, `tp_da_remove_unordered`, the `#if 0` block at `stb_teapot.h:485-497`, the 10-TODO wall at `stb_teapot.h:520-529`

- [x] **Step 1: Confirm unused symbols with a search**

```bash
rg -n 'tp_chop_by_delim_into_array|tp_string_array|tp_sa_append_str|tp_sa_free|tp_da_last|tp_da_remove_unordered' --glob '!docs/**'
```

Expected: definitions only, no call sites outside `stb_teapot.h`. If a call site exists, stop and do not delete that symbol.

- [x] **Step 2: Guard zero-length append (the entire 8d85/d0ac branches)**

Replace `tp_da_append_many` with:

```c
#define tp_da_append_many(da, new_items, new_items_count)                                           \
    do                                                                                              \
    {                                                                                               \
        size_t tp_da_new_items_count__ = (new_items_count);                                         \
        if (tp_da_new_items_count__ > 0)                                                            \
        {                                                                                           \
            tp_da_reserve((da), (da)->count + tp_da_new_items_count__);                             \
            memcpy((da)->items + (da)->count, (new_items),                                          \
                   tp_da_new_items_count__ * sizeof(*(da)->items));                                 \
            (da)->count += tp_da_new_items_count__;                                                 \
        }                                                                                           \
    } while (0)
```

- [x] **Step 3: Delete the unused symbols and the TODO wall**

Replace the 10 TODOs on `tp_parse_and_append_header_line` with one line:

```c
/* HTTP/1.1 subset: no obs-fold. Oversize names/values are rejected by the caller as 400. */
```

Delete `#if 0` … `#endif` around `tp_trim_trailing_ws`.

Move `#ifndef TP_MAX_HEADER_NAME_LEN` / `TP_MAX_HEADER_VALUE_LEN` to the top of the file with the other knobs (next to `TP_DA_INIT_CAP`). Do not `#define` them inside a function.

Remove duplicate `#include <stdio.h>` / `<stdlib.h>` / `<string.h>` / `<stdarg.h>` in the implementation block (they are already included above). Keep `<ctype.h>` and `<stdarg.h>` once.

- [x] **Step 4: Rebuild**

Run: `cc nob.c -o nob && ./nob`

Expected: still fails on the 3 header assertions (Task 3). Compile of examples succeeds. `wc -l stb_teapot.h` is lower than 1075.

- [x] **Step 5: Commit**

```bash
git add stb_teapot.h
git commit -m "$(cat <<'EOF'
refactor: delete unused teapot helpers and the header TODO wall

EOF
)"
```

---

### Task 3: Header trim and empty names (make the existing tests pass)

**Files:**
- Modify: `stb_teapot.h` (`tp_trim_leading_ws` / `tp_trim_ws` / `tp_parse_and_append_header_line`)
- Test: `tests/unit_test_headers.c:64-127` (already written; currently failing)

**Interfaces:**
- Consumes: `tp_extract_header_keyval(tp_headers *, const char *, size_t)` — change the raw pointer to `const char *` here if not already
- Produces: names and values trimmed on both sides; lines with empty names return 0 and are not appended; oversize name or value returns 0 (no silent clamp)

- [x] **Step 1: Run the existing tests to confirm they still fail**

Run: `./build/unit_test_headers`

Expected:

```
[FAIL] trim name == X-Hello
[FAIL] empty name ignored -> 1 header
[FAIL] Good==v
```

- [x] **Step 2: Replace length-only trim with a span trim**

```c
typedef struct
{
    const char *p;
    size_t n;
} tp_span;

static tp_span tp_span_trim(const char *s, size_t len)
{
    while (len > 0 && isspace((unsigned char)*s))
    {
        ++s;
        --len;
    }
    while (len > 0 && isspace((unsigned char)s[len - 1]))
    {
        --len;
    }
    return (tp_span){s, len};
}
```

Delete `tp_trim_leading_ws` and `tp_trim_ws` if nothing else uses them after this change.

- [x] **Step 3: Parse a header line with the span; reject empty and oversize names**

```c
static int tp_parse_and_append_header_line(tp_headers *headers_parsed, const char *line, size_t linelen)
{
    if (!headers_parsed || !line || linelen == 0)
    {
        return 0;
    }

    const char *colon = (const char *)memchr(line, ':', linelen);
    if (!colon)
    {
        return 0;
    }

    tp_span name = tp_span_trim(line, (size_t)(colon - line));
    tp_span value = tp_span_trim(colon + 1, (size_t)((line + linelen) - (colon + 1)));

    if (name.n == 0)
    {
        return 0;
    }
    if (name.n > (size_t)TP_MAX_HEADER_NAME_LEN || value.n > (size_t)TP_MAX_HEADER_VALUE_LEN)
    {
        return 0;
    }

    tp_header_line header_line = {0};
    tp_sb_append_buf(&header_line.name, name.p, name.n);
    tp_sb_append_null(&header_line.name);
    if (value.n > 0)
    {
        tp_sb_append_buf(&header_line.value, value.p, value.n);
        tp_sb_append_null(&header_line.value);
    }
    tp_da_append(headers_parsed, header_line);
    return 1;
}
```

Change `tp_extract_header_keyval` to take `const char *raw_header`.

Update `test_clamping` in `tests/unit_test_headers.c`: oversize input must produce **0 headers**, not a truncated one.

```c
ok("oversize rejected -> 0 headers", h.count == 0);
```

Remove the `name clamped` / `value clamped` assertions.

- [x] **Step 4: Run tests**

Run: `cc nob.c -o nob && ./nob`

Expected: `unit_test_headers` prints `ALL TESTS PASSED`; `./nob` exits 0.

- [x] **Step 5: Commit**

```bash
git add stb_teapot.h tests/unit_test_headers.c
git commit -m "$(cat <<'EOF'
fix: trim header names from the start of the span and reject empty names

EOF
)"
```

---

### Task 4: One header lookup, one Content-Length

**Files:**
- Modify: `stb_teapot.h` (`tp_headers_check`, `parse_request`, `teapot_handle_client_connection`)
- Test: `tests/unit_test_headers.c` (add `tp_headers_check` cases); later request tests in Task 7 exercise Content-Length

**Interfaces:**
- Consumes: `tp_headers_find`, `tp_headers_get`
- Produces: `tp_headers_check` does **one** find; `parse_request` no longer `strstr`s `Content-Type` or `Content-Length`; body length comes from `tp_headers_get(h, "Content-Length")` via `strtoul`

- [x] **Step 1: Add failing tests for `tp_headers_check`**

Append to `tests/unit_test_headers.c`:

```c
static void test_headers_check(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("Content-Type: text/plain\r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));

    tp_header_line out = {0};
    ok("check missing", tp_headers_check(&h, "X-No", NULL, &out) == TP_HEADER_NOT_FOUND);
    ok("check found", tp_headers_check(&h, "Content-Type", NULL, &out) == TP_HEADER_FOUND);
    ok("check match", tp_headers_check(&h, "Content-Type", "text/plain", &out) == TP_HEADER_MATCH);
    ok("check mismatch", tp_headers_check(&h, "Content-Type", "text/html", &out) == TP_HEADER_FOUND);

    tp_headers_free(&h);
    free(buf);
}
```

Call it from `main`. Run `./build/unit_test_headers` — these should already pass if `tp_headers_check` works; the point of the rewrite is to keep them passing with one find.

- [x] **Step 2: Collapse `tp_headers_check`**

```c
tp_header_result tp_headers_check(const tp_headers *h, const char *name, const char *expected_value, tp_header_line *o_header_line)
{
    const tp_header_line *hl = tp_headers_find(h, name);
    if (!hl)
    {
        return TP_HEADER_NOT_FOUND;
    }
    if (o_header_line)
    {
        *o_header_line = *hl;
    }
    if (expected_value == NULL)
    {
        return TP_HEADER_FOUND;
    }
    const char *val = hl->value.items ? hl->value.items : "";
    return (strcmp(val, expected_value) == 0) ? TP_HEADER_MATCH : TP_HEADER_FOUND;
}
```

Keep `tp_headers_match` as a one-line wrapper around `tp_headers_check(...) == TP_HEADER_MATCH` **or** delete it if `rg tp_headers_match` shows no callers outside the header. Prefer delete.

- [x] **Step 3: Parse Content-Length once from structured headers**

Add next to the other knobs:

```c
#ifndef TEAPOT_MAX_BODY_SIZE
#define TEAPOT_MAX_BODY_SIZE (4u * 1024u * 1024u)
#endif
```

```c
static int teapot_content_length_from_headers(const tp_headers *h, size_t *out_length)
{
    const tp_string_builder *val = tp_headers_get(h, "Content-Length");
    *out_length = 0;
    if (val == NULL || val->items == NULL || val->items[0] == '\0')
    {
        return 0; /* absent → 0 */
    }
    char *end = NULL;
    unsigned long n = strtoul(val->items, &end, 10);
    if (end == val->items)
    {
        return -1;
    }
    while (*end != '\0' && isspace((unsigned char)*end))
    {
        ++end;
    }
    if (*end != '\0' || n > (unsigned long)TEAPOT_MAX_BODY_SIZE)
    {
        return -1;
    }
    *out_length = (size_t)n;
    return 0;
}
```

Do **not** add a second wrapper `teapot_request_content_length` that only forwards to this.

- [x] **Step 4: Rewrite `parse_request` to one walk**

Remove the unused `content_type[128]` / `strstr(buffer, "Content-Type:")` / `strstr(buffer, "Content-Length:")` / `sscanf` of those.

```c
static int parse_request(char *buffer, size_t size, teapot_request *req)
{
    if (buffer == NULL || size == 0 || req == NULL)
    {
        return -1;
    }

    req->path = (tp_string_builder){0};
    req->body = (tp_string_builder){0};
    req->headers = (tp_headers){0};

    char method_buf[8] = {0};
    char path_buf[512] = {0};
    if (sscanf(buffer, "%7s %511s", method_buf, path_buf) != 2)
    {
        return -1;
    }

    teapot_method method = parse_method(method_buf);
    if (method == TEAPOT_UNKNOWN)
    {
        return -1;
    }

    const char *request_line_end = strstr(buffer, "\r\n");
    const char *header_end = strstr(buffer, "\r\n\r\n");
    if (request_line_end == NULL || header_end == NULL || request_line_end > header_end)
    {
        return -1;
    }

    const char *headers_start = request_line_end + 2;
    size_t header_size = header_end > headers_start ? (size_t)(header_end - headers_start) : 0;
    tp_extract_header_keyval(&req->headers, headers_start, header_size);

    size_t content_length = 0;
    if (teapot_content_length_from_headers(&req->headers, &content_length) != 0)
    {
        return -1;
    }

    const char *body_start = header_end + 4;
    size_t body_available = 0;
    if (size > (size_t)(body_start - buffer))
    {
        body_available = size - (size_t)(body_start - buffer);
    }
    size_t to_append = content_length < body_available ? content_length : body_available;

    req->method = method;
    tp_sb_append_buf(&req->path, path_buf, strlen(path_buf));
    tp_sb_append_null(&req->path);
    tp_sb_append_buf(&req->body, body_start, to_append);
    tp_sb_append_null(&req->body);
    req->body_length = to_append;
    return 0;
}
```

Change `parse_method` to `strcmp`:

```c
static teapot_method parse_method(const char *s)
{
    if (strcmp(s, "GET") == 0)
        return TEAPOT_GET;
    if (strcmp(s, "POST") == 0)
        return TEAPOT_POST;
    if (strcmp(s, "PUT") == 0)
        return TEAPOT_PUT;
    if (strcmp(s, "DELETE") == 0)
        return TEAPOT_DELETE;
    return TEAPOT_UNKNOWN;
}
```

In `teapot_handle_client_connection`, replace the nested `atol` / 4MB block with a call that uses the **already parsed** `req.body_length` vs a second `teapot_content_length_from_headers`. Prefer storing `content_length` on `teapot_request` so you do not parse the header twice:

```c
/* add to teapot_request */
size_t content_length; /* from Content-Length, 0 if absent */
```

Set it in `parse_request` (`req->content_length = content_length`). Body completion then loops until `req->body_length >= req->content_length` with no header re-parse.

If `parse_request` fails, send a 400 then close (after Task 6 splits serve/close; for now keep close as today):

```c
static void teapot_send_status_body(stb_teapot_socket_t client, int status, const char *msg)
{
    teapot_response resp;
    teapot_response_init(&resp, status);
    tp_sb_appendf(&resp.body, "%s", msg);
    (void)teapot_send_response(client, &resp);
    teapot_response_free(&resp);
}
```

Use this for 400 on parse/body failure and keep the existing 404 branch. One helper, not one function per status code.

- [x] **Step 5: Run tests**

Run: `cc nob.c -o nob && ./nob`

Expected: header unit tests pass. Examples still compile.

- [x] **Step 6: Commit**

```bash
git add stb_teapot.h tests/unit_test_headers.c
git commit -m "$(cat <<'EOF'
fix: parse HTTP headers once and take Content-Length from that walk

EOF
)"
```

---

### Task 5: Send path and write_all

**Files:**
- Modify: `stb_teapot.h` (`teapot_write`, `teapot_send_response`)
- Test: `tests/unit_test_response.c` (create in this task)

**Interfaces:**
- Consumes: `tp_sb_appendf`, `teapot_write`
- Produces: `teapot_write_all(stb_teapot_socket_t, const char *, size_t)` (static); `teapot_send_response` builds the header with `tp_sb_appendf`, rejects `\r`/`\n` in `content_type`, writes header then body with `write_all`

- [x] **Step 1: Create `tests/unit_test_response.c` (failing)**

POSIX `socketpair`. Windows stub `return 0` until someone ports it; do not block the upgrade on Win32 test I/O.

```c
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response is POSIX-only for now");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static int failures = 0;

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static size_t read_all(stb_teapot_socket_t fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n <= 0)
            break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_response(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("socketpair for long content type", 0);
        return;
    }

    char content_type[400];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send long content type", teapot_send_response(sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[1024];
    (void)read_all(sockets[1], received, sizeof(received));
    ok("response includes full long content type", strstr(received, content_type) != NULL);
    ok("response includes body length", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}

static void test_rejects_response_header_injection(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("socketpair for injection", 0);
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", 2);

    ok("reject CRLF in content type", teapot_send_response(sockets[0], &resp) == -1);

    teapot_response_free(&resp);
    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");
    test_long_content_type_response();
    test_rejects_response_header_injection();
    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
```

`teapot_close` is still `static` until Task 6. Because this file `#define STB_TEAPOT_IMPLEMENTATION`, `teapot_close` is visible. That is enough for this task.

Add the file to `tests_and_examples[]` and `unit_tests[]` in `nob.c`.

- [x] **Step 2: Run the new test to see it fail**

Run: `cc nob.c -o nob && ./nob`

Expected: FAIL `send long content type` and/or `response includes full long content type` (256-byte header). Injection may already “fail send” by accident of truncation — the assertion is `== -1` after an explicit CRLF check.

- [x] **Step 3: Implement `teapot_write_all` and rebuild send on `tp_sb_appendf`**

```c
#ifndef _WIN32
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

static int teapot_write(stb_teapot_socket_t s, const char *buf, int len)
{
    if (len <= 0)
        return 0;
#ifdef _WIN32
    return send(s, buf, len, 0);
#else
    return (int)send(s, buf, (size_t)len, MSG_NOSIGNAL);
#endif
}

static int teapot_write_all(stb_teapot_socket_t s, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        size_t remaining = len - total;
        int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        int n = teapot_write(s, buf + total, chunk);
        if (n <= 0)
            return -1;
        total += (size_t)n;
    }
    return 0;
}

int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp)
{
    if (!socket_ok(client) || !resp) /* Task 6 renames this to teapot_socket_ok */
        return -1;

    const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
    if (strpbrk(ct, "\r\n") != NULL)
        return -1;

    tp_string_builder header = {0};
    tp_sb_appendf(&header,
                  "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\n",
                  resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body));

    int rc = teapot_write_all(client, header.items, header.count);
    if (rc == 0 && resp->body.count > 0)
        rc = teapot_write_all(client, resp->body.items, resp->body.count);

    tp_sb_free(header);
    return rc;
}
```

Until Task 6 renames `socket_ok`, call the existing `socket_ok`.

Do **not** `snprintf(NULL, 0)` + `TP_REALLOC`. That duplicates `tp_sb_appendf`.

- [x] **Step 4: Run tests**

Run: `cc nob.c -o nob && ./nob`

Expected: `unit_test_headers` and `unit_test_response` both `ALL TESTS PASSED`.

- [x] **Step 5: Commit**

```bash
git add stb_teapot.h tests/unit_test_response.c nob.c
git commit -m "$(cat <<'EOF'
fix: send HTTP responses through a string builder and reject header injection

EOF
)"
```

---

### Task 6: Socket ownership

**Files:**
- Modify: `stb_teapot.h` (API block ~325–333, `socket_ok`, `teapot_close`, `teapot_handle_client_connection`, `teapot_listen`, `teapot_listener_open`)
- Modify: `examples/threaded_server.c:81-107` (remove extra close)
- Modify: `examples/thread_pool_server_crossplat.c:392-396` (listen socket closed twice on Unix)
- Test: `tests/unit_test_request.c` created in Task 7; this task keeps examples compiling and adds `SO_REUSEADDR`

**Interfaces:**
- Consumes: current `teapot_handle_client_connection` that always closes
- Produces: public `teapot_socket_ok`, `teapot_close`, `teapot_listener_close`, `teapot_serve_client` (no close); `teapot_handle_client_connection` = serve + close; `teapot_listener_open` sets `SO_REUSEADDR`

- [x] **Step 1: Make close and socket_ok public**

In the API block (outside `STB_TEAPOT_IMPLEMENTATION`):

```c
int teapot_socket_ok(stb_teapot_socket_t s);
void teapot_close(stb_teapot_socket_t s);
void teapot_listener_close(stb_teapot_socket_t listen_sock);
int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client);
```

Rename the implementation of `socket_ok` to `teapot_socket_ok` and drop `static`. Provide a compatibility macro only if something outside the repo would break — nothing in this repo should keep calling `socket_ok`.

```c
void teapot_close(stb_teapot_socket_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void teapot_listener_close(stb_teapot_socket_t listen_sock)
{
    teapot_close(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
}
```

`teapot_init` / `WSAStartup` stays inside `teapot_listener_open`. Document in a one-line comment: one listener per process is the supported model on Windows.

- [x] **Step 2: Split serve vs handle**

```c
int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client)
{
    if (!server || !teapot_socket_ok(client))
        return -1;

    char buffer[8192] = {0};
    int received = 0;
    if (teapot_recv_request(client, buffer, (int)sizeof(buffer), &received) < 0)
        return -1;

    teapot_request req = {0};
    if (parse_request(buffer, (size_t)received, &req) < 0)
    {
        free_request(&req);
        teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
        return -1;
    }

    if (teapot_complete_request_body(client, &req) != 0)
    {
        free_request(&req);
        teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
        return -1;
    }

    teapot_handler handler = teapot_find_handler(server, &req);
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    if (handler)
        resp = handler(&req);
    else
    {
        resp.status = TEAPOT_HTTP_NOT_FOUND;
        tp_sb_appendf(&resp.body, "404 Not Found\n");
    }

    int rc = teapot_send_response(client, &resp);
    teapot_response_free(&resp);
    free_request(&req);
    return rc;
}

int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
{
    int rc = teapot_serve_client(server, client);
    teapot_close(client);
    return rc;
}
```

`teapot_complete_request_body` is the existing while-loop, using `req->content_length` from Task 4. If `n <= 0` before the body is complete, return -1.

Add `SO_REUSEADDR` after `socket()` in `teapot_listener_open`:

```c
int yes = 1;
(void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
```

- [x] **Step 3: Fix example double-closes**

`examples/threaded_server.c`: delete `teapot_close(pc->client_socket)` in both thread funcs. `teapot_handle_client_connection` owns the fd.

`examples/thread_pool_server_crossplat.c`: the Unix path closes the listen socket inside `#else` and again after `#endif`. Close once:

```c
    job_queue_shutdown(&g_queue);
#ifdef _WIN32
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        if (workers[i])
        {
            WaitForSingleObject(workers[i], INFINITE);
            CloseHandle(workers[i]);
        }
    }
    free(workers);
#else
    for (int i = 0; i < WORKER_COUNT; ++i)
        pthread_join(workers[i], NULL);
    free(workers);
#endif
    teapot_listener_close(listen_sock);
```

Change `int r = tp_headers_check(...)` to `tp_header_result r` in both examples (kills `-Wenum` under `-Werror`).

- [x] **Step 4: Rebuild**

Run: `cc nob.c -o nob && ./nob`

Expected: existing unit tests pass; examples compile.

- [x] **Step 5: Commit**

```bash
git add stb_teapot.h examples/threaded_server.c examples/thread_pool_server_crossplat.c
git commit -m "$(cat <<'EOF'
fix: serve a client without closing, and take ownership only in handle

EOF
)"
```

---

### Task 7: Request framing tests

**Files:**
- Create: `tests/unit_test_request.c`
- Modify: `nob.c` (`tests_and_examples[]`, `unit_tests[]`)

**Interfaces:**
- Consumes: `teapot_handle_client_connection`, `TEAPOT_MAX_BODY_SIZE`, `teapot_close`
- Produces: coverage for lowercase `Content-Length`, incomplete body → 400, oversize body → 400, `GETTING` → 400

- [x] **Step 1: Write `tests/unit_test_request.c`**

Copy the socketpair harness from `origin/cursor/critical-bug-investigation-d5e3:tests/unit_test_request.c` with these tests only (no wildcard test yet — that is Task 8):

- `test_lowercase_content_length_keeps_buffered_body`
- `test_incomplete_body_rejected_before_handler`
- `test_oversized_body_rejected_before_handler`
- `test_method_prefix_rejected`

The d5e3 file is the reference. Windows stub `return 0` like response tests.

Because `teapot_handle_client_connection` now closes `sockets[0]`, the test must **not** close it again. `exchange_request` should only `teapot_close(sockets[1])`.

- [x] **Step 2: Run tests — they should pass if Tasks 4–6 are correct**

Run: `cc nob.c -o nob && ./nob`

Expected: all three unit-test binaries `ALL TESTS PASSED`. If incomplete-body still reaches the handler, `teapot_complete_request_body` is returning 0 on short reads — fix that before continuing.

- [x] **Step 3: Commit**

```bash
git add tests/unit_test_request.c nob.c
git commit -m "$(cat <<'EOF'
test: cover Content-Length, incomplete bodies, and method prefix matching

EOF
)"
```

---

### Task 8: Exact routes plus an explicit prefix flag

**Files:**
- Modify: `stb_teapot.h` (`teapot_route`, `teapot_find_handler`)
- Modify: `tests/unit_test_request.c` (add prefix tests)
- Modify: examples only if they used `/path*` — they currently do not

**Interfaces:**
- Consumes: `teapot_route` with three fields; C zero-fills a new trailing `int prefix`
- Produces: exact `strcmp` when `prefix == 0`; when `prefix != 0`, `strncmp` of `path` as a prefix (no `*` in the string, no nested ternary)

- [x] **Step 1: Write failing prefix tests**

```c
static void test_prefix_route_matches_subpath(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/", recording_handler, 1},
    };

    reset_observed();
    int rc = exchange_request("GET /api/users HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("prefix request succeeds", rc == 0);
    ok("prefix reaches handler", handler_called == 1);
}

static void test_exact_route_does_not_act_as_glob(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/", recording_handler, 0},
    };

    reset_observed();
    int rc = exchange_request("GET /api/users HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("exact miss is 404", strstr(response, "HTTP/1.1 404") != NULL);
    ok("exact miss does not reach handler", handler_called == 0);
    (void)rc;
}
```

Existing `{TEAPOT_GET, "/hello", hello_handler}` keeps working because `prefix` zero-initializes.

- [x] **Step 2: Replace `teapot_find_handler`**

```c
typedef struct
{
    teapot_method method;
    const char *path;
    teapot_handler handler;
    int prefix;
} teapot_route;

static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
{
    for (size_t i = 0; i < server->route_count; i++)
    {
        const teapot_route *r = &server->routes[i];
        if (r->method != req->method)
            continue;
        if (r->prefix)
        {
            size_t n = strlen(r->path);
            if (strncmp(r->path, req->path.items, n) == 0)
                return r->handler;
        }
        else if (strcmp(r->path, req->path.items) == 0)
        {
            return r->handler;
        }
    }
    return NULL;
}
```

Delete every check of `r->path[path_len - 1] == '*'`.

- [x] **Step 3: Run tests**

Run: `cc nob.c -o nob && ./nob`

Expected: all unit tests pass, including the two new ones.

- [x] **Step 4: Commit**

```bash
git add stb_teapot.h tests/unit_test_request.c
git commit -m "$(cat <<'EOF'
refactor: replace trailing-star route globs with an explicit prefix flag

EOF
)"
```

---

### Task 9: Demo handlers once; stop treating a server as a test

**Files:**
- Create: `examples/demo_handlers.h`
- Modify: `examples/threaded_server.c`, `examples/thread_pool_server_crossplat.c`
- Rename via git: `tests/low_level_test_stb_teapot.c` → `examples/basic_server.c`
- Delete: `tests/header_parse.c` (redundant with `unit_test_headers`)
- Modify: `nob.c` lists
- Modify: `README.md`

**Interfaces:**
- Consumes: public teapot API + `tp_headers_check`
- Produces: hello/echo in one place; examples do **not** `tp_sb_append_null` on response bodies (NUL must not enter `Content-Length`)

- [x] **Step 1: Create `examples/demo_handlers.h`**

```c
#ifndef TEAPOT_DEMO_HANDLERS_H
#define TEAPOT_DEMO_HANDLERS_H

#include "../stb_teapot.h"

static teapot_response hello_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    tp_header_line h = {0};
    tp_header_result r = tp_headers_check(&req->headers, "X-Hello", NULL, &h);
    if (r != TP_HEADER_NOT_FOUND)
        tp_sb_appendf(&resp.body, "Hello (X-Hello=%s)\n", h.value.items ? h.value.items : "");
    else
        tp_sb_appendf(&resp.body, "Hello from GET /hello\n");
    return resp;
}

static teapot_response echo_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    if (req->body_length == 0)
    {
        resp.status = TEAPOT_HTTP_BAD_REQUEST;
        tp_sb_appendf(&resp.body, "Bad Request: No body provided\n");
        return resp;
    }

    tp_header_line hdr = {0};
    tp_header_result res = tp_headers_check(&req->headers, "Content-Type", "text/plain", &hdr);
    if (res == TP_HEADER_NOT_FOUND)
    {
        resp.status = TEAPOT_HTTP_BAD_REQUEST;
        tp_sb_appendf(&resp.body, "Bad Request: Missing Content-Type header\n");
        return resp;
    }
    if (res != TP_HEADER_MATCH)
    {
        resp.status = TEAPOT_HTTP_UNSUPPORTED_MEDIA_TYPE;
        tp_sb_appendf(&resp.body, "Unsupported Media Type (%s): Only text/plain is supported\n",
                      hdr.value.items ? hdr.value.items : "");
        return resp;
    }

    tp_sb_appendf(&resp.body, "POST /echo received!\nBody (%zu bytes) %s:\n%s\n",
                  req->body_length, hdr.value.items ? hdr.value.items : "",
                  req->body.items ? req->body.items : "");
    return resp;
}

#endif
```

- [x] **Step 2: Point examples at it; move the blocking “test”**

```bash
git mv tests/low_level_test_stb_teapot.c examples/basic_server.c
```

`examples/basic_server.c` includes `demo_handlers.h` and only contains `main` + `teapot_listen`.

Delete `tests/header_parse.c`.

`nob.c` `tests_and_examples[]` becomes examples only for the servers, plus the three unit tests. `unit_tests[]` stays the three unit tests. Do not execute `basic_server` / `threaded_server` / `thread_pool_server_crossplat` from `./nob`.

- [x] **Step 3: README**

Replace the empty “Usage” paragraph with:

```markdown
## Build and test

```sh
cc nob.c -o nob && ./nob
```

`./nob` compiles examples into `build/` and runs `unit_test_headers`, `unit_test_request`, and `unit_test_response`.

## Socket ownership

`teapot_serve_client` does not close the client socket.
`teapot_handle_client_connection` takes ownership and closes it.
Do not close the same fd again.

## HTTP subset

Request line + headers + `Content-Length` body. No header folding, no chunked encoding, no pipelining. Oversize headers and bodies are 400.
```

- [x] **Step 4: Run `./nob`**

Expected: exit 0; `build/basic_server` exists; `build/header_parse` and `build/low_level_test_stb_teapot` are gone (delete stale binaries in `build/` if nob does not).

- [x] **Step 5: Commit**

```bash
git add examples/demo_handlers.h examples/basic_server.c examples/threaded_server.c examples/thread_pool_server_crossplat.c tests/header_parse.c nob.c README.md
git commit -m "$(cat <<'EOF'
refactor: share demo handlers and stop treating the sample server as a unit test

EOF
)"
```

---

### Task 10: CI runs the tests; file-size gate

**Files:**
- Modify: `.github/workflows/nob.yml`
- Modify: none if `stb_teapot.h` is already under 1000 lines; if not, this task is the shrinkage pass — no new features, only more deletion/inlining until `wc -l stb_teapot.h` is `< 1000`

**Interfaces:**
- Consumes: Task 1 `./nob` that runs unit tests
- Produces: CI fails when tests fail; header under 1000 lines

- [x] **Step 1: Update the workflow**

```yaml
name: nob

on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: build and test
        run: cc -o nob nob.c && ./nob
      - name: header stays under 1000 lines
        run: test "$(wc -l < stb_teapot.h)" -lt 1000
```

- [x] **Step 2: Enforce the line budget locally**

Run: `wc -l stb_teapot.h`

Expected: `< 1000`. If not, delete remaining ballast before anything else: leftover comments, unused `tp_headers_match`, unused format macros, duplicate platform includes. Do not “fix” the budget by wrapping lines.

- [x] **Step 3: Full verification**

Run:

```bash
cc nob.c -o nob && ./nob
wc -l stb_teapot.h
```

Expected: `./nob` exit 0; line count under 1000.

- [x] **Step 4: Commit**

```bash
git add .github/workflows/nob.yml stb_teapot.h
git commit -m "$(cat <<'EOF'
ci: run unit tests and keep stb_teapot.h under 1000 lines

EOF
)"
```

---

### Task 11: Delete the 59 Cursor branches (human / maintainer)

This is not application code. After Tasks 1–10 are on `main`:

```bash
# preview
git branch -r | grep origin/cursor/

# delete remotes (requires push access)
git push origin --delete \
  cursor/critical-bug-investigation-d5e3 \
  # ... every other origin/cursor/* name from the ranking tables
```

Do this as one scripted push of all `cursor/*` names. Keep `origin/main`.

Optional: copy the useful Cloud notes from `origin/cursor/dev-environment-setup-4d6c:AGENTS.md` (gcc vs clang `-Werror`, do not run the blocking server without `timeout`) into `README.md` if Task 9 did not already.

---

## Self-review

**Spec coverage**

| Finding | Task |
|---|---|
| Dual sscanf/strstr parser | 4 |
| Dead strtok helper / `#if 0` / TODO wall / unused DA | 2 |
| Header trim + empty name (3 failing tests) | 3 |
| `nob` `return 0` / `strtok(".c")` / tests never run | 1 |
| 256-byte send over-read + injection | 5 |
| Socket double-close / `teapot_close` not public | 6 |
| `GETTING` matches GET; incomplete body; lowercase CL | 4, 7 |
| `*` glob spaghetti / d5e3 nested ternary | 8 (explicit `prefix`) |
| Copy-pasted hello/echo; NUL in Content-Length | 9 |
| File already over 1k; d5e3 → 1232 | 2 then 10 |
| CI compile-only | 10 |
| 59 duplicate Cursor branches | ranking + Task 11 |

**Placeholder scan:** none. Branch deletion in Task 11 lists the command pattern and points at the ranking tables for the full name list.

**Type consistency:** `teapot_serve_client` / `teapot_handle_client_connection` / `teapot_socket_ok` / `teapot_close` / `teapot_listener_close` / `teapot_route.prefix` / `teapot_request.content_length` are named the same in Tasks 4–8.

---

## What not to do

- Do not merge `d5e3` and “clean it up later.” The cleanup **is** the change.
- Do not add a 70-line overflow-checked decimal parser. `strtoul` + `TEAPOT_MAX_BODY_SIZE` is the whole function.
- Do not put a thread pool in the header.
- Do not introduce `teapot_send_bad_request` plus `teapot_send_not_found` plus friends. One `teapot_send_status_body`.
- Do not leave `compile_commands.json` untracked in a commit (local clangd dump).
