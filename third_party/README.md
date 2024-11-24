Bundled dependencies
====================

- [`webgpu`](https://github.com/eliemichel/WebGPU-distribution): Obvious dependency of this project. This contains CMake scripts that fetch Dawn/wgpu-native precompiled binaries and create a unified `webgpu` target that works with all implementations, including emscripten. This also provides a `webgpu.hpp` and `webgpu-raii.hpp` confort headers to use C++ idioms on top of WebGPU's raw C API.
- [`CLI11`](https://github.com/CLIUtils/CLI11) is used by the generator. We could have used any similar library (e.g., [`argparse`](https://github.com/p-ranav/argparse)), or even hacked something manually but CLI11 is quite leightweight so it is good to have around. Although it is a header only library, I prefer to clearly define it as a separate CMake target to highlight that it is third party work.
