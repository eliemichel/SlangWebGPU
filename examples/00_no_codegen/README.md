no_codegen
==========

This demo shows how to use the Slang -> WGSL conversion setup **without any sort of code generation**. It is much **less invasive** for the build setup, at the cost of **requiring manual setup** of pipelines and bind groups.

The only custom element in the CMake configuration is the `add_slang_shader` function, defined in `cmake/SlangUtils.cmake`, used like this:

```CMake
add_slang_shader(
	add_buffers
	SOURCE shaders/add-buffers.slang
	ENTRY computeMain
)
```

This creates a custom target `add_buffers` that you can add to your dependency graph to make sure the shader transpilation (i.e., the call to `slangc` executable) is triggered when needed.
