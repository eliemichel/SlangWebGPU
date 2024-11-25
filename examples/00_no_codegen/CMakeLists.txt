
add_executable(slang_webgpu_example_00_no_codegen)
set_example_target_properties(slang_webgpu_example_00_no_codegen)

target_sources(slang_webgpu_example_00_no_codegen
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_example_00_no_codegen
	PRIVATE
	webgpu
	slang_webgpu_common
	slang_webgpu_example_common
)

# Create a slang shader target to trigger compilation from slang to WGSL
add_slang_shader(
	transpile_add_buffers
	SOURCE shaders/add-buffers.slang
	ENTRY computeMain
)

# Make sure the shader compilation is called whenever we need to build this
# example. This uses 'add_dependencies' rather than 'target_link_libraries'
# because there is nothing to link in the transpilation target.
add_dependencies(
	slang_webgpu_example_00_no_codegen
	transpile_add_buffers
)

# Let our source code know where to find the generated WGSL shader through
# a preprocessor variable. When using emscripten, this also loads the shader
# into the app's data bundle.
# TODO: Add a mode were relative path is used
if (EMSCRIPTEN)
	target_link_options(slang_webgpu_example_00_no_codegen
		PRIVATE
		--preload-file ${CMAKE_CURRENT_BINARY_DIR}/shaders@shaders
	)
	target_compile_definitions(slang_webgpu_example_00_no_codegen
		PRIVATE
		SHADER_DIR="shaders/"
	)
else (EMSCRIPTEN)
	target_compile_definitions(slang_webgpu_example_00_no_codegen
		PRIVATE
		SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders/"
	)
endif (EMSCRIPTEN)
