module_import
=============

This demo shows how that the source Slang file provided to `add_slang_webgpu_kernel` can include/import other source files, which are correctly added to the compilation dependency graph. It is even possible to specfify extra include directories:

```CMake
add_slang_webgpu_kernel(
	generate_buffer_math_kernel
	NAME BufferMath
	SOURCE shaders/buffer-math.slang
	SLANG_INCLUDE_DIRECTORIES
		other-shaders
	ENTRY
		computeMainAdd
		computeMainSub
		computeMainMultiply
)
```

> [!NOTE]
> Tracking down dependencies of a Slang shader is only enabled for versions of CMake greater or equal to 3.21, because before that not all generators supported the `DEPFILE` option of [`add_custom_command`](https://cmake.org/cmake/help/latest/command/add_custom_command.html).
