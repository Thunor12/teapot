# Teapot
A simple HTTP server in a single C/C++ header file.

## Build and test

This repo vendors [nob.h](https://github.com/tsoding/nob.h) as a git submodule. Clone with `--recurse-submodules`, or after a plain clone run `git submodule update --init`.

```sh
git submodule update --init
cc nob.c -o nob && ./nob
```

On Windows, run `./nob.exe` instead of `./nob`.

`./nob` compiles examples into `build/` and runs `unit_test_headers`, `unit_test_request`, and `unit_test_response`. A failing unit test fails the build. If `valgrind` is installed, unit tests run under `--leak-check=full`.

Do not run `build/basic_server` (or the other example servers) from `./nob` — they are blocking listeners. Use `timeout` if you start one by hand.

## Socket ownership

`teapot_serve_client` does not close the client socket.
`teapot_handle_client_connection` takes ownership and closes it.
Do not close the same fd again.

## HTTP subset

Request line + headers + `Content-Length` body. No header folding, no chunked encoding, no pipelining. Oversize headers and bodies are 400.

Clang `-Wsign-conversion` is not compatible with this project's `-Werror` flags; `gcc` is the CI compiler.

## Features
- Single header file: `stb_teapot.h`
- Lightweight and easy to integrate into existing projects
- Supports basic HTTP functionalities
- Cross-platform compatibility (Windows, Linux, macOS)
- Minimal dependencies

## Usage
Include `stb_teapot.h` after `#define STB_TEAPOT_IMPLEMENTATION`. See `examples/basic_server.c` for a blocking listener, `examples/threaded_server.c` for one thread per client, and `examples/demo_handlers.h` for shared hello/echo handlers.

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your changes.
For major changes, please open an issue first to discuss what you would like to change.

