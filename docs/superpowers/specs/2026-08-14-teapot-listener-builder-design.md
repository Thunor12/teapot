# Teapot Listener / Runtime Builder Design

**Date:** 2026-08-14  
**Status:** draft for review  
**Goal:** One API to build either a blocking server or a multiplexed non-blocking server (epoll / kqueue / WSAPoll / WFMO), without putting HTTP/2 in the library.

## Constraints (locked)

- Users still write `#define STB_TEAPOT_IMPLEMENTATION` and `#include "stb_teapot.h"`. That file is an **amalgam** of smaller modules.
- Source of truth is `src/` (multiple `.c` / `.h` files). `scripts/amalgamate` concatenates them into `stb_teapot.h`. Both `src/` and the amalgam are committed.
- The old “`stb_teapot.h` < 1000 lines” gate is replaced by: **each `src/` file < 400 lines**; amalgam **< 2500 lines**. CI checks both.
- HTTP subset is unchanged: `Content-Length` only, reject `Transfer-Encoding`, reject mismatched duplicate `Content-Length`, reject pipelined leftovers, `Connection: close`, recv timeout on blocking sockets.
- No TLS, HTTP/2, HTTP/3, chunked bodies, or off-thread handlers in this spec.
- Windows wait backends are compile-time variants, not a second product.

## Architecture

Three layers, one step function:

1. **HTTP + connection step** — parse, route, serialize. `teapot_conn_step` never blocks. Returns `NEED_READ`, `NEED_WRITE`, `DONE`, or `ERROR`.
2. **Wait backend** — selected by `TEAPOT_WAIT`. Turns those wants into `epoll_wait` / `kevent` / `WSAPoll` / `WaitForMultipleObjects` / `poll`.
3. **App builder** — bind + routes + `teapot_run_blocking` or `teapot_run`.

```
teapot_app_bind / teapot_app_routes
        │
        ├─ teapot_run_blocking  → blocking sockets, loop conn_step
        └─ teapot_run           → non-blocking sockets, TEAPOT_WAIT reactor
                                      │
                                      ├─ TEAPOT_WAIT_EPOLL    (Linux default)
                                      ├─ TEAPOT_WAIT_KQUEUE   (BSD/mac default)
                                      ├─ TEAPOT_WAIT_WSAPOLL  (Windows default)
                                      ├─ TEAPOT_WAIT_WFMO     (Windows, 64-fd cap)
                                      └─ TEAPOT_WAIT_POLL     (portable fallback)
```

`teapot_serve_client` becomes a blocking loop around `teapot_conn_step`. Existing examples keep compiling.

## Module layout

| File | Responsibility |
| --- | --- |
| `src/tp_platform.h` | socket typedefs, `teapot_socket_ok`, close, `EAGAIN` mapping |
| `src/tp_http.c` | headers, parse, routing, response serialize (today’s HTTP core) |
| `src/tp_conn.c` | `teapot_conn`, `teapot_conn_init` / `step` / `free` |
| `src/tp_listen.c` | bind/listen/accept; host + port + backlog |
| `src/tp_app.c` | `teapot_app`, `teapot_run_blocking`, `teapot_run` dispatch |
| `src/tp_wait_epoll.c` | Linux epoll reactor |
| `src/tp_wait_kqueue.c` | kqueue reactor |
| `src/tp_wait_wsapoll.c` | Windows `WSAPoll` |
| `src/tp_wait_wfmo.c` | Windows `WSAEventSelect` + `WSAWaitForMultipleEvents` |
| `src/tp_wait_poll.c` | POSIX `poll` |
| `scripts/amalgamate.py` | emit `stb_teapot.h` |
| `examples/basic_server.c` | `teapot_run_blocking` |
| `examples/epoll_server.c` | `teapot_run` (Linux) |
| `examples/threaded_server.c` | keep; uses `teapot_listener_accept` + `teapot_handle_client_connection` |
| `examples/thread_pool_server_crossplat.c` | keep as a third recipe |

Amalgamation order: platform, http, conn, listen, wait_* (each behind its `#if TEAPOT_WAIT == ...`), app. Unused wait backends are not compiled when the user sets `TEAPOT_WAIT` (ifdef in each wait file).

## Connection step

```c
typedef enum {
    TEAPOT_IO_NEED_READ = 1,
    TEAPOT_IO_NEED_WRITE = 2,
    TEAPOT_IO_DONE = 0,
    TEAPOT_IO_ERROR = -1,
} teapot_io;

typedef struct teapot_conn teapot_conn;

teapot_io teapot_conn_init(teapot_conn *c, teapot_server *server, stb_teapot_socket_t fd);
teapot_io teapot_conn_step(teapot_conn *c);
void teapot_conn_free(teapot_conn *c);
```

Internal phases: `READ_HEAD` → `READ_BODY` → `WRITE_RESP` → `DONE`.

- `teapot_conn_step` uses `recv`/`send` (or `teapot_read`/`teapot_write`) **without** waiting. `EAGAIN` / `EWOULDBLOCK` / `WSAEWOULDBLOCK` → return `NEED_READ` or `NEED_WRITE` without changing phase.
- Headers + framing use the same rules as today (8 KiB first buffer, TE, dup CL, leftover bytes).
- When the request is complete, the route handler runs **synchronously** on that thread, then the response is serialized into an out-buffer and phase becomes `WRITE_RESP`.
- `DONE` means the response is fully written. The **caller** closes the fd (`teapot_run*` does this). `teapot_conn_step` does not close the socket (same ownership as `teapot_serve_client`).
- `ERROR` means the connection should be dropped. Optional: if a 400 was serialized, phase may still be `WRITE_RESP` until that is flushed; then `DONE`. Spec: parse/framing failures serialize 400, `NEED_WRITE`, then `DONE`. Hard I/O failures (`recv == 0` with incomplete request, `send` error other than EAGAIN) → `ERROR` and no further writes.
- `teapot_conn_free` frees request/response/out buffers; does not close fd.

`teapot_serve_client`:

1. `teapot_conn_init`
2. loop: if `NEED_READ`, blocking `recv` into the conn (or leave fd blocking so `step`’s recv blocks — **must not**: step is defined as non-blocking). Blocking run sets the fd **blocking** and, when `step` returns `NEED_READ`/`NEED_WRITE`, performs one blocking `recv`/`send` helper that fills/flushes, then `step` again. Cleaner: blocking run uses `poll` on one fd with `TEAPOT_RECV_TIMEOUT_MS`, then `step`. That unifies timeout with the reactor.
3. `teapot_conn_free`

**Timeout:** `TEAPOT_RECV_TIMEOUT_MS` applies to **wait** (poll/epoll timeout or `SO_RCVTIMEO` in the single-fd blocking helper), not to the handler. A slow handler is not interrupted.

## App builder

```c
typedef struct {
    teapot_server server;   /* bind_host, port, backlog, routes, route_count */
    volatile int stop;      /* teapot_app_request_stop */
} teapot_app;

void teapot_app_init(teapot_app *app);
void teapot_app_bind(teapot_app *app, const char *host, int port);
void teapot_app_routes(teapot_app *app, const teapot_route *routes, size_t n);
void teapot_app_request_stop(teapot_app *app);

int teapot_run_blocking(teapot_app *app);
int teapot_run(teapot_app *app);
```

- `teapot_app_init` zeroes the struct. `teapot_app_bind` sets `server.bind_host` (NULL → `INADDR_ANY`) and `server.port`. Backlog stays 8 unless the caller sets `server.backlog` (0 means 8).
- `teapot_server` gains `const char *bind_host` and `int backlog`. `teapot_listener_open` uses them. Existing `{ .port = 8080, .routes = ... }` still works (NULL host, 0 backlog).
- `teapot_run_blocking`: open listen socket, accept loop, per client `teapot_serve_client` (conn_step loop), until `app->stop`.
- `teapot_run`: open listen socket, set non-blocking, register listen fd with the wait backend, accept on listen-read, `teapot_conn_init` + register client, `step` on events, close on `DONE`/`ERROR`, until `app->stop`.
- Demos install SIGINT / console ctrl → `teapot_app_request_stop`.

`teapot_listen(teapot_server *)` remains: wrap a stack `teapot_app` with that server’s port/routes and call `teapot_run_blocking`.

## TEAPOT_WAIT

```c
#define TEAPOT_WAIT_POLL    1
#define TEAPOT_WAIT_EPOLL   2
#define TEAPOT_WAIT_KQUEUE  3
#define TEAPOT_WAIT_WSAPOLL 4
#define TEAPOT_WAIT_WFMO    5
```

Default if the user does not define `TEAPOT_WAIT`:

- Linux: `TEAPOT_WAIT_EPOLL`
- Apple / FreeBSD / OpenBSD / NetBSD: `TEAPOT_WAIT_KQUEUE`
- Windows: `TEAPOT_WAIT_WSAPOLL`
- else: `TEAPOT_WAIT_POLL`

User override: `#define TEAPOT_WAIT TEAPOT_WAIT_WFMO` before the include.

If `teapot_run` is compiled with a backend that does not exist on this OS, compilation fails with `#error`. `teapot_run_blocking` never needs a wait backend.

**WFMO:** `WSAEventSelect` + `WSAWaitForMultipleEvents`. Maximum **64** events including the listen socket. If accept would exceed 63 clients, drop the new client (close fd) and continue; do not abort the process. Document this cap in README.

**IOCP** is not in v1. The `TEAPOT_WAIT` numbering leaves room (`6`) for it later.

## Examples

- `basic_server.c` — `teapot_app_bind(..., 8080)` + `teapot_run_blocking`.
- `epoll_server.c` — same bind/routes, `teapot_run` (Linux CI can compile this).
- `threaded_server.c` / `thread_pool_server_crossplat.c` — still valid compositions of `accept` + `handle_client_connection`; not removed.

## Tests

- Existing unit tests stay on `teapot_serve_client` / parse / send.
- New: `unit_test_conn_step.c` — socketpair, non-blocking optional; feed a full GET, `step` until `DONE`; chunked still 400; pipeline leftover 400.
- New: `unit_test_conn_partial.c` — write headers without body, `step` → `NEED_READ`; write rest; `DONE`.
- Reactor tests: optional POSIX-only smoke that binds port 0, `teapot_run` in a thread with stop, one `connect`+GET, assert 200. Skip on Windows CI if too brittle; blocking tests remain the gate.

## Out of scope

- HTTP/2, HTTP/3, TLS
- Chunked request bodies
- Running handlers on a worker pool while the reactor thread only does I/O (async handlers)
- io_uring, IOCP
- Raising the WFMO 64 cap
- Fluent chained builders (`bind(routes(run()))`)

## Success criteria

1. `#include "stb_teapot.h"` still builds a blocking server in ~15 lines.
2. The same routes struct runs under `teapot_run` with epoll on Linux.
3. `#define TEAPOT_WAIT TEAPOT_WAIT_WFMO` compiles a Windows multiplexed server with a documented 64-fd cap.
4. `teapot_conn_step` is the only I/O state machine; wait backends do not parse HTTP.
5. Amalgam is generated, not hand-edited. CI fails if `stb_teapot.h` is not the amalgam output.
