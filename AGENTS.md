# Teapot

A single-header HTTP server library in C (`stb_teapot.h`). Build system is `nob` (see `README.md`).

## Cursor Cloud specific instructions

- Build: `cc nob.c -o nob && ./nob` compiles the test and example binaries into `build/`. This matches CI (`.github/workflows/nob.yml`), which only builds — it does not run the tests.
- The default `cc` on this VM is pointed at `gcc` (via `update-alternatives`). This is required: `nob.c` compiles with `-Werror -Wconversion`, and clang's stricter `-Wsign-conversion` breaks the build. Do not build these with clang.
- `nob` builds but never runs the test binaries. When run manually: `./build/unit_test_headers` has 3 pre-existing failing assertions, and `./build/low_level_test_stb_teapot` starts a blocking server that does not self-exit — run it with a `timeout` if you invoke it. These are existing repo characteristics, not environment issues.
- Run a real HTTP server for a smoke test: `./build/threaded_server` listens on `:8080`. Example: `curl -i -H 'X-Hello: world' http://localhost:8080/hello` returns `200 Hello (X-Hello=world)`.
