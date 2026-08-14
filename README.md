# Teapot
A simple HTTP server in a single C/C++ header file.

## Build Tests and Examples

This repo vendors [nob.h](https://github.com/tsoding/nob.h) as a git submodule. Clone with `--recurse-submodules`, or after a plain clone run `git submodule update --init`.

### Linux / macOS
```sh
git submodule update --init
cc nob.c -o nob && ./nob
```

### Windows
```sh
git submodule update --init
cc nob.c -o nob && ./nob.exe
```

`./nob` compiles the examples into `build/` and runs `unit_test_headers`. A failing unit test fails the build.

## Features
- Single header file: `stb_teapot.h`
- Lightweight and easy to integrate into existing projects
- Supports basic HTTP functionalities
- Cross-platform compatibility (Windows, Linux, macOS)
- Minimal dependencies

## Usage
Include the `stb_teapot.h` header file in your project and use the provided functions to set up and run the HTTP server. Refer to the comments in the header file for detailed usage instructions.

## License
This project is licensed under the MIT License. See the LICENSE file for details.

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your changes.
For major changes, please open an issue first to discuss what you would like to change.

