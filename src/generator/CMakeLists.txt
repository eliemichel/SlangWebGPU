
add_executable(slang_webgpu_generator)
set_common_target_properties(slang_webgpu_generator)

target_sources(slang_webgpu_generator
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_generator
	PRIVATE
	slang
	slang_webgpu_common
	CLI11
	magic_enum
)

target_copy_slang_binaries(slang_webgpu_generator)
