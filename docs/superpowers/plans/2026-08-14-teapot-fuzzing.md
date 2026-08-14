# Teapot Fuzzing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add clang libFuzzer coverage of `teapot_serve_client` via `socketpair`, with `./nob fuzz` and a 30s CI job, without changing the gcc `./nob` loop.

**Architecture:** Harness in `tests/fuzz_serve.c` drives the public serve path. Seeds live in `tests/fuzz_corpus/`. `nob.c` gains a `fuzz` subcommand that compiles with clang sanitizers. rfc_lite grammar aborts only when `TEAPOT_FUZZ_GRAMMAR` is set. Request-line `SP+` is accepted in `parse_request` so the extra-spaces seed is not a standing CI failure.

**Tech Stack:** C17, clang libFuzzer, ASan, UBSan, nob.h, GitHub Actions.

## Global Constraints

- `stb_teapot.h` stays under 1000 lines; no fuzz-only `#ifdef` in the header.
- Default `./nob` stays gcc unit tests + valgrind; it must not build or run the fuzzer.
- Fuzzer uses public API only (`teapot_serve_client`, not `teapot_handle_client_connection`).
- POSIX fuzzer; Windows `./nob fuzz` exits 1 with a message.
- Do not reuse gcc `COMPILE_FLAGS` for the fuzzer artifact.

---

### Task 1: Accept extra spaces on the request line

**Files:**
- Modify: `tests/unit_test_request.c`
- Modify: `stb_teapot.h` (`parse_request`)

**Interfaces:**
- Consumes: existing `exchange_request`, `recording_handler`
- Produces: `parse_request` skips `SP+` after the method so `GET  /hello  HTTP/1.1` routes like `GET /hello HTTP/1.1`

- [ ] **Step 1: Write the failing test**

Add to `tests/unit_test_request.c` and call it from `main`:

```c
static void test_request_line_extra_spaces_still_routes(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", recording_handler},
    };

    reset_observed();
    int rc = exchange_request("GET  /hello  HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("extra spaces request succeeds", rc == 0);
    ok("extra spaces reaches handler", handler_called == 1);
    ok("extra spaces response is 200", strstr(response, "HTTP/1.1 200 OK") != NULL);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cc nob.c -o nob && ./nob`  
Expected: FAIL `extra spaces request succeeds` (empty path → 400)

- [ ] **Step 3: Skip SP+ after the method**

In `stb_teapot.h`, next to other parse helpers:

```c
static const char *tp_skip_spaces(const char *p, const char *end)
{
    while (p < end && *p == ' ')
        ++p;
    return p;
}
```

In `parse_request`, after finding the first space and parsing the method:

```c
const char *path0 = tp_skip_spaces(sp1 + 1, line_end);
if (path0 >= line_end)
    return -1;
const char *sp2 = (const char *)memchr(path0, ' ', (size_t)(line_end - path0));
if (!sp2)
    return -1;
size_t path_n = (size_t)(sp2 - path0);
if (path_n == 0)
    return -1;
```

Do not start validating the HTTP version. Keep the rest of `parse_request` as-is.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cc nob.c -o nob && ./nob`  
Expected: ALL TESTS PASSED. `wc -l stb_teapot.h` still `< 1000`.

- [ ] **Step 5: Commit**

```bash
git add tests/unit_test_request.c stb_teapot.h
git commit -m "fix: skip extra spaces after the HTTP method on the request line"
```

---

### Task 2: Seed corpus

**Files:**
- Create: `tests/fuzz_corpus/get-root`
- Create: `tests/fuzz_corpus/post-echo-cl`
- Create: `tests/fuzz_corpus/http10-get`
- Create: `tests/fuzz_corpus/extra-spaces`
- Create: `tests/fuzz_corpus/get-with-body`
- Create: `tests/fuzz_corpus/unknown-header`
- Create: `tests/fuzz_corpus/oversize-header`
- Create: `tests/fuzz_corpus/incomplete-body`

**Interfaces:**
- Consumes: none
- Produces: raw HTTP seed files (CRLF), used by Task 3

- [ ] **Step 1: Write the seed files**

Exact bytes (use `$'\r\n'` / Python, not bare `\n`):

- `get-root`: `GET / HTTP/1.1\r\n\r\n`
- `post-echo-cl`: `POST /echo/x HTTP/1.1\r\nhost: local\r\ncontent-length: 5\r\n\r\nhello`
- `http10-get`: `GET / HTTP/1.0\r\n\r\n`
- `extra-spaces`: `GET  /  HTTP/1.1\r\n\r\n`
- `get-with-body`: `GET / HTTP/1.1\r\nContent-Length: 3\r\n\r\nxyz`
- `unknown-header`: `GET / HTTP/1.1\r\nX-Foo: bar\r\n\r\n`
- `oversize-header`: `GET / HTTP/1.1\r\n` + 257 `N` bytes + `: v\r\n\r\n`
- `incomplete-body`: `POST /echo/x HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc`

- [ ] **Step 2: Verify CRLF**

Run: `python3 -c "import pathlib; p=pathlib.Path('tests/fuzz_corpus');
for f in sorted(p.iterdir()): b=f.read_bytes(); print(f.name, b.count(b'\r\n'), len(b))"`  
Expected: every file has `\r\n`; `get-root` is 18 bytes.

- [ ] **Step 3: Commit**

```bash
git add tests/fuzz_corpus
git commit -m "test: add libFuzzer seed corpus for HTTP serve"
```

---

### Task 3: libFuzzer harness

**Files:**
- Create: `tests/fuzz_serve.c`

**Interfaces:**
- Consumes: `teapot_serve_client`, `teapot_close`, `teapot_socket_ok`, `tp_headers_get`, corpus from Task 2
- Produces: `int fuzz_one(const uint8_t *data, size_t size)` and `LLVMFuzzerTestOneInput`

- [ ] **Step 1: Write `tests/fuzz_serve.c`**

POSIX harness. Keep `fuzz_one` as a plain function so AFL can wrap it later.

Must include:

- Dummy routes: exact `GET /`, prefix `POST /echo/` (`prefix = 1`)
- Handler records `handler_called`, `body_length`, `content_length` (0 if header missing)
- `socketpair` → write `data` → `SHUT_WR` → `teapot_serve_client` → drain → oracles → close both fds
- Empty `size == 0`: return 0
- `socketpair`/write failure: return 0
- Always abort: client fd closed after serve (`fcntl(F_GETFD)` fails); handler `body_length` > parsed Content-Length; handler `body_length` > `size`
- rfc_lite checker per spec (GET/POST/PUT/DELETE, `SP+`, HTTP/1.0|1.1, tchar names, one Content-Length, no TE, header block ≤ 8191, body exactly CL, CL ≤ `TEAPOT_MAX_BODY_SIZE`). Abort on (rfc_lite valid && (serve < 0 || response status 400)) only when `TEAPOT_FUZZ_GRAMMAR` is set and non-empty
- Close fds before `abort()`
- Never call `teapot_handle_client_connection`
- `#ifdef _WIN32`: empty `main` returning 0 (not built by `./nob fuzz` on Windows)

rfc_lite tchar: ALPHA / DIGIT / `!#$%&'*+-.^_`|~`

- [ ] **Step 2: Compile with clang and replay the corpus**

```sh
clang -O1 -g -std=c17 -fsanitize=fuzzer,address,undefined \
  -o build/fuzz_serve tests/fuzz_serve.c
TEAPOT_FUZZ_GRAMMAR=1 ./build/fuzz_serve tests/fuzz_corpus -runs=32 -timeout=2
```

Expected: exit 0, no ASan/UBSan, no abort.

Without grammar, same command must also exit 0.

- [ ] **Step 3: Commit**

```bash
git add tests/fuzz_serve.c
git commit -m "test: add libFuzzer harness for teapot_serve_client"
```

---

### Task 4: `./nob fuzz`

**Files:**
- Modify: `nob.c`

**Interfaces:**
- Consumes: `tests/fuzz_serve.c`, clang on PATH
- Produces: `./nob fuzz [libFuzzer args...]` → `build/fuzz_serve`

- [ ] **Step 1: Add the `fuzz` subcommand**

After shifting the program name, if the next arg is `fuzz`:

1. On `_WIN32`, log that fuzzing is POSIX-only and return 1.
2. If `clang` is not on PATH (try `clang` via a version probe or `nob_file_exists` on `/usr/bin/clang` **and** a PATH search — prefer running `clang -v` / `which` equivalent: `nob_needs_rebuild` is not this. Implement `have_clang()` that runs `clang -dumpversion` or checks common paths plus `PATH`). Simplest robust check: `nob_cmd_run` of `clang -dumpversion` redirected, or `access` after searching PATH.
   Recommended: iterate `PATH` for a file named `clang`, plus `/usr/bin/clang`. If missing: log `sudo apt-get install clang` and return 1.
3. `nob_mkdir_if_not_exists(BUILD_DIR)`
4. Compile (do **not** use `COMPILE_FLAGS`):

```
clang -O1 -g -std=c17 -fsanitize=fuzzer,address,undefined -o build/fuzz_serve tests/fuzz_serve.c
```

5. Run `./build/fuzz_serve tests/fuzz_corpus -max_total_time=30 -timeout=2` then append any extra args after `fuzz`.

Default `./nob` (no args) must not compile or run this binary.

- [ ] **Step 2: Verify both paths**

```sh
cc nob.c -o nob && ./nob
# must not mention fuzz_serve in the compile list
./nob fuzz -runs=32 -max_total_time=1
```

Expected: unit tests pass; fuzz command builds and exits 0.

- [ ] **Step 3: Commit**

```bash
git add nob.c
git commit -m "build: add ./nob fuzz for clang libFuzzer"
```

---

### Task 5: CI job and README

**Files:**
- Modify: `.github/workflows/nob.yml`
- Modify: `README.md`

**Interfaces:**
- Consumes: `./nob fuzz` from Task 4
- Produces: `fuzz` GitHub Actions job with `TEAPOT_FUZZ_GRAMMAR=1`

- [ ] **Step 1: Add the `fuzz` job**

Keep the existing `build` job byte-for-byte in behavior. Add:

```yaml
  fuzz:
    runs-on: ubuntu-latest
    env:
      TEAPOT_FUZZ_GRAMMAR: "1"
    steps:
      - uses: actions/checkout@v7
        with:
          submodules: true
      - name: install clang
        run: sudo apt-get update && sudo apt-get install -y clang
      - name: fuzz smoke
        run: cc -o nob nob.c && ./nob fuzz
```

- [ ] **Step 2: Document in README**

After the existing `./nob` paragraph, add that `./nob fuzz` requires clang, runs libFuzzer for 30s against `tests/fuzz_corpus`, and that CI sets `TEAPOT_FUZZ_GRAMMAR=1`. Show the raw clang compile line from the spec. Mention `./nob fuzz -max_total_time=0` for a long local run.

- [ ] **Step 3: Verify locally**

```sh
cc nob.c -o nob && ./nob
TEAPOT_FUZZ_GRAMMAR=1 ./nob fuzz
test "$(wc -l < stb_teapot.h)" -lt 1000
```

Expected: both exit 0. Fuzz job equivalent is the second command (30s).

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/nob.yml README.md
git commit -m "ci: smoke libFuzzer for 30s with the rfc_lite grammar oracle"
```
