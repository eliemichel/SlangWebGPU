
add_library(slang_webgpu_common STATIC)
set_common_target_properties(slang_webgpu_common)

target_include_directories(slang_webgpu_common PUBLIC include)

target_sources(slang_webgpu_common
	PRIVATE
	src/lib.cpp
)
