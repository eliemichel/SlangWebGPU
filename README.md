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
> The WebGPU API is still a Work in Progress at the time I write these lines. **To make sure this setup works**, use the very `webgpu` directory provided in this repository, which fetches the very version of the API for which it was developed (which is [Dawn](https://dawn.googlesource.com/dawn)-specific, BTW).


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

5. As we notice that a large part of the code to create a kernel (layout and bind groups) is redundant with binding information provided by the shader, we use **Slang reflection API** to generate some code. We thus create `src/generator`, using [Slang Playground](https://shader-slang.com/slang-playground/) to explore the reflection API. For this, we add [`argparse`](https://github.com/p-ranav/argparse) the `third_party` directory.
