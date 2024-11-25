multiple_entrypoints
====================

This demo shows how the same kernel can contain multiple entrypoints that share the same bind group layout. You may have a look at the definition of the `generate_buffer_math_kernel` target in `CMakeLists.txt:`

```CMake
add_slang_webgpu_kernel(
	generate_buffer_math_kernel
	NAME BufferMath
	SOURCE shaders/buffer-math.slang
	ENTRY
		computeMainAdd
		computeMainSub
		computeMainMultiply
		computeMainDivide
		computeMainIdentity
)
```

This will produce a kernel class with multiple `dispatch` methods, where the entrypoint name is appended (and capitalized):

```C++
#include "generated/BufferMathKernel.h"

// (assuming 'device' is a wgpu::Device object)
generated::BufferMathKernel kernel(device);

// (assuming 'buffer0', 'buffer1' and 'result' are wgpu::Buffer objects)
wgpu::BindGroup bindGroup = kernel.createBindGroup(buffer0, buffer1, result);

// The kernel has multiple dispatch methods:
kernel.dispatchComputeMainAdd(ThreadCount{ 10 }, bindGroup);
kernel.dispatchComputeMainSub(ThreadCount{ 10 }, bindGroup);
kernel.dispatchComputeMainMultiply(ThreadCount{ 10 }, bindGroup);
kernel.dispatchComputeMainDivide(ThreadCount{ 10 }, bindGroup);
```
