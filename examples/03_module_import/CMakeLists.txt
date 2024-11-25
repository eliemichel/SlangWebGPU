
add_executable(slang_webgpu_example_03_module_import)
set_example_target_properties(slang_webgpu_example_03_module_import)

target_sources(slang_webgpu_example_03_module_import
	PRIVATE
	main.cpp
)

add_slang_webgpu_kernel(
	generate_split_buffer_math_kernel
	NAME BufferMath
	SOURCE shaders/buffer-math.slang
	SLANG_INCLUDE_DIRECTORIES
		other-shaders
	ENTRY
		computeMainAdd
		computeMainSub
		computeMainMultiply
)

target_link_libraries(slang_webgpu_example_03_module_import
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
	generate_split_buffer_math_kernel
)
