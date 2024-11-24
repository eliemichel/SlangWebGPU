
add_executable(slang_webgpu_generator)
set_common_target_properties(slang_webgpu_generator)

target_sources(slang_webgpu_generator
	PRIVATE
	main.cpp
)

target_link_libraries(slang_webgpu_generator
	PRIVATE
	slang_webgpu_common
	argparse
)
