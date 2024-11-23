Slang x WebGPU
==============

This is a demo of a possible use of [Slang](https://shader-slang.com/) shader compiler together with [WebGPU](https://www.w3.org/TR/webgpu/) in [C++](https://eliemichel.github.io/LearnWebGPU/) (both in native and Web contexts), using [CMake](https://cmake.org/).

**Features**

- The CMake setup **fetches** Slang compiler and a WebGPU implementation.
- Slang shaders are **compiled into WGSL** upon compilation, with CMake taking care of dependencies.
- Slang introspection API is used to **auto-generate boilerplate** binding code on the C++ side.
- Example of **auto-differentiation** features of Slang are given.
- Build instruction for **native** and **Web** targets.

> [WARNING]
> The WebGPU API is still a Work in Progress at the time I write these lines. **To make sure this setup works**, use the very `webgpu` directory provided in this repository, which fetches the very version of the API for which it was developped (which is [Dawn](https://dawn.googlesource.com/dawn)-specific, BTW).


