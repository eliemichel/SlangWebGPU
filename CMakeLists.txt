cmake_minimum_required(VERSION 3.16...3.30 FATAL_ERROR)
project(
	SlangWebGPU
	VERSION 1.0.0
	LANGUAGES CXX C
)

# To add some missing features for older versions of cmake
include(cmake/CMakePolyfill.cmake)

#############################################
# Options

option(SLANG_WEBGPU_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

#############################################
# Utils

# Provides 'add_slang_shader' and 'target_link_slang_shaders'
include(cmake/SlangUtils.cmake)

# Provides 'set_common_target_properties'
include(cmake/TargetProperties.cmake)

# Provides 'set_default_target'
include(cmake/Utils.cmake)

#############################################
# Dependencies

# Fetch Slang binaries
include(cmake/FetchSlang.cmake)

# Dependencies bundled within this codebase
add_subdirectory(third_party)

#############################################
# Targets

add_subdirectory(src)

if (SLANG_WEBGPU_BUILD_EXAMPLES)
	add_subdirectory(examples)
	set_default_target(slang_webgpu_example)
endif()

