Slang x WebGPU
==============

This is a demo of a possible use of [Slang](https://shader-slang.com/) shader compiler together with [WebGPU](https://www.w3.org/TR/webgpu/) in [C++](https://eliemichel.github.io/LearnWebGPU/) (both in native and Web contexts), using [CMake](https://cmake.org/).

**Features**

- The CMake setup **fetches** Slang compiler and a WebGPU implementation.
- Slang shaders are **compiled into WGSL** upon compilation, with CMake taking care of dependencies.
- Slang reflection API is used to **auto-generate boilerplate** binding code on the C++ side.
- Example of **auto-differentiation** features of Slang are given.
- Build instruction for **native** and **Web** targets.
- Provides a `add_slang_shader` and `target_link_slang_shader` to help managing Slang shader targets (in `cmake/SlangUtils.cmake`).

> [!WARNING]
> The WebGPU API is still a Work in Progress at the time I write these lines. **To make sure this setup works**, use the very `webgpu` directory provided in this repository, which fetches the very version of the API for which it was developed (which is [Dawn](https://dawn.googlesource.com/dawn)-specific, BTW). When using emscripten, use version **3.1.72**.

> [!NOTE]
> This example relies on the `webgpu.hpp` and `webgpu-raii.hpp` shallow wrapper of `webgpu.h` provided [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution). If this would be a deal-breaker for your use case, you are welcome to **open an issue or a pull request** that addresses this, as there should be no particular blocker to get rid of it.

> [!NOTE]
> Examples in the [`examples/`](examples) directory are sorted from the less complex to the most complex.

Building
--------

```bash
# In /src/SlangWebGPU
cmake -B build-generator -DSLANG_WEBGPU_BUILD_EXAMPLES=OFF
emcmake cmake -B build-web -DSLANG_WEBGPU_BUILD_GENERATOR=OFF -DSlangWebGPU_Generator_DIR=/src/SlangWebGPU/build-generator
cmake --build build-web
python -m http.server 8000
# Browse to:
#  - http://localhost:8000/build-web/examples/00_no_codegen/slang_webgpu_example_no_codegen.html
#  - http://localhost:8000/build-web/examples/01_basic_example/slang_webgpu_example.html
#  - http://localhost:8000/build-web/examples/02_multiple_entrypoints/slang_webgpu_example_multiple_entrypoints.html
```

Manually reproducing this repo
------------------------------

Although this repo can be reused as is, it may be of interest to know how it was originally built, so here is **a rough log of the steps I followed** during its construction:

1. Getting WebGPU: I get from [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) a CMake setup that fetches a precompiled version. I copied the content of the `fetch` branch (soon to become `main`) except `.github` to `third_party/webgpu` and added a fatal error in case one tries to use `wgpu-native` because I will focus on Dawn for now.

2. Getting Slang: I write `cmake/FetchSlang.cmake` to fetch precompiled Slang from [official releases](https://github.com/shader-slang/slang/releases), freezing to `v2024.14.5` to ensure this repo doesn't break over time.

3. Write `cmake/SlangUtils.cmake` to trigger slang compiler from CMake build setup.

4. Write a basic `Kernel` class that contains everything needed to dispatch a compute job (for instance getting inspiration from [gpu.cpp](https://github.com/AnswerDotAI/gpu.cpp)):

```C++
struct Kernel {
	std::string name;
	std::vector<raii::BindGroupLayout> bindGroupLayouts;
	raii::ComputePipeline pipeline;
};
```

> [!NOTE]
> If you are interested in an example that only calls `slangc` to compile Slang shaders into WGSL but **does no code generation**, you may have a look at [the `no_codegen` example](examples/no_codegen).

5. As we notice that a large part of the code to create a kernel (layout and bind groups) is redundant with binding information provided by the shader, we use **Slang reflection API** to generate some code. We thus create `src/generator`, using [Slang Playground](https://shader-slang.com/slang-playground/) to explore the reflection API. For this, we add [`CLI11`](https://github.com/CLIUtils/CLI11) to the `third_party` directory.

6. To write the generator, follow instructions from [Slang Compilation API documentation](https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api), then [Slang Reflection API documentation](https://shader-slang.com/slang/user-guide/reflection.html).

7. Create a very basic template system so that the generated file can be drafter in `src/generator/binding-template.tpl`. Adding [`magic_enum`](https://github.com/Neargye/magic_enum) to the `third_party` directory to produce nicer error messages.

> [!WARNING]
> Our binding generator does not handle every single one of the many cases of bindings that Slang supports. To add extra mechanism, go have a look at `BindingGenerator` in `src/generator/main.cpp`.

8. Split the CMake build into two stages to be able to separately build the generator and the examples when cross-compiling to WebAssembly (because the generator must be built for the host system, rather than for the target system). Following instructions from [CMake documentation](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html#running-executables-built-in-the-project) about cross-compilation.

9. Generalize to cases where we want to expose **multiple entry points** from the same shader, hence implementing multiple `dispatch` methods.

10. Progressively complexify the generator to handle slang shader file inclusion (through a Depfile).
TODO: autodiff.
TODO: custom entry point name
TODO: uniforms
TODO: texture
TODO: backport emscripten changes to webgpu distrib

Internal notes
--------------

```bash
# Memo to look for TODOs:
grep -R TODO --exclude-dir=build* --exclude-dir=third_party
```

TODO: why does the generator crash?
