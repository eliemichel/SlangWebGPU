
add_executable(slang_webgpu_example_multiple_entrypoints)
set_example_target_properties(slang_webgpu_example_multiple_entrypoints)

target_sources(slang_webgpu_example_multiple_entrypoints
	PRIVATE
	main.cpp
)

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

target_link_libraries(slang_webgpu_example_multiple_entrypoints
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
	generate_buffer_math_kernel
)
