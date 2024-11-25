simple_kernel
=============

This demo is the "hello world" of our SlangWebGPU setup, including both Slang -> WGSL conversion and automated C++ kernel binding code generation to reduce the need to write boilerplate code.

The generation and compilation of this boilerplate code is managed by a `add_slang_webgpu_kernel` function, defined in `cmake/SlangUtils.cmake`, which creates a target `generate_hello_world_kernel` that your application can link to.

```CMake
add_slang_webgpu_kernel(
	generate_hello_world_kernel
	NAME HelloWorld
	SOURCE shaders/hello-world.slang
	ENTRY computeMain
)
```

You can then use the generated kernel as follows:

```C++
#include "generated/HelloWorldKernel.h"

// (assuming 'device' is a wgpu::Device object)
generated::HelloWorldKernel kernel(device);

// (assuming 'buffer0', 'buffer1' and 'result' are wgpu::Buffer objects)
wgpu::BindGroup bindGroup = kernel.createBindGroup(buffer0, buffer1, result);

// First argument can be ThreadCount{ ... } or WorkgroupCount{ ... }
kernel.dispatch(ThreadCount{ 10 }, bindGroup);
```
