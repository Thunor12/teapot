# Teapot
A single-header C HTTP/1.1 application server: route a request, return JSON, bytes, or text. HTML is one content type, not the product.

## Quick start

`GET /ping` with `teapot_json` and a loopback bind:

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
    };
    teapot_server server = {
        .port = 8080,
        .routes = routes,
        .route_count = 1,
        .bind_host = "127.0.0.1",
    };
    return teapot_listen(&server);
}
```

`curl http://127.0.0.1:8080/ping` → `{"ok":true}`.

Also `teapot_text` and `teapot_bytes`. Full example: `examples/basic_server.c`.

## `teapot_listen` vs `teapot_run`

| | `teapot_listen` | `teapot_run` |
| --- | --- | --- |
| Concurrency | One client at a time | Many clients on one thread |
| Typical use | Small tools, demos | Services that must overlap I/O |
| Example | `examples/basic_server.c` | `examples/epoll_server.c` |

Both bind, accept, and dispatch the same `teapot_route` table. Stop with `teapot_request_stop`. For one thread per client, open the listener yourself (`examples/threaded_server.c`).

## Bind address

Set `bind_host` to `"127.0.0.1"` for loopback (what the examples do). `NULL` binds `0.0.0.0` (all IPv4 interfaces). The value is a dotted IPv4 address, not a hostname: `"localhost"` fails.

## TLS, HTTP/2, templates

TLS and HTTP/2 belong in a reverse proxy, not in this header. HTML templates are optional and **not this release**.

## Build and test

This repo vendors [nob.h](https://github.com/tsoding/nob.h) as a git submodule. Clone with `--recurse-submodules`, or after a plain clone run `git submodule update --init`.

```sh
git submodule update --init
cc nob.c -o nob && ./nob
```

On Windows, run `./nob.exe` instead of `./nob`.

`./nob` amalgamates `src/` into `stb_teapot.h`, compiles examples into `build/`, and runs the unit tests. A failing unit test fails the build. If `valgrind` is installed, unit tests run under `--leak-check=full`.

Fuzzing is a separate clang artifact. `./nob fuzz` requires `clang`, builds `build/fuzz_serve` with `-fsanitize=fuzzer,address,undefined`, and smokes libFuzzer for 30 seconds against a copy of `tests/fuzz_corpus` (so the checked-in seeds stay clean). CI sets `TEAPOT_FUZZ_GRAMMAR=1` so rfc_lite-valid HTTP that teapot 400s is a failure.

```sh
./nob fuzz
./nob fuzz -max_total_time=0   # local campaign, no time cap
```

Or compile the harness yourself (copy the seeds first if you do not want libFuzzer writing into git):

```sh
mkdir -p build/fuzz_corpus
cp -a tests/fuzz_corpus/. build/fuzz_corpus/
clang -O1 -g -std=c17 -fsanitize=fuzzer,address,undefined \
  -o build/fuzz_serve tests/fuzz_serve.c
./build/fuzz_serve build/fuzz_corpus -max_total_time=30 -timeout=2
```

Do not run `build/basic_server` (or the other example servers) from `./nob` — they are blocking listeners. Use `timeout` if you start one by hand.

## Socket ownership

`teapot_serve_client` does not close the client socket.
`teapot_handle_client_connection` takes ownership and closes it.
Do not close the same fd again.

## HTTP subset

Request line + headers + `Content-Length` body. No header folding, no chunked encoding, no pipelining. `Transfer-Encoding` is 400. Duplicate `Content-Length` is 400 unless the values parse to the same number. Extra bytes after the body in the first read are 400. Oversize headers and bodies are 400. Client sockets get a recv timeout (`TEAPOT_RECV_TIMEOUT_MS`, default 5000). Responses include `Connection: close`.

Clang `-Wsign-conversion` is not compatible with this project's `-Werror` flags; `gcc` is the CI compiler.

## Wait backends (`teapot_run`)

`teapot_run` multiplexes connections with a compile-time wait backend. Define **one** of these **before** including `stb_teapot.h` (do not set `TEAPOT_WAIT` by number):

| Macro | Backend | Default on |
| --- | --- | --- |
| `TEAPOT_USE_EPOLL` | Linux epoll | Linux |
| `TEAPOT_USE_POLL` | POSIX `poll` | other Unix |
| `TEAPOT_USE_KQUEUE` | kqueue | macOS / BSD |
| `TEAPOT_USE_WSAPOLL` | `WSAPoll` | Windows |
| `TEAPOT_USE_WFMO` | `WSAEventSelect` + `WSAWaitForMultipleEvents` | (opt-in) |

Example: `#define TEAPOT_USE_POLL` then `#include "stb_teapot.h"`.

**WFMO cap:** `TEAPOT_USE_WFMO` can watch at most **64** sockets at once (listen socket plus clients). Past that, teapot stops accepting until a connection finishes. Prefer `TEAPOT_USE_WSAPOLL` when you need more concurrent connections on Windows.

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your changes.
For major changes, please open an issue first to discuss what you would like to change.
