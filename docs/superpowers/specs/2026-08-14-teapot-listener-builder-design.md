# Teapot HTTP Runtime Design

**Date:** 2026-08-14  
**Status:** locked for implementation plan  
**Goal:** A lean C HTTP/1.1 application server in the same niche as axum and actix-web: route a request to a handler, return a response. JSON, bytes, and text are first-class. HTML is one content type, not the product.

## Product position

Teapot is a **single-header C library** for writing HTTP services. The unit of work is:

```
TCP bytes → parse HTTP/1.1 → match route → handler(request) → response → bytes
```

That is axum `Router` + `serve` / actix-web `App` + `HttpServer`, not a static-site or HTML toolkit.

| Those frameworks have | Teapot v1 has |
| --- | --- |
| Route table + handlers | `teapot_route[]` + `teapot_handler` |
| App / State | `teapot_server.user` copied to `teapot_request.user` |
| Blocking or async runtime | `teapot_listen` (one client at a time) or `teapot_run` (one multiplexed thread) |
| JSON / bytes / HTML responses | `teapot_json` / `teapot_text` / `teapot_bytes` (no serde, no JSON parser) |
| HTTP/2, TLS, WebSockets, actors, Tower middleware | **Out of scope.** Put TLS/h2 in a reverse proxy. |

Optional Askama-style HTML codegen is a **separate spec**. The runtime never includes it. An API-only server never sees a template file.

## Constraints (locked)

- Users write `#define STB_TEAPOT_IMPLEMENTATION` and `#include "stb_teapot.h"`. That file is an **amalgam** of `src/`.
- Source of truth is `src/`. `./nob amalgamate` (and default `./nob`, which runs amalgamate first) concatenates into `stb_teapot.h`. Both `src/` and the amalgam are committed. Banner: `/* GENERATED — do not edit. Source: src/ */`.
- Tests, examples, and fuzzing compile **only** the amalgam, never `src/*.c` directly.
- No line-count gates on `src/` or the amalgam. CI: `./nob amalgamate && git diff --exit-code stb_teapot.h` (generated, not hand-edited). Split files by responsibility, not to satisfy a budget.
- HTTP subset unchanged: CRLF, `Content-Length` only, reject `Transfer-Encoding`, reject mismatched duplicate `Content-Length`, reject pipelined leftovers, `Connection: close`.
- No TLS, HTTP/2, HTTP/3, WebSockets, chunked bodies, off-thread handlers, io_uring, or IOCP.
- Windows wait backends are compile-time variants of the same `teapot_run`, not a second product.
- Existing `{ .port, .routes, .route_count }` designated initializers keep compiling. New fields are **appended**.
- The library does **not** install signal handlers and does **not** `printf` (move the listen banner to examples). Keep `perror` on bind/listen failure.
- Keep public `teapot_recv_request` / `teapot_send_response`. Serve path no longer uses `recv_request`.

## Public API (locked)

No `teapot_app`. No `teapot_run_blocking`. `teapot_server` **is** the app.

```c
typedef teapot_response (*teapot_handler)(const teapot_request *);

typedef struct {
    teapot_method method;
    const char *path;
    teapot_handler handler;
    int prefix; /* 1 = prefix match; path must end in '/' or the route never prefix-matches */
} teapot_route;

typedef struct {
    teapot_method method;
    tp_string_builder path;    /* full request-target, including ?query if present */
    tp_string_builder body;
    tp_headers headers;
    size_t body_length;
    void *user;                /* copied from teapot_server.user at dispatch; NULL ok */
} teapot_request;

typedef struct {
    int port;                  /* 0 = ephemeral; listener_open stores the bound port */
    const teapot_route *routes;
    size_t route_count;
    const char *bind_host;     /* NULL = 0.0.0.0; IPv4 dotted decimal only */
    int backlog;               /* 0 = 8 */
    volatile sig_atomic_t stop;
    void *user;                /* app state (axum State / actix web::Data); NULL ok */
    int max_conns;             /* 0 = TEAPOT_MAX_CONNS (128); teapot_run only */
} teapot_server;

void teapot_request_stop(teapot_server *server); /* store 1; signal-safe */

int teapot_listen(teapot_server *server); /* sequential accept loop; 0 = stop, 1 = setup fail */
int teapot_run(teapot_server *server);    /* multiplexed; same return codes */

teapot_response teapot_text(int status, const char *s);
teapot_response teapot_json(int status, const char *json);
teapot_response teapot_bytes(int status, const char *ctype, const void *p, size_t n);

/* existing: listener_open / accept / close, serve_client, handle_client_connection, send_response */
```

`teapot_conn_*`, `teapot_io`, `tp_wait_*`, `tp_now_ms` are **not** documented product API. They exist in `src/` and in the `#ifdef STB_TEAPOT_IMPLEMENTATION` half of the amalgam (tests that `#define STB_TEAPOT_IMPLEMENTATION` may call `teapot_conn_step` if non-static; prefer testing through `teapot_serve_client`). Do not put `teapot_conn` in the public prototypes block.

Canonical usage (JSON API, no HTML):

```c
#define STB_TEAPOT_IMPLEMENTATION
#include "stb_teapot.h"

static teapot_response ping(const teapot_request *req)
{
    (void)req;
    return teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
}

int main(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/ping", ping},
        {TEAPOT_POST, "/echo", echo_handler},
    };
    teapot_server srv = {
        .bind_host = "127.0.0.1",
        .port = 8080,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };
    return teapot_listen(&srv);
}
```

Same `srv` with `teapot_run(&srv)` is the one-process many-connections runtime. Thread-per-client stays `accept` + `teapot_handle_client_connection`.

### Response helpers

Each helper fully initializes a `teapot_response` (status, content_type, empty then filled body) and returns it by value. Safe to call on an uninitialized local. They do **not** parse structured types.

- `teapot_text`: `content_type = "text/plain"`; copy `s` via `strlen`. NULL `s` → empty body.
- `teapot_json`: `content_type = "application/json"`; copy `json` via `strlen`. NULL → empty body. Does not parse, escape, or reject JSON. Newlines in the body are allowed.
- `teapot_bytes`: caller `ctype` (NULL → `"text/plain"`); copy `n` bytes. NULL `p` with `n==0` → empty body.

CR/LF in `content_type` are rejected at **serialize** time (`teapot_send_response` and WRITE_RESP), not in the helper. Keep `test_rejects_response_header_injection`. Add: `teapot_bytes` with ctype `"a\r\nX: b"` → send/step fails, no injected header on the peer.

Keep `teapot_response_init` / `teapot_response_write` for incremental bodies.

### Bind host

- `bind_host == NULL` → `INADDR_ANY`. Preserves today’s `teapot_listen`.
- Else `inet_pton(AF_INET, bind_host, ...)`. `"localhost"` and `""` fail `listener_open` with -1. No `getaddrinfo`.
- Pointer must outlive `teapot_listen` / `teapot_run`.
- Examples and README bind `127.0.0.1`. Do not change the NULL default.
- Keep `AF_INET`, `SO_REUSEADDR`, `teapot_init()` (WSAStartup) in `listener_open`. `teapot_run` calls `listener_open`.
- After successful bind, `getsockname` and store the bound port in `server->port` (so port 0 works for tests).

### Routing

Compare the request-target **up to the first `?`** (if any) against `teapot_route.path`. `req->path` remains the full request-target. No query parser. Prefix match: route `path` must end in `/`; otherwise that route never prefix-matches (no runtime error). Method miss or no path match → 404 `"404 Not Found\n"`, `serve_client` returns 0. No 405 in v1.

### Stop

`teapot_request_stop` stores `1` in `server->stop`. Signal-safe.

`teapot_listen` **must not** block forever in `accept()`. Wait on the listen fd with a portable one-fd primitive: POSIX `poll`, Windows `WSAPoll` (or `select`). Timeout **250 ms**. Re-check `stop` after every wake. `EINTR` → retry. This is **not** `TEAPOT_WAIT`. The amalgam still contains wait shims; `tp_listen.c` does not call them.

After stop, `server->stop` stays `1`. Caller must set it to `0` before a second `listen`/`run`.

Demos (`basic_server.c`, `epoll_server.c`): SIGINT / console ctrl → `teapot_request_stop`. `threaded_server.c` stays `while (1)` (out of this change).

`teapot_run` wait timeout = `min(250 ms, remaining until earliest conn deadline)`. After `stop=1`: do not accept; `del`+close+free all conns; `listener_close`; return 0. Bind or `tp_wait_create` fail → return 1.

## Architecture

```
teapot_server
    ├─ teapot_listen  → poll(listen, 250ms) + handle_client_connection
    └─ teapot_run     → non-blocking fds + wait shim + internal conn_step
```

Three layers, one step function:

1. **HTTP + connection step** — parse, route, serialize. Never blocks on I/O. Handlers run synchronously on that thread (a slow handler stalls `teapot_run`; documented).
2. **Wait shim** — `create` / `add` / `mod` / `del` / `wait` / `destroy`. Selected by `TEAPOT_WAIT`.
3. **One reactor loop** in `src/tp_run.c`. Wait files do not parse HTTP and do not copy this loop.

## Amalgamate (locked)

`nob.c` command `amalgamate`:

1. Emit banner + existing public preamble (include guard, platform typedefs, public structs/prototypes, DA macros, `extern "C"`).
2. `#ifdef STB_TEAPOT_IMPLEMENTATION`
3. Concatenate `src/*.c` in order: `tp_platform.c` (or `.h` helpers), `tp_parse.c` (if split), `tp_http.c`, `tp_conn.c`, `tp_listen.c`, `tp_wait_*.c`, `tp_run.c`.
4. Strip lines that are `#include "tp_*.h"`.
5. Each `tp_wait_*.c` already wraps itself in `#if TEAPOT_WAIT == …`.
6. Close `STB_TEAPOT_IMPLEMENTATION` and the header guard.

Default `./nob` runs amalgamate first, then compiles tests against the amalgam only. CI: `./nob amalgamate && git diff --exit-code stb_teapot.h`.

## Connection step (internal, locked)

Complete struct in `src/tp_conn.c` (visible in the implementation amalgam). Stack-allocatable for `teapot_serve_client` (one conn on the stack). `teapot_run` allocates **one slab** at start: `TP_REALLOC(NULL, max * sizeof(teapot_conn))` where `max = server->max_conns ? server->max_conns : TEAPOT_MAX_CONNS` (default 128). No per-accept malloc of `teapot_conn`.

```c
typedef enum {
    TEAPOT_IO_NEED_READ  = 1,
    TEAPOT_IO_NEED_WRITE = 2,
    TEAPOT_IO_DONE       = 0,
    TEAPOT_IO_ERROR      = -1,
} teapot_io;

typedef enum {
    TEAPOT_CONN_READ_HEAD,
    TEAPOT_CONN_READ_BODY,
    TEAPOT_CONN_WRITE_RESP,
    TEAPOT_CONN_DONE,
} teapot_conn_phase;

#ifndef TEAPOT_CONN_BUF
#define TEAPOT_CONN_BUF 8192
#endif

#ifndef TEAPOT_MAX_CONNS
#define TEAPOT_MAX_CONNS 128
#endif

typedef struct teapot_conn {
    teapot_server *server;
    stb_teapot_socket_t fd;
    teapot_conn_phase phase;
    char in[TEAPOT_CONN_BUF];
    size_t in_len;
    size_t header_end;     /* index of '\r' of the blank line; 0 until seen */
    size_t body_need;
    size_t body_got;
    teapot_request req;
    teapot_response res;
    tp_string_builder out;
    size_t out_sent;
    uint64_t deadline_ms;  /* set at init; does not reset */
    int failed;
    int slot_used;
} teapot_conn;
```

### Clock

```c
static uint64_t tp_now_ms(void);
```

POSIX: `clock_gettime(CLOCK_MONOTONIC)`. Windows: `GetTickCount64()`. Never decreases. `teapot_conn_init` sets `deadline_ms = tp_now_ms() + TEAPOT_RECV_TIMEOUT_MS` (if timeout is 0, no deadline). Idle/deadline does **not** reset on recv/send (Slowloris: one request must finish within one timeout). Handler time is not counted against the deadline (deadline is checked only in READ_HEAD / READ_BODY waits).

### Rules

- **`teapot_conn_step` never blocks on recv/send.** `EAGAIN` / `EWOULDBLOCK` / `WSAEWOULDBLOCK` → `NEED_READ` or `NEED_WRITE`, same phase. The fd **must already be non-blocking**. `step` does not set nonblock.
- **`teapot_run`** sets listen + every accepted client non-blocking before `init`. **`teapot_serve_client`** sets the client non-blocking, then `poll`/`WSAPoll` when `step` returns `NEED_*`.
- **READ_HEAD:** `n = recv(fd, in + in_len, TEAPOT_CONN_BUF - in_len)`. Search the **accumulated** `in[0..in_len]` for `\r\n\r\n` (blank line may split across recvs). If `in_len == TEAPOT_CONN_BUF` with no blank line → framing 400. If `in_len == TEAPOT_CONN_BUF`, do not recv again.
- When blank line seen: parse **once** (same parse rules as today). `extra = in_len - (header_end + 4)`. **If `extra > body_need` → framing 400** (pipelining, including `Content-Length: 0` / missing CL). Else copy `extra` bytes into `req.body` (not into `in[]` for the rest of the body). If `body_need == 0` after that copy: run handler, serialize `out`, `WRITE_RESP`. Else `READ_BODY`.
- **READ_BODY:** `recv` into a stack bounce (4096, same as today) of length `min(remaining, 4096)`, append to `req.body`. `TEAPOT_MAX_BODY_SIZE` cap: `body_need > TEAPOT_MAX_BODY_SIZE` → 400 before reading. Never accumulate the body in `in[]`.
- **Handler** runs synchronously, then serialize into `out` using the same bytes as `teapot_send_response` (status line, Content-Type, Content-Length, `Connection: close`, body). v1 `teapot_response` has no extra header table.
- **`DONE`:** response fully written. Caller closes the fd. `step` never closes.
- **HTTP 400 then flush** (`failed = 1`, `WRITE_RESP`, then `DONE`; `teapot_serve_client` returns **-1**; handler not called): `Transfer-Encoding`; mismatched duplicate `Content-Length`; pipelined leftover (`extra > body_need`); oversize header name/value; no `\r\n\r\n` in `TEAPOT_CONN_BUF`; unknown method / bad request line; `Content-Length` > `TEAPOT_MAX_BODY_SIZE`; **`recv == 0` (including `SHUT_WR`) while headers or body are incomplete**. Wire + `rc` match `tests/unit_test_request.c`.
- **`ERROR`, no further writes** (`failed = 1`; `teapot_serve_client` returns **-1**; handler not called): **deadline exceeded** during READ_HEAD / READ_BODY while the peer is still open; `send` error other than EAGAIN; **`recv < 0` other than EAGAIN / EWOULDBLOCK / WSAEWOULDBLOCK**. **No 400 on the wire.** Matches `tests/unit_test_timeout.c` assertions (`rc` + handler). Change that test’s PASS string from “times out as 400” to “times out without handler”.
- **404** (no route): serialize 404, `failed = 0`, `DONE`, `serve_client` returns 0.

## `teapot_serve_client` / `teapot_listen`

Signatures stay as today: `teapot_serve_client(teapot_server *, stb_teapot_socket_t)` and `teapot_handle_client_connection(teapot_server *, stb_teapot_socket_t)`.

`teapot_serve_client`:

1. Set client non-blocking.
2. Stack `teapot_conn`, `teapot_conn_init`.
3. Loop until DONE/ERROR: `io = teapot_conn_step(c)`.
   - `NEED_READ` / `NEED_WRITE`: wait with POSIX `poll` / Windows `WSAPoll`. **Never use a 0 ms timeout** (that busy-spins). If `TEAPOT_RECV_TIMEOUT_MS == 0`, wait 250 ms and do not apply a deadline. If phase is READ_HEAD or READ_BODY and a deadline exists, wait `min(250 ms, remaining until deadline)`. If phase is WRITE_RESP, wait 250 ms (deadline does **not** abort a 400 flush). After every wake: if `server->stop` → `conn_free`, return -1. If the wait timed out, phase is READ_*, and `tp_now_ms() >= deadline_ms` → `conn_free`, return -1 (no 400).
   - `DONE`: `rc = c->failed ? -1 : 0`.
   - `ERROR`: `rc = -1`.
4. `teapot_conn_free` (does not close fd).

`teapot_handle_client_connection` = serve + close (unchanged).

`teapot_listen`: `listener_open`; loop until `stop`: wait listen 250 ms; on readable `accept` then **`teapot_handle_client_connection`**; then `listener_close`; return 0. Bind fail → 1.

Do not use `SO_RCVTIMEO` on the serve path.

## Wait backends (locked)

Numeric values at the **top** of the public preamble. Users write `#define TEAPOT_USE_WFMO` (etc.) **before** include. Do not document `#define TEAPOT_WAIT 5`.

```c
#define TEAPOT_WAIT_POLL    1
#define TEAPOT_WAIT_EPOLL   2
#define TEAPOT_WAIT_KQUEUE  3
#define TEAPOT_WAIT_WSAPOLL 4
#define TEAPOT_WAIT_WFMO    5

#if defined(TEAPOT_USE_WFMO)
#define TEAPOT_WAIT TEAPOT_WAIT_WFMO
#elif defined(TEAPOT_USE_POLL)
#define TEAPOT_WAIT TEAPOT_WAIT_POLL
#elif defined(TEAPOT_USE_EPOLL)
#define TEAPOT_WAIT TEAPOT_WAIT_EPOLL
#elif defined(TEAPOT_USE_KQUEUE)
#define TEAPOT_WAIT TEAPOT_WAIT_KQUEUE
#elif defined(TEAPOT_USE_WSAPOLL)
#define TEAPOT_WAIT TEAPOT_WAIT_WSAPOLL
#elif !defined(TEAPOT_WAIT)
#  if defined(__linux__)
#    define TEAPOT_WAIT TEAPOT_WAIT_EPOLL
#  elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#    define TEAPOT_WAIT TEAPOT_WAIT_KQUEUE
#  elif defined(_WIN32)
#    define TEAPOT_WAIT TEAPOT_WAIT_WSAPOLL
#  else
#    define TEAPOT_WAIT TEAPOT_WAIT_POLL
#  endif
#endif
```

If the selected backend’s OS headers are missing → `#error "TEAPOT_WAIT backend unknown or unavailable on this OS"`.

### Shim API (internal)

Level-triggered. No `EV_ONESHOT` / `EV_CLEAR` on kqueue. Interest is the last `teapot_io`.

`tp_wait` is a **complete struct in the active wait file**, amalgamated **before** `tp_run.c`. `tp_run.c` stack-allocates `tp_wait w`. Not public API. Each backend may `malloc` internal arrays in `create`.

```c
#define TEAPOT_WAIT_IN  1
#define TEAPOT_WAIT_OUT 2

typedef struct {
    void *udata;
    int events;
} tp_wait_event;

int  tp_wait_create(tp_wait *w);   /* 0 / -1 */
int  tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata);
int  tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata);
int  tp_wait_del(tp_wait *w, stb_teapot_socket_t fd);
int  tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out);
void tp_wait_destroy(tp_wait *w);
```

`add`/`mod`/`del`: `0` success, `-1` failure. **`add` returns -1 when this backend is full** (WFMO at 64 events). epoll/poll/kqueue/WSAPoll do not hit that from the OS cap; they still fail if the reactor slab is full (reactor does not call `add` then).

`wait`: `n >= 0` events, `0` on timeout, `-1` on error. Retry `EINTR` inside wait.

**Reactor (all backends, no `if WFMO`):** listen `udata = NULL`. Client `udata = &slab[i]`. On listen readable, `accept` until `EAGAIN` or no free slab slot. If no free slot: `tp_wait_del(listen)` (disarm). If `accept` succeeds but nonblock or `tp_wait_add` fails: `teapot_close` that client **and** `tp_wait_del(listen)` (treat like slab full; WFMO at 64 events is this path). Do not busy-spin. On client DONE/ERROR: `tp_wait_del`, `teapot_conn_free` fields, `teapot_close(fd)`, mark slot free; if listen was disarmed, `tp_wait_add(listen, IN, NULL)`.

On every `tp_wait_wait` return, **including timeout**: scan used slots whose phase is READ_HEAD or READ_BODY; if a deadline exists and `tp_now_ms() >= deadline_ms`, take the ERROR path (del, free, close). Silent Slowloris clients must not wait for a later readable event.

On `teapot_run` return: del+close all used slots, `tp_wait_destroy`, `listener_close`.

**v1 Linux CI:** default epoll + a second compile `-DTEAPOT_USE_POLL`. kqueue / WSAPoll / WFMO are compile variants, not a Linux CI gate.

**WFMO:** `WSAEventSelect` + `WSAWaitForMultipleEvents`. `add` creates `WSAEVENT`; `del` closes it. After wait, **`WSAEnumNetworkEvents`** on signaled handles (resets the event; without this, busy-loop). Map `FD_ACCEPT`/`FD_READ`/`FD_CLOSE` → `IN`, `FD_WRITE` → `OUT`. `wait` may return 1 event per call. Cap 64 = listen + clients. README documents the cap. Stop uses the 250 ms tick (no wakeup fd).

## Module layout

| File | Responsibility |
| --- | --- |
| `src/tp_platform.h` / `.c` | sockets, `teapot_socket_ok`, close, nonblock, `EAGAIN`, `tp_now_ms` |
| `src/tp_parse.c` | request-line, headers, Content-Length |
| `src/tp_http.c` | routing, serialize, `teapot_text` / `json` / `bytes` |
| `src/tp_conn.c` | conn init / step / free |
| `src/tp_listen.c` | bind/listen/accept; `teapot_listen` |
| `src/tp_run.c` | `teapot_run` reactor (one copy) |
| `src/tp_wait_epoll.c` | epoll shim |
| `src/tp_wait_kqueue.c` | kqueue shim |
| `src/tp_wait_wsapoll.c` | WSAPoll shim |
| `src/tp_wait_wfmo.c` | WFMO shim |
| `src/tp_wait_poll.c` | POSIX poll shim |
| `examples/basic_server.c` | `teapot_listen` JSON/text; SIGINT → stop; bind 127.0.0.1 |
| `examples/epoll_server.c` | same routes, `teapot_run` (or omit and document `teapot_run` in README — prefer one extra example for CI compile) |
| `examples/threaded_server.c` | keep |
| `examples/thread_pool_server_crossplat.c` | keep |

## Tests (TDD)

Existing `unit_test_*` stay the gate.

**First RED (must fail on today’s one-shot recv, not on missing symbols):** split-header `teapot_serve_client` using the same thread + `FIONREAD` drain as `test_split_body_reads_remaining_bytes`. Do **not** write both fragments before `serve_client` (that is already green). Do **not** call `serve_client` on the same thread as the second write (after conn_step it would wait until deadline).

1. socketpair; write `"GET /hello HTTP/1.1\r\n"` only (no blank line, no `SHUT_WR`).
2. Start `teapot_serve_client` on a thread.
3. Wait until `ioctl(FIONREAD)` on the serve fd is 0 (first write drained), 2 s cap.
4. Write `"\r\n"`.
5. Join; assert `rc == 0`, peer has `HTTP/1.1 200` and distinctive body `"pong-body"`.

Today: first recv parses incomplete headers → 400. After accumulation: 200.

Also keep:

- Framing tests on `teapot_serve_client` (chunked 400 + `rc == -1`, pipeline including CL=0, dup CL, incomplete body + `SHUT_WR` still 400).
- Timeout: incomplete body, peer open, `rc == -1`, handler 0; do not assert 400 on the wire.
- `test_listener_open_bind_host_loopback`: `bind_host = "127.0.0.1"`, `getsockname` is `INADDR_LOOPBACK`. `"localhost"` → `listener_open == -1`.
- `test_listen_returns_when_stop_set`: `teapot_request_stop` then `teapot_listen` in a thread; 2 s watchdog; assert return 0. Hang is RED, not a missing symbol.
- `teapot_json` / `teapot_bytes` injection test as above.
- Query: `GET /ping?x=1` matches route `/ping`.
- Reactor smoke (POSIX): `port = 0`, `teapot_run` in a thread, read `server.port` after bind (sleep/poll until `port != 0` or a test-only handshake), one `connect`+GET `/ping`, assert 200, `teapot_request_stop`, join. Linux CI gate.

Fuzzing stays `teapot_serve_client`. Split-header inputs that previously 400 may succeed if they complete before the deadline — intended. rfc_lite header budget: `TEAPOT_CONN_BUF` is **8192** without a forced NUL; update the fuzzing oracle if it assumed 8191.

## Out of scope

- HTTP/2, HTTP/3, TLS, WebSockets
- Chunked bodies, keep-alive pipelining, custom response headers, 405
- Handler worker pool, async handlers, actors, middleware, path params, JSON parser
- io_uring, IOCP
- Fluent builders, `teapot_run_blocking`, HTML escape in `stb_teapot.h`
- Baking templates into the amalgam

## Success criteria

1. A JSON API server is ~20 lines: route table + `teapot_listen`. No template toolchain.
2. The same `teapot_server` runs under `teapot_run` with epoll on Linux.
3. Internal `conn_step` is the only I/O state machine; wait shims do not parse HTTP.
4. Existing unit tests and `./nob fuzz` still pass; framing still 400 + `rc == -1`; incomplete body + EOF still 400; idle timeout still `rc == -1` without requiring 400.
5. `#define TEAPOT_USE_WFMO` is a documented Windows compile variant with a 64-fd cap; not a Linux CI gate.
6. Amalgam is generated; CI fails if `stb_teapot.h` is hand-edited.
