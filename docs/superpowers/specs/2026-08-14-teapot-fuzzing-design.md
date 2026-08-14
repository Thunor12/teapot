# Teapot Fuzzing Design

**Date:** 2026-08-14  
**Status:** draft for review  
**Goal:** Catch crash, ASan/UBSan, ownership, and “rfc_lite-valid HTTP got 400” bugs on the public serve path, without changing `stb_teapot.h` or the gcc `./nob` loop.

## Constraints (locked)

- Stay a single-header library. Fuzzing does **not** modify `stb_teapot.h` (no test hook, no exported `parse_request`). Amalgam line budget is owned by the HTTP runtime spec (not 1000).
- Default `cc nob.c -o nob && ./nob` stays gcc unit tests + valgrind. Clang is not required for that path.
- Fuzzing is clang + libFuzzer + ASan + UBSan.
- Full serve path: `socketpair` → write bytes → `SHUT_WR` → `teapot_serve_client`. Same shape as `tests/unit_test_request.c`.
- POSIX-only (Linux CI). Windows does not build the fuzzer, matching `unit_test_request`.
- CI smokes the fuzzer for 30 seconds on every push/PR to `main`.
- `./nob fuzz` builds and runs the same target when clang is present; missing clang is a hard fail with an install hint.

## Architecture

Two toolchains, one public API.

```
gcc ./nob          → unit_test_*  (+ valgrind if present)
clang ./nob fuzz   → build/fuzz_serve  (-fsanitize=fuzzer,address,undefined)
```

`LLVMFuzzerTestOneInput` never calls static helpers. It drives:

1. `socketpair(AF_UNIX, SOCK_STREAM, 0, fds)`
2. Write the entire fuzzer buffer to `fds[1]`, then `shutdown(fds[1], SHUT_WR)`
3. `teapot_serve_client(&server, fds[0])`
4. Drain `fds[1]`, check oracles, close both fds

`SHUT_WR` is what prevents a huge `Content-Length` from hanging: `teapot_complete_request_body` gets `read == 0` and returns -1. libFuzzer `-timeout=2` is a backstop, not the primary hang control.

Headers accumulate into `TEAPOT_CONN_BUF` (8192) with no reserved NUL. A header block that never finds `\r\n\r\n` in 8192 bytes is 400. Body bytes already past the blank line in that buffer count toward `Content-Length`; the rest complete from the socket. Seeds must cover both “headers+body in the first recv” and “headers in the first recv, body completed from the socket.”

## Components

| Path | Role |
| --- | --- |
| `tests/fuzz_serve.c` | libFuzzer harness, dummy routes, oracles, rfc_lite checker |
| `tests/fuzz_corpus/` | checked-in seed files (raw HTTP bytes) |
| `nob.c` | `fuzz` subcommand only; default build list unchanged |
| `.github/workflows/nob.yml` | existing `build` job unchanged; new `fuzz` job |
| `README.md` | document `./nob fuzz` and the raw clang line |

### Harness (`tests/fuzz_serve.c`)

Dummy server, process-lifetime, reset per input:

- Exact `GET /` → handler
- Prefix `POST /echo/` → handler (`prefix = 1`)

The handler records `method`, `path` (copied, capped), `body_length`, and whether it ran. It returns HTTP 200, `text/plain`, body `ok`. It must not set a `Content-Type` containing CR/LF.

Per input:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
```

Empty input: return 0 without aborting (recv fails; that is not a bug).

### Corpus (`tests/fuzz_corpus/`)

One file per seed, no extension required. Minimum seeds:

- `get-root` — `GET / HTTP/1.1\r\n\r\n`
- `post-echo-cl` — POST `/echo/x` with lowercase `content-length` and a short body
- `http10-get` — `GET / HTTP/1.0\r\n\r\n`
- `extra-spaces` — `GET  /  HTTP/1.1\r\n\r\n` (multiple spaces on the request line)
- `get-with-body` — GET `/` with `Content-Length` and a body
- `unknown-header` — GET `/` plus `X-Foo: bar`
- `oversize-header` — a header name or value over teapot’s limits (not rfc_lite-valid; mutation starting point)
- `incomplete-body` — POST with `Content-Length` larger than the written body (not rfc_lite-valid)

### `./nob fuzz`

If `clang` is not on `PATH`, log a one-line install hint (`sudo apt-get install clang` on Ubuntu) and **exit 1**.

Otherwise compile:

```
clang -O1 -g -std=c17 -fsanitize=fuzzer,address,undefined \
  -o build/fuzz_serve tests/fuzz_serve.c
```

Do not reuse gcc `COMPILE_FLAGS` (`-Werror`, `_FORTIFY_SOURCE`, etc.) for this artifact. Fuzzer sanitizers conflict with some of those.

Then run (CI and default local):

```
./build/fuzz_serve tests/fuzz_corpus -max_total_time=30 -timeout=2
```

Arguments after `fuzz` are appended to that command so a local long run is:

```
./nob fuzz -max_total_time=0
```

(`-max_total_time=0` means no time cap in libFuzzer.)

Default `./nob` must not compile or invoke `fuzz_serve`.

### CI

`.github/workflows/nob.yml`:

- Job `build`: unchanged (`actions/checkout@v7`, submodules, valgrind, `cc -o nob nob.c && ./nob`, line budget).
- Job `fuzz`: `ubuntu-latest`, checkout with submodules, `sudo apt-get install -y clang`, `cc -o nob nob.c && ./nob fuzz`.

The fuzz job sets `TEAPOT_FUZZ_GRAMMAR=1` in the environment so rfc_lite mismatches fail CI.

## Oracles

### Always abort (CI-failing)

1. Process crash, ASan error, UBSan error, or libFuzzer `-timeout=2`.
2. After `teapot_serve_client` returns, `fcntl(fds[0], F_GETFD)` fails — serve closed a socket it does not own.
3. Handler ran and `req->body_length` is greater than the `Content-Length` value the handler observed from `tp_headers_get(req->headers, "Content-Length")` (missing header counts as 0).
4. Handler ran and `req->body_length` is greater than `size` (bytes the fuzzer wrote).

### Expected, never abort

- `teapot_serve_client` returns -1 (parse/body failure).
- HTTP 400 or 404 on the wire.
- Handler not called.
- First recv empty or truncated garbage.

### rfc_lite grammar (CI on)

A checker **in `tests/fuzz_serve.c`**, not in `stb_teapot.h`. It answers: “would a slightly wider HTTP/1.x client consider this a complete request?”

If the checker returns true **and** teapot treated the connection as a client error (serve returns -1, **or** the drained response status is 400), `abort()`. 404 after a successful parse is **not** a grammar failure.

Enabled when `getenv("TEAPOT_FUZZ_GRAMMAR")` is non-NULL and the string is not empty. CI sets `TEAPOT_FUZZ_GRAMMAR=1`. A local run without the env var does not abort on grammar mismatches.

#### rfc_lite: valid

The buffer is a single request (no pipelining):

1. **Request line** (CRLF-terminated): `METHOD SP+ PATH SP+ VERSION CRLF`
   - `METHOD` is exactly `GET`, `POST`, `PUT`, or `DELETE` (teapot’s `parse_method` set). `HEAD` / `PATCH` / `OPTIONS` are **not** rfc_lite-valid.
   - `SP+` is one or more `0x20` bytes. Tabs are not allowed on the request line.
   - `PATH` is one or more bytes, none of which are `SP`, CR, or LF.
   - `VERSION` is exactly `HTTP/1.0` or `HTTP/1.1`.
2. **Headers:** zero or more lines `NAME ":" OWS VALUE OWS CRLF`
   - `NAME` is one or more RFC 9110 `tchar` bytes (no empty name, no spaces in the name).
   - `VALUE` has no CR or LF (no folding, no obs-fold).
   - Unknown names are allowed.
   - At most one `Content-Length` header (case-insensitive). Duplicates are not rfc_lite-valid.
   - `NAME` length ≤ `TP_MAX_HEADER_NAME_LEN` (256). `VALUE` length ≤ `TP_MAX_HEADER_VALUE_LEN` (4096). Oversize is a deliberate teapot 400, not a grammar finding.
3. **End of headers:** a blank line `CRLF`.
4. **Header block size:** request line + headers + blank line is ≤ 8192 bytes (`TEAPOT_CONN_BUF`; no forced NUL). Larger “valid HTTP” that never fits in the conn buffer is out of scope for this oracle.
5. **Body:**
   - If `Content-Length` is absent, there are no extra bytes after the blank line (or they are ignored by the checker: extra trailing bytes make the input **invalid** so pipelining does not false-alarm).
   - If present, the value is a base-10 integer with optional trailing spaces, `end` after digits+spaces as `strtoul` would accept, `0 ≤ n ≤ TEAPOT_MAX_BODY_SIZE` (4 MiB). GET with a body is allowed.
   - The number of bytes after the header blank line is **exactly** `n`. Incomplete or over-long bodies are not rfc_lite-valid (teapot 400 on incomplete body is expected).
6. **Transfer-Encoding:** if any header name matches `Transfer-Encoding` (case-insensitive), the input is **not** rfc_lite-valid (no chunked).

#### rfc_lite: not valid (teapot 400 is fine)

- Unknown method, HTTP/2, LF-only lines, header folding, empty header name, missing colon, oversize name/value, `Content-Length` over `TEAPOT_MAX_BODY_SIZE`, incomplete body, chunked encoding, pipelining, header block larger than 8192 bytes.

If CI fails because current teapot 400s an rfc_lite-valid seed, that is a **product decision**: either accept that request in `stb_teapot.h` or tighten this grammar. Do not delete the oracle to go green.

**Known first finding:** teapot takes the path as the bytes between the first and second `SP` on the request line. `GET  / HTTP/1.1` (two spaces after `GET`) is rfc_lite-valid (`SP+`) and today’s parser sees an empty path and 400s. The `extra-spaces` seed is expected to fail the grammar oracle until that is decided. HTTP/1.0, unknown headers, and GET-with-body are expected to already parse.

## Error handling in the harness

- `socketpair` failure: return 0 (environment, not a teapot bug).
- Write failure: return 0.
- Always `teapot_close` both fds on every path after a successful `socketpair`, including abort paths that ASan will still see as process death — prefer closing before `abort()` on oracle failures so leak reports stay about teapot, not the harness.
- Reset handler observation globals at the start of each input.
- Do not call `teapot_handle_client_connection` (that closes the fd by design and would trip the ownership oracle).

## Testing plan

| Command | What it proves |
| --- | --- |
| `./nob` | gcc unit tests still pass; fuzzer not involved |
| `./nob fuzz` | clang present, 30s smoke, grammar on if the env is set |
| CI `fuzz` job | same as `./nob fuzz` with `TEAPOT_FUZZ_GRAMMAR=1` |
| `./nob fuzz -max_total_time=0` | local campaign, no time cap |
| `TEAPOT_FUZZ_GRAMMAR= ./nob fuzz` | local smoke without grammar abort |

Pass for CI: 30 seconds, exit 0, no sanitizer report, no oracle `abort`.

## Out of scope

- AFL++ / honggfuzz / OSS-Fuzz (the `fuzz_one` logic in `fuzz_serve.c` should stay a plain function so those can wrap it later).
- Fuzzing `teapot_send_response` except as a side effect of the dummy 200.
- Fuzzing listen/accept/bind.
- Windows fuzzer.
- Changing `TEAPOT_MAX_BODY_SIZE`, recv buffer size, or the public API.

## Success criteria

1. `./nob` on gcc is unchanged in behavior.
2. `./nob fuzz` on a machine with clang builds `build/fuzz_serve` and runs 30s against `tests/fuzz_corpus`.
3. CI `fuzz` job does the same with grammar enabled.
4. A crashing input saved by libFuzzer is a regression: copy it into `tests/fuzz_corpus/` or add a focused case in `unit_test_request.c`.
5. No fuzz-only `#ifdef` in the header. Amalgam line budget is owned by the HTTP runtime spec (not 1000). rfc_lite header size follows `TEAPOT_CONN_BUF` (8192), not 8191.
