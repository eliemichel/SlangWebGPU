
add_executable(slang_webgpu_example_no_codegen)
set_example_target_properties(slang_webgpu_example_no_codegen)

target_sources(slang_webgpu_example_no_codegen
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_example_no_codegen
	PUBLIC
	webgpu
	slang_webgpu_common
)

# Create a slang shader target to trigger compilation from slang to WGSL
add_slang_shader(
	add_buffers
	SOURCE shaders/add-buffers.slang
	ENTRY computeMain
)

# Make sure the shader compilation is called whenever we need to build this
# example
target_link_slang_shaders(slang_webgpu_example_no_codegen
	add_buffers
)

# Let our source code know where to find the generated WGSL shader through
# a preprocessor variable
target_compile_definitions(slang_webgpu_example_no_codegen
	PRIVATE
	SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders/"
)
