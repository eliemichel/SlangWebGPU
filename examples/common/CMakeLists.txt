
add_library(slang_webgpu_example_common STATIC)
set_common_target_properties(slang_webgpu_example_common)

target_include_directories(slang_webgpu_example_common PUBLIC include)

set(INCLUDE_DIR include/slang-webgpu/examples)
target_sources(slang_webgpu_example_common
	PRIVATE
	${INCLUDE_DIR}/webgpu-utils.h
	src/webgpu-utils.cpp
)

target_link_libraries(slang_webgpu_example_common
	PUBLIC
	webgpu
	slang_webgpu_common
)

set_target_properties(slang_webgpu_example_common
	PROPERTIES
	FOLDER "SlangWebGPU/examples"
)
