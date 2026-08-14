# Teapot HTTP Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a lean C HTTP/1.1 application server (axum / actix-web niche): JSON/bytes/text first, `teapot_listen` plus multiplexed `teapot_run`, generated amalgam, fail-closed framing preserved.

**Architecture:** Source of truth is `src/`; `./nob amalgamate` emits `stb_teapot.h`. Internal `teapot_conn_step` never blocks. `teapot_listen` is a 250 ms poll + `handle_client_connection` loop. `teapot_run` is one reactor in `tp_run.c` plus wait shims. Templates are a later, separate plan.

**Tech Stack:** C17, POSIX + Winsock, `nob.c`, gcc `./nob`, clang libFuzzer `./nob fuzz`.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-08-14-teapot-listener-builder-design.md` (locked). Templates spec is **not** this plan.
- Product: axum/actix analogue. No TLS, HTTP/2, WebSockets, HTML escape in the amalgam, `teapot_app`, or `teapot_run_blocking`.
- Users: `#define STB_TEAPOT_IMPLEMENTATION` + `#include "stb_teapot.h"`. Tests compile only the amalgam.
- No line-count gates on `src/` or the amalgam.
- HTTP subset unchanged: CL only, reject TE / mismatched dup CL / pipelining; `Connection: close`.
- `{ .port, .routes, .route_count }` keeps compiling; new `teapot_server` fields **appended**.
- Incomplete request + `recv==0`/`SHUT_WR` → **400 + `rc == -1`**. Idle timeout, peer open → **`rc == -1`, no 400 required**.
- Library does not `printf` and does not install signal handlers.

## Merge gate (every task)

A task is mergeable only when **all** of these pass:

1. New unit tests for that task (TDD: RED then GREEN).
2. `cc -o nob nob.c && ./nob` (existing unit tests + examples).
3. `./nob fuzz` when clang is present (always on CI; locally skip only if clang is missing **and** the task did not touch parse/serve).
4. `/thermo-nuclear-review` on the task branch (bugs, breaks, security, devex). Fix High/Medium before merge.
5. No hand-edited amalgam: `./nob amalgamate && git diff --exit-code stb_teapot.h`.

After **all** tasks merge, re-run the skill audit against the axum/actix objective. If gaps remain, write a follow-up spec/plan. Do **not** start `docs/superpowers/specs/2026-08-14-teapot-templates-design.md` until that audit.

Work on a feature branch, not `main`, unless the user says otherwise.

---

## File map

| Path | Role |
| --- | --- |
| `src/teapot.h` | Public preamble (guards, types, prototypes, DA macros) |
| `src/tp_platform.c` | sockets, close, read/write, nonblock, `tp_now_ms` |
| `src/tp_parse.c` | headers, request-line, Content-Length |
| `src/tp_http.c` | routing, serialize, helpers, `serve_client` (until Task 4) |
| `src/tp_conn.c` | conn init/step/free (Task 4) |
| `src/tp_listen.c` | bind/accept/`teapot_listen` |
| `src/tp_run.c` | `teapot_run` (Task 5) |
| `src/tp_wait_poll.c` | POSIX poll shim (Task 5) |
| `src/tp_wait_epoll.c` | epoll shim (Task 6) |
| `src/tp_wait_kqueue.c` | kqueue shim (Task 7) |
| `src/tp_wait_wsapoll.c` | WSAPoll shim (Task 7) |
| `src/tp_wait_wfmo.c` | WFMO shim (Task 7) |
| `nob.c` | `amalgamate` + existing tests/fuzz |
| `stb_teapot.h` | generated only |
| `tests/unit_test_*.c` | gates |
| `examples/basic_server.c` | JSON/`teapot_listen`/SIGINT/127.0.0.1 |
| `examples/epoll_server.c` | same routes, `teapot_run` (Task 6) |

Amalgam order: `teapot.h` preamble, `#ifdef STB_TEAPOT_IMPLEMENTATION`, then `tp_platform.c`, `tp_parse.c`, `tp_http.c`, `tp_conn.c` (once it exists), `tp_listen.c`, wait files, `tp_run.c`. Strip `#include "tp_*.h"` and `#include "teapot.h"` from `.c` files.

---

### Task 1: Amalgamate identity (no behavior change)

**Files:**
- Create: `src/teapot.h`, `src/tp_platform.c`, `src/tp_parse.c`, `src/tp_http.c`, `src/tp_listen.c`
- Modify: `nob.c` (add `amalgamate`), `.github/workflows/nob.yml` (replace `< 1000` with amalgam regenerate + 1800 + per-file 400)
- Modify: `stb_teapot.h` (generated output of the split)
- Test: existing `./nob` suite (characterization)

**Interfaces:**
- Consumes: current `stb_teapot.h`
- Produces: `./nob amalgamate` writes `stb_teapot.h` with banner `/* GENERATED — do not edit. Source: src/ */`

Move functions without editing logic:

- `tp_platform.c`: `teapot_init`, `teapot_socket_ok`, `teapot_close`, `teapot_listener_close`, `teapot_read`, `teapot_write`, `teapot_write_all`
- `tp_parse.c`: `tp_stricmp`, header find/get/check, `tp_span` helpers, `tp_parse_and_append_header_line`, `tp_parse_header_block`, `tp_extract_header_keyval`, `teapot_content_length_from_headers`, `parse_request`, `free_request`, `parse_method`
- `tp_http.c`: `tp_sb_appendf`, `teapot_status_to_str`, `teapot_find_handler`, `teapot_send_status_body`, `teapot_recv_request`, `teapot_send_response`, `teapot_complete_request_body`, `teapot_serve_client`, `teapot_handle_client_connection`
- `tp_listen.c`: `teapot_listener_open`, `teapot_listener_accept`, `teapot_listen`
- `src/teapot.h`: everything currently above `#ifdef STB_TEAPOT_IMPLEMENTATION`

- [ ] **Step 1: Add `amalgamate` to `nob.c`**

Walk the file list above. Emit banner, copy `src/teapot.h`, then `#ifdef STB_TEAPOT_IMPLEMENTATION`, concatenate each `.c` skipping `#include "…"` lines that reference `teapot.h` or `tp_*.h`, then `#endif`. If `argc>=2 && strcmp(argv[1],"amalgamate")==0`, run only that and exit 0.

Default `./nob` must call amalgamate **before** compiling tests.

- [ ] **Step 2: Split `stb_teapot.h` into `src/` with zero logic change**

Keep `static` on internal helpers. Public functions stay non-static.

- [ ] **Step 3: Generate amalgam and confirm it builds the old tests**

Run:

```bash
cc -o nob nob.c && ./nob amalgamate && git diff --stat stb_teapot.h
cc -o nob nob.c && ./nob
```

Expected: all unit tests PASS. Behavior identical (one-shot recv still).

- [ ] **Step 4: Replace CI line gate**

In `.github/workflows/nob.yml`, replace the 1000-line step with:

```yaml
      - name: amalgam is generated and within budget
        run: |
          cc -o nob nob.c && ./nob amalgamate
          git diff --exit-code stb_teapot.h
          test "$(wc -l < stb_teapot.h)" -lt 1800
          for f in src/*.c src/*.h; do test "$(wc -l < "$f")" -lt 400; done
```

- [ ] **Step 5: Commit**

```bash
git add src nob.c stb_teapot.h .github/workflows/nob.yml
git commit -m "$(cat <<'EOF'
build: amalgam stb_teapot.h from src/ with no behavior change

EOF
)"
```

Merge gate: `./nob`, amalgamate diff-empty, thermo-nuclear-review (expect no functional findings).

---

### Task 2: Bind host, backlog, stop, listen tick

**Files:**
- Modify: `src/teapot.h` (`teapot_server` fields appended, `teapot_request_stop` prototype)
- Modify: `src/tp_listen.c`, `src/tp_platform.c` (optional one-fd wait helper)
- Modify: `examples/basic_server.c` (127.0.0.1, SIGINT → stop, drop library printf)
- Test: `tests/unit_test_listen.c` (new)

**Interfaces:**
- Consumes: `teapot_listener_open` / `teapot_listen`
- Produces:

```c
void teapot_request_stop(teapot_server *server);
/* teapot_server append: bind_host, backlog, stop, user, max_conns */
/* listener_open: inet_pton; getsockname into server->port; backlog 0 → 8 */
/* teapot_listen: poll/WSAPoll 250ms; return 0 on stop, 1 on bind fail; no printf */
```

- [ ] **Step 1: Write failing tests in `tests/unit_test_listen.c`**

```c
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void ok(const char *name, int cond)
{
    if (cond) printf("[PASS] %s\n", name);
    else { printf("[FAIL] %s\n", name); failures++; }
}

static void test_bind_host_loopback(void)
{
    teapot_server server = {.port = 0, .bind_host = "127.0.0.1"};
    stb_teapot_socket_t ls = (stb_teapot_socket_t)-1;
    ok("bind_host open", teapot_listener_open(&server, &ls) == 0);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ok("getsockname", getsockname(ls, (struct sockaddr *)&addr, &len) == 0);
    ok("is loopback", addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    ok("port written back", server.port != 0);
    teapot_listener_close(ls);
}

static void test_bind_host_localhost_fails(void)
{
    teapot_server server = {.port = 0, .bind_host = "localhost"};
    stb_teapot_socket_t ls = (stb_teapot_socket_t)-1;
    ok("localhost fails", teapot_listener_open(&server, &ls) == -1);
}

struct stop_ctx { teapot_server *server; int rc; volatile int done; };

static void *listen_thread(void *arg)
{
    struct stop_ctx *ctx = arg;
    ctx->rc = teapot_listen(ctx->server);
    ctx->done = 1;
    return NULL;
}

static void test_listen_returns_when_stop_set(void)
{
    teapot_server server = {.port = 0, .bind_host = "127.0.0.1"};
    teapot_request_stop(&server);
    struct stop_ctx ctx = {.server = &server, .rc = -99, .done = 0};
    pthread_t th;
    ok("thread", pthread_create(&th, NULL, listen_thread, &ctx) == 0);
    int i;
    for (i = 0; i < 40 && !ctx.done; ++i) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};
        nanosleep(&ts, NULL);
    }
    ok("listen returned", ctx.done == 1 && ctx.rc == 0);
    if (ctx.done) pthread_join(th, NULL);
}

int main(void)
{
    test_bind_host_loopback();
    test_bind_host_localhost_fails();
    test_listen_returns_when_stop_set();
    return failures ? 1 : 0;
}
```

Add the source to `tests_and_examples` / `unit_tests` in `nob.c` like the other unit tests.

- [ ] **Step 2: Run test — expect FAIL** (missing `teapot_request_stop`, still `INADDR_ANY`, listen hangs)

- [ ] **Step 3: Implement**

`teapot_server` append only:

```c
    const char *bind_host;
    int backlog;
    volatile sig_atomic_t stop;
    void *user;
    int max_conns;
```

Need `#include <signal.h>` in `src/teapot.h` for `sig_atomic_t`.

`teapot_request_stop`: `server->stop = 1;`

`listener_open`: `backlog = server->backlog ? server->backlog : 8`; `bind_host == NULL` → `INADDR_ANY`; else `inet_pton(AF_INET, …)` or return -1; after bind, `getsockname` into `server->port`. Keep `SO_REUSEADDR` and `teapot_init()`.

`teapot_listen`: no `printf`. Loop: wait listen fd 250 ms (`poll` POSIX / `WSAPoll` Windows). On `stop`, close listen, return 0. On readable, `accept` + `teapot_handle_client_connection`. Bind fail → 1.

`basic_server.c`: `.bind_host = "127.0.0.1"`; SIGINT handler calls `teapot_request_stop(&server)` (server must be reachable — static `teapot_server *g_srv`).

- [ ] **Step 4: `./nob` — all PASS including new listen tests**

- [ ] **Step 5: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat: bind_host, request_stop, and a stoppable teapot_listen

EOF
)"
```

Merge gate: tests + fuzz (listen not on fuzz path; still run if clang present) + thermo-nuclear-review.

---

### Task 3: Response helpers + request user + query routing

**Files:**
- Modify: `src/teapot.h`, `src/tp_http.c`, `src/tp_parse.c` (route match uses path up to `?`)
- Modify: `tests/unit_test_response.c`, `tests/unit_test_request.c`
- Modify: `examples/demo_handlers.h` / `basic_server.c` to show `teapot_json` on `/ping`

**Interfaces:**
- Produces:

```c
teapot_response teapot_text(int status, const char *s);
teapot_response teapot_json(int status, const char *json);
teapot_response teapot_bytes(int status, const char *ctype, const void *p, size_t n);
/* teapot_request.user copied from server->user at dispatch */
```

- [ ] **Step 1: Failing tests**

In `unit_test_response.c`:

```c
static void test_teapot_json_sets_ctype_and_body(void)
{
    teapot_response r = teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
    ok("json ctype", r.content_type && strcmp(r.content_type, "application/json") == 0);
    ok("json body", r.body.count == 11 && memcmp(r.body.items, "{\"ok\":true}", 11) == 0);
    teapot_response_free(&r);
}

static void test_teapot_bytes_crlf_ctype_rejected_on_send(void)
{
    stb_teapot_socket_t sp[2];
    ok("socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
    teapot_response r = teapot_bytes(TEAPOT_HTTP_OK, "a\r\nX: b", "x", 1);
    ok("send rejects", teapot_send_response(sp[0], &r) == -1);
    teapot_response_free(&r);
    teapot_close(sp[0]);
    teapot_close(sp[1]);
}
```

In `unit_test_request.c`: `GET /hello?x=1 HTTP/1.1\r\n\r\n` against route `/hello` → 200 and handler called. Copy `server.user` into `req->user` (handler records it).

- [ ] **Step 2: Run — FAIL** (missing symbols / query 404)

- [ ] **Step 3: Implement helpers as fully initializing return-by-value.** Route compare uses request-target up to first `?`. At dispatch, `req.user = server->user`.

- [ ] **Step 4: `./nob` PASS**

- [ ] **Step 5: Commit** `feat: teapot_json/text/bytes, query strip, request.user`

---

### Task 4: `teapot_conn_step` + rewrite `teapot_serve_client`

**Files:**
- Create: `src/tp_conn.c`
- Modify: `src/tp_http.c` (serve_client becomes poll + step; remove one-shot recv path)
- Modify: `src/tp_platform.c` (`tp_now_ms`, `teapot_set_nonblock`)
- Modify: `tests/unit_test_request.c` (split-header RED), `tests/unit_test_timeout.c` (PASS string)
- Modify: amalgamate file list in `nob.c`

**Interfaces:**
- Consumes: parse + serialize from Task 1–3
- Produces: internal `teapot_conn_init` / `step` / `free`; `teapot_serve_client` per spec

- [ ] **Step 1: Write the first RED in `unit_test_request.c`**

Reuse `split_serve_thread` + `FIONREAD` drain from `test_split_body_reads_remaining_bytes`:

```c
static teapot_response pong_body_handler(const teapot_request *req)
{
    (void)req;
    teapot_response r;
    teapot_response_init(&r, TEAPOT_HTTP_OK);
    teapot_response_write(&r, "pong-body", 9);
    return r;
}

static void test_split_headers_accumulate(void)
{
    stb_teapot_socket_t sockets[2];
    ok("split hdr socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    ok("split hdr first write",
       write_all_raw(sockets[1], "GET /hello HTTP/1.1\r\n", 21) == 0);

    teapot_route routes[] = {{TEAPOT_GET, "/hello", pong_body_handler}};
    teapot_server server = {.port = 0, .routes = routes, .route_count = 1};
    struct split_serve_ctx ctx = {.server = &server, .fd = sockets[0], .rc = 1};
    pthread_t th;
    ok("split hdr thread", pthread_create(&th, NULL, split_serve_thread, &ctx) == 0);

    int drained = 0;
    for (int i = 0; i < 2000; ++i) {
        int unread = 0;
        if (ioctl(sockets[0], FIONREAD, &unread) == 0 && unread == 0) {
            drained = 1;
            break;
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};
        nanosleep(&ts, NULL);
    }
    ok("split hdr drained", drained);
    ok("split hdr blank line", write_all_raw(sockets[1], "\r\n", 2) == 0);
    pthread_join(th, NULL);

    char response[512];
    ssize_t n = read(sockets[1], response, sizeof(response) - 1);
    if (n < 0) n = 0;
    response[n] = '\0';
    ok("split hdr rc", ctx.rc == 0);
    ok("split hdr 200", strstr(response, "HTTP/1.1 200") != NULL);
    ok("split hdr body", strstr(response, "pong-body") != NULL);
    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}
```

Do **not** write both fragments before `serve_client`. Call this from `main`.

- [ ] **Step 2: Run `./nob` — this test FAIL** (400 / `rc == -1`). Other tests still PASS.

- [ ] **Step 3: Implement `tp_conn.c` + rewrite `serve_client` exactly as the spec**

Lock list (do not invent):

- `deadline_ms = now + TEAPOT_RECV_TIMEOUT_MS` at init; does not reset.
- READ_HEAD: `recv` into `in+in_len`, length `TEAPOT_CONN_BUF - in_len`; find `\r\n\r\n` on the accumulated buffer; `extra > body_need` → 400 (pipeline including CL=0).
- Body lives in `req.body`, not `in[]`.
- `recv==0` incomplete → 400 flush then DONE, `failed=1`.
- Deadline / `recv<0` not EAGAIN / send error not EAGAIN → ERROR, no 400.
- `serve_client`: set nonblock; poll/WSAPoll; never 0 ms timeout; READ wait `min(250, remaining deadline)`; WRITE wait 250 ms; `server->stop` aborts with -1.
- Copy `server->user` into `req.user` before the handler.
- `conn_free` does not close fd.

- [ ] **Step 4: Run `./nob` — all request/timeout/response tests PASS.** Fix timeout PASS string to “times out without handler”.

- [ ] **Step 5: `./nob fuzz` (required).** Expected: no crash. Split-header corpus may now 200; that is intended.

- [ ] **Step 6: Commit** `feat: accumulate headers in teapot_serve_client via conn_step`

Merge gate: tests + fuzz + thermo-nuclear-review (Slowloris deadline, pipeline CL=0, fd not closed by step).

---

### Task 5: POSIX poll wait shim + `teapot_run`

**Files:**
- Create: `src/tp_wait_poll.c`, `src/tp_run.c`
- Modify: `src/teapot.h` (`teapot_run`, `TEAPOT_WAIT_*`, `TEAPOT_USE_*`)
- Test: `tests/unit_test_run.c`

**Interfaces:**
- Consumes: `teapot_conn_step`, `listener_open`
- Produces: `int teapot_run(teapot_server *server);` — 0 on stop, 1 on setup fail

`tp_wait` complete struct in `tp_wait_poll.c`. `tp_run.c` stack-allocates it. Slab: `TEAPOT_MAX_CONNS` (128) unless `server->max_conns > 0`. Scan READ slots on every wait return including timeout.

- [ ] **Step 1: Failing `unit_test_run.c`**

```c
#define TEAPOT_USE_POLL
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static teapot_response ping(const teapot_request *req)
{
    (void)req;
    return teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
}

struct run_ctx { teapot_server *s; int rc; };

static void *run_thread(void *arg)
{
    struct run_ctx *c = arg;
    c->rc = teapot_run(c->s);
    return NULL;
}

static void test_run_port0_ping_then_stop(void)
{
    teapot_route routes[] = {{TEAPOT_GET, "/ping", ping}};
    teapot_server srv = {
        .bind_host = "127.0.0.1",
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };
    struct run_ctx ctx = {.s = &srv, .rc = -99};
    pthread_t th;
    ok("run thread", pthread_create(&th, NULL, run_thread, &ctx) == 0);
    int i, port = 0;
    for (i = 0; i < 100 && (port = srv.port) == 0; ++i) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&ts, NULL);
    }
    ok("ephemeral port", port != 0);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    ok("connect", connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    const char *req = "GET /ping HTTP/1.1\r\n\r\n";
    ok("write", write(fd, req, strlen(req)) == (ssize_t)strlen(req));
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) n = 0;
    buf[n] = '\0';
    ok("200 json", strstr(buf, "HTTP/1.1 200") && strstr(buf, "{\"ok\":true}"));
    close(fd);

    teapot_request_stop(&srv);
    pthread_join(th, NULL);
    ok("run returned 0", ctx.rc == 0);
}
```

Compile this file with `-DTEAPOT_USE_POLL` already in the source. `nob.c` must compile `unit_test_run` like other tests.

- [ ] **Step 2: Run — FAIL** (`teapot_run` missing)

- [ ] **Step 3: Implement poll shim + `teapot_run` per spec** (listen nonblock, accept until EAGAIN, slab, disarm on full/`add` fail, close on DONE/ERROR, deadline scan, stop).

- [ ] **Step 4: `./nob` PASS** including `unit_test_run`

- [ ] **Step 5: Commit** `feat: teapot_run with POSIX poll`

---

### Task 6: epoll backend (Linux default)

**Files:**
- Create: `src/tp_wait_epoll.c`
- Modify: `src/teapot.h` defaults (`__linux__` → `TEAPOT_WAIT_EPOLL`)
- Create: `examples/epoll_server.c` (same routes as basic, `teapot_run`, SIGINT, 127.0.0.1)
- Modify: `nob.c` to compile `epoll_server` on Linux; compile `unit_test_run` a second time as `unit_test_run_epoll` **without** `TEAPOT_USE_POLL` (default epoll)

**Interfaces:**
- Consumes: same `tp_wait_*` API
- Produces: default Linux `teapot_run` uses epoll

- [ ] **Step 1: Duplicate `unit_test_run.c` as `tests/unit_test_run_epoll.c` without `#define TEAPOT_USE_POLL`.** Same assertions.

- [ ] **Step 2: Run — FAIL or wrongly use poll if default not wired**

- [ ] **Step 3: Implement epoll shim** (`epoll_create1`, `epoll_ctl` ADD/MOD/DEL, `epoll_wait`). Level-triggered. No WFMO `if` in `tp_run.c`.

- [ ] **Step 4: `./nob` PASS both run tests. `./nob fuzz` PASS.**

- [ ] **Step 5: Commit** `feat: epoll teapot_run default on Linux`

---

### Task 7: kqueue / WSAPoll / WFMO compile variants

**Files:**
- Create: `src/tp_wait_kqueue.c`, `src/tp_wait_wsapoll.c`, `src/tp_wait_wfmo.c`
- Modify: README (document `TEAPOT_USE_*`, WFMO 64-fd cap)
- CI: Linux still only poll + epoll. Optional: `clang -fsyntax-only` is **not** required for Windows files on Linux (they `#error` or wrap in `#if TEAPOT_WAIT ==`). Each wait file is entirely inside `#if TEAPOT_WAIT == N` so unused backends compile to empty.

**Interfaces:**
- WFMO: `WSAEventSelect`, `WSAWaitForMultipleEvents`, **`WSAEnumNetworkEvents` after wait**, `add` returns -1 at 64 events.
- kqueue: level-triggered (no `EV_ONESHOT` / `EV_CLEAR`).

- [ ] **Step 1: Add the three files wrapped in `#if TEAPOT_WAIT == …`.** On Linux `./nob amalgamate` still < 1800. `./nob` still PASS.

- [ ] **Step 2: Document in README.** No Linux CI gate for WFMO.

- [ ] **Step 3: Commit** `feat: kqueue, WSAPoll, and WFMO wait shims`

Merge gate: Linux tests + fuzz + thermo-nuclear-review (WFMO event reset, add-fail disarms listen — review the code even if unrun on Linux).

---

### Task 8: Docs, examples, fuzzing spec/CI alignment

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-08-14-teapot-fuzzing-design.md` (already notes 8192 / not 1000 — confirm CI and success criteria match)
- Modify: `examples/basic_server.c` if still binding all interfaces
- Modify: fuzz oracle if it assumes 8191-byte NUL-terminated first recv

- [ ] **Step 1: README** — JSON `/ping` example; `teapot_listen` vs `teapot_run`; bind `127.0.0.1`; reverse proxy for TLS/h2; templates are optional and not this release.

- [ ] **Step 2: `./nob` + `./nob fuzz` PASS**

- [ ] **Step 3: Commit** `docs: axum/actix-shaped teapot_listen and teapot_run`

---

### Task 9: Skill audit of the implementation (no new features)

After Task 8 is merged:

- [ ] Dispatch brainstorming / TDD / writing-plans / thermo-nuclear / code-quality / senior review against **the code** and the axum/actix objective.
- [ ] If they agree the HTTP product is met: stop. Templates stay a **new** plan from `2026-08-14-teapot-templates-design.md`.
- [ ] If they find gaps that are in-scope for the HTTP spec (missed query strip, listen hang, etc.): file a follow-up task, do not start templates.

---

## Spec coverage

| Spec requirement | Task |
| --- | --- |
| Amalgam + CI gates | 1, 8 |
| bind_host / backlog / port 0 / stop / listen tick | 2 |
| `teapot_json` / text / bytes, `user`, query strip | 3 |
| conn_step, split headers, 400 vs timeout, serve_client | 4 |
| `teapot_run` poll + slab + deadline scan | 5 |
| epoll default | 6 |
| kqueue / WSAPoll / WFMO | 7 |
| README / fuzz oracle | 8 |
| Templates | **not this plan** |

## Placeholder scan

None. Type names: `teapot_server`, `teapot_run`, `teapot_listen`, `teapot_request_stop`, `teapot_json`, `tp_wait_*`, `teapot_conn_*` (internal).
