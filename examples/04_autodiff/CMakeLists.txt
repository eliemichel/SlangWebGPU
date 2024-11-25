
add_executable(slang_webgpu_example_04_autodiff)
set_example_target_properties(slang_webgpu_example_04_autodiff)

target_sources(slang_webgpu_example_04_autodiff
	PRIVATE
	main.cpp
)

add_slang_webgpu_kernel(
	generate_simple_autodiff_kernel
	NAME SimpleAutodiff
	SOURCE shaders/simple_autodiff.slang
	ENTRY main
)

target_link_libraries(slang_webgpu_example_04_autodiff
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
	generate_simple_autodiff_kernel
)
