add_subdirectory(cli11)
add_subdirectory(magic_enum)

if (SLANG_WEBGPU_BUILD_EXAMPLES)
	add_subdirectory(webgpu)
endif()
