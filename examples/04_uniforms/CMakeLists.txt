
add_executable(slang_webgpu_example_04_uniforms)
set_example_target_properties(slang_webgpu_example_04_uniforms)

target_sources(slang_webgpu_example_04_uniforms
	PRIVATE
	main.cpp
)

add_slang_webgpu_kernel(
	generate_buffer_scalar_math_kernel
	NAME BufferScalarMath
	SOURCE shaders/buffer-scalar-math.slang
	ENTRY
		add
		sub
		multiplyAndAdd
)

target_link_libraries(slang_webgpu_example_04_uniforms
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
	generate_buffer_scalar_math_kernel
)
