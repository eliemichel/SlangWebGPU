

add_slang_shader(
	hello_world
	SOURCE shaders/hello-world.slang
	ENTRY computeMain
)

add_executable(slang_webgpu_example_no_codegen)
set_example_target_properties(slang_webgpu_example_no_codegen)

target_sources(slang_webgpu_example_no_codegen
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_example_no_codegen
	PUBLIC
	webgpu
	slang_static
	slang_webgpu_common
)

target_link_slang_shaders(slang_webgpu_example_no_codegen
	hello_world
)

