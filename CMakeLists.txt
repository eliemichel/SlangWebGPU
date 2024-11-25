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
option(SLANG_WEBGPU_BUILD_GENERATOR "Build code generator (not compatible with cross-compilation). Alternatively, provide the path of a native generator build through the SlangWebGPU_Generator_DIR variable." ON)

#############################################
# Check setup validity

if (SLANG_WEBGPU_BUILD_GENERATOR AND CMAKE_CROSSCOMPILING)
	message(FATAL_ERROR "Cross-compiling  (e.g., to WebAssembly) requires to have a generator build already available for the host system. Configure first a native build (potentially with exampled turned off), then provide the through the SlangWebGPU_Generator_DIR variable.")
endif()

if (NOT SLANG_WEBGPU_BUILD_GENERATOR)
	find_package(SlangWebGPU_Generator QUIET)
	if (NOT SlangWebGPU_Generator_FOUND)
		message(FATAL_ERROR "When NOT building the SlangWebGPU (e.g., because you are cross-compiling to WebAssembly), you MUST provide the SlangWebGPU_Generator_DIR variable to point to a native build directory where the generator is exported.")
	endif()
endif()

if (EMSCRIPTEN)
	set(EMSCRIPTEN_EXPECTED_VERSION "3.1.72")
	if (NOT EMSCRIPTEN_VERSION VERSION_EQUAL EMSCRIPTEN_EXPECTED_VERSION)
		message(STATUS "Warning: Using emscripten ${EMSCRIPTEN_VERSION}, but this project was written for emscripten ${EMSCRIPTEN_EXPECTED_VERSION}. Since WebGPU evolves a lot these days, code may require manual adaptation.")
	endif()
endif (EMSCRIPTEN)

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
	set_default_target(slang_webgpu_example_02_multiple_entrypoints)
endif()

