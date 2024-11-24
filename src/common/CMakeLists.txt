
add_library(slang_webgpu_common STATIC)
set_common_target_properties(slang_webgpu_common)

target_include_directories(slang_webgpu_common PUBLIC include)

set(INCLUDE_DIR include/slang-webgpu/common)

target_sources(slang_webgpu_common
	PRIVATE
	${INCLUDE_DIR}/result.h
	${INCLUDE_DIR}/logger.h
	${INCLUDE_DIR}/io.h
	${INCLUDE_DIR}/kernel-utils.h
	${INCLUDE_DIR}/variant-utils.h
	src/io.cpp
)
