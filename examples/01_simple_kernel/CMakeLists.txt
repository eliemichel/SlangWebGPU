
add_executable(slang_webgpu_example_01_simple_kernel)
set_example_target_properties(slang_webgpu_example_01_simple_kernel)

target_sources(slang_webgpu_example_01_simple_kernel
	PRIVATE
	main.cpp
)

add_slang_webgpu_kernel(
	generate_hello_world_kernel
	NAME HelloWorld
	SOURCE shaders/hello-world.slang
	ENTRY computeMain
)

target_link_libraries(slang_webgpu_example_01_simple_kernel
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
	generate_hello_world_kernel
)
