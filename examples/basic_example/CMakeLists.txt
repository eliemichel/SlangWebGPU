
add_executable(slang_webgpu_example)
set_example_target_properties(slang_webgpu_example)

target_sources(slang_webgpu_example
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_example
	PRIVATE
	webgpu
	slang_webgpu_common
)

add_slang_shader(
	hello_world
	SOURCE shaders/hello-world.slang
	ENTRY computeMain
)

target_link_slang_shaders(slang_webgpu_example
	hello_world
)

add_slang_webgpu_kernel(
	generate_hello_world_kernel
	NAME HelloWorld
	SOURCE shaders/hello-world.slang
	ENTRY computeMain
)

target_link_libraries(slang_webgpu_example
	PRIVATE
	generate_hello_world_kernel
)
