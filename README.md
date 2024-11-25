<div align="center">
	<picture>
		<source media="(prefers-color-scheme: dark)" srcset="doc/logo-light.svg">
		<source media="(prefers-color-scheme: light)" srcset="doc/logo-dark.svg">
		<img alt="Learn WebGPU Logo" src="doc/logo.svg" width="300">
	</picture>
	<p>
		<a href="https://github.com/eliemichel/LearnWebGPU">LearnWebGPU</a> &nbsp;|&nbsp;
		<a href="https://github.com/eliemichel/WebGPU-Cpp">WebGPU-C++</a> &nbsp;|&nbsp;
		<a href="https://github.com/eliemichel/WebGPU-distribution">WebGPU-distribution</a>
		<br/>
		<a href="https://github.com/eliemichel/glfw3webgpu">glfw3webgpu</a> &nbsp;|&nbsp;
		<a href="https://github.com/eliemichel/sdl2webgpu">sdl2webgpu</a> &nbsp;|&nbsp;
		<a href="https://github.com/eliemichel/sdl3webgpu">sdl3webgpu</a>
	</p>
	<p>
		<a href="https://discord.gg/2Tar4Kt564"><img src="https://img.shields.io/static/v1?label=Discord&message=Join%20us!&color=blue&logo=discord&logoColor=white" alt="Discord | Join us!"/></a>
	</p>
</div>

Slang x WebGPU
==============

This is a demo of a possible use of [Slang](https://shader-slang.com/) shader compiler together with [WebGPU](https://www.w3.org/TR/webgpu/) in [C++](https://eliemichel.github.io/LearnWebGPU/) (both in native and Web contexts), using [CMake](https://cmake.org/).

**Key Features**

- The CMake setup **fetches** Slang compiler and a WebGPU implementation.
- Slang shaders are **compiled into WGSL** upon compilation, with CMake taking care of dependencies.
- Slang reflection API is used to **auto-generate boilerplate** binding code on the C++ side.
- Example of **auto-differentiation** features of Slang are given.
- Build instruction for **native** and **Web** targets.
- Provides a `add_slang_shader` and `target_link_slang_shader` to help managing Slang shader targets (in `cmake/SlangUtils.cmake`).

Outline
-------

- [Notes](#notes)
- [Building](#building)
  * [Compilation of a native app](compilation-of-a-native-app)
  * [Cross-compilation of a WebAssembly module](cross-compilation-of-a-webassembly-module)
- [Going further](#going-further)
- [Limitations](#limitations)
- [Manually reproducing this repo](#manually-reproducing-this-repo)

Notes
-----

> [!WARNING]
> The WebGPU API is still a Work in Progress at the time I write these lines. **To make sure this setup works**, use the very `webgpu` directory provided in this repository, which fetches the very version of the API for which it was developed (which is [Dawn](https://dawn.googlesource.com/dawn)-specific, BTW). When using emscripten, use version **3.1.72**.

> [!NOTE]
> This example relies on the `webgpu.hpp` and `webgpu-raii.hpp` shallow wrapper of `webgpu.h` provided [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution). If this would be a deal-breaker for your use case, you are welcome to **open an issue or a pull request** that addresses this, as there should be no particular blocker to get rid of it.

> 🤔 Ergl, **I don't want codegen**, but I'm interested in the Slang to WGSL part...

Sure, have a look at [`examples/00_no_codegen`](examples/00_no_codegen). All it needs is `cmake/FetchSlang.cmake` and `cmake/SlangUtils.cmake`, but you can strip down the whole codegen part if you don't like it.

Building
--------

This project can be used either in a fully **native** scenario, where the target is an executable, or in a **web** cross-compilation scenario, where the target is a WebAssembly module (and HTML demo page).

### Compilation of a native app

Nothing surprising in this case:

```bash
# Configure the build
cmake -B build

# Compile everything
cmake ---build build
```

> [!NOTE]
> This project uses CMake and tries to bundle as many dependencies as possible. However, it will **fetch at configuration time** the following:
> - A prebuilt version of [Dawn](https://dawn.googlesource.com/), to get a WebGPU implementation ([wgpu-native](https://github.com/gfx-rs/wgpu-native) is a possible alternative, modulo some tweaks).
> - A prebuilt version of [Slang](https://github.com/shader-slang/slang), both executable and library.

You may then explore `build/examples` to execute the various examples.

### Cross-compilation of a WebAssembly module

Cross-compilation and code generation are difficult roommates, but here is how to get them along together: **we create 2 build directories**.

1. We configure a `build-generator` build, where we can disable the examples so that it only builds the generator. Indeed, even if the end target is a WebAssembly module, we still need the generator to build for the host system (where the compilation occurs):

```bash
# Configure a native build, to compile the code generator
# We can turn on examples (or reuse the native build done previously and skip this)
cmake -B build-native -DSLANG_WEBGPU_BUILD_EXAMPLES=OFF
```

> [!NOTE]
> Setting `SLANG_WEBGPU_BUILD_EXAMPLES=OFF` has the nice byproduct of **not fetching Dawn**, because WebGPU is not needed for the generator, and the WebAssembly build has built-in support for WebGPU (so not need for Dawn).

We then build the generator target:

```bash
# Build the generator with the native build
cmake --build build-native --target slang_webgpu_generator
```

2. We configure a `build-web` build with `emcmake` to put in place the cross-compilation to WebAssembly, and this time we **do not** build the generator (`SLANG_WEBGPU_BUILD_GENERATOR=OFF`), but rather tell CMake where to find it with the `SlangWebGPU_Generator_DIR` option:

```bash
# Configure a cross-compilation build
emcmake cmake -B build-web -DSLANG_WEBGPU_BUILD_GENERATOR=OFF -DSlangWebGPU_Generator_DIR=build-native
```

> [!NOTE]
> The `emcmake` command is provided by [emscripten](https://emscripten.org/docs/getting_started/downloads.html). Make sure you **activate your emscripten environment first** (and select preferably version **3.1.72**).

We can now build the WebAssembly module, which will call the generator from `build-native` whenever needed:

```bash
# Build the Web targets
cmake --build build-web
```

And it is now ready to be tested!

```bash
# Start a local server
python -m http.server 8000
```

Then browse for instance to:
- http://localhost:8000/build-web/examples/00_no_codegen/slang_webgpu_example_00_no_codegen.html
- http://localhost:8000/build-web/examples/01_simple_kernel/slang_webgpu_example_01_simple_kernel.html
- http://localhost:8000/build-web/examples/02_multiple_entrypoints/slang_webgpu_example_02_multiple_entrypoints.html
- http://localhost:8000/build-web/examples/03_module_import/slang_webgpu_example_03_module_import.html
- http://localhost:8000/build-web/examples/04_uniforms/slang_webgpu_example_04_uniforms.html
- http://localhost:8000/build-web/examples/05_autodiff/slang_webgpu_example_05_autodiff.html

Going further
-------------

This repository is only meant to be a demo. To go further, start from one of the examples and progressively turn it into something more complex. You may eventually want to move your example into `src/`. You will probably be tempted to tune the generator a bit and add all sorts of fun extensions: do it!

> [!NOTE]
> Examples in the [`examples/`](examples) directory are sorted from the less complex to the most complex.

Limitations
-----------

This is a **proof of concept** more than a fully fledged framework, and it is missing a lot of features that I will probably add progressively, or that you are more than welcome to suggest through a Pull Request:

- Add support for **texture bindings** (shouldn't be too difficult, just a matter of adding it to the generator).
- Auto-generate the `Uniform` struct in each kernel that has global uniform parameters (started already, though a bit boring to write all the cases of nested structs).
- Add support to **local uniform parameters** (only global parameters are handled for now), which may lead to multiple `createBindGroup` methods for the same kernel (not sure).
- Try **more complex scenarios**, for instance the 2D gaussian splatting one of [Slang playground](https://shader-slang.com/slang-playground/).
- Add proper CI workflow to check that everything works as expected.

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

10. Progressively complexify the generator to handle slang shader file inclusion (through a Depfile), to support global uniform buffers, etc.

Internal notes
--------------

```bash
# Memo to look for TODOs:
grep -R TODO --exclude-dir=build* --exclude-dir=third_party
```
