# This file is part of the "Slang x WebGPU" demo.
#   https://github.com/eliemichel/SlangWebGPU
# 
# MIT License
# Copyright (c) 2024 Elie Michel
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

include(FetchContent)

#############################################
# Information

# This fetches a precompiled release of Slang, and makes it available through:
#  - Target 'slang' links to slang dynamic library.
#  - Variable 'SLANGC' points to the slangc executable.
#  - Function 'target_copy_slang_binaries' should be called for all executable
#    target the indirectly links slang so that the dll/dylib/so file of slang
#    is copied next to the generated binary.

#############################################
# Options

set(SLANG_VERSION "2024.14.4" CACHE STRING "Version of the Slang release to use, without the leading 'v'. Must correspond to a release available at https://github.com/shader-slang/slang/releases (or whatever the SLANG_MIRROR variable is set to).")
set(SLANG_MIRROR "https://github.com/shader-slang/slang" CACHE STRING "This is the source from which release binaries are fetched, set by default to official Slang repository but you may change it to use a fork.")

#############################################
# Target architecture detection (thank you CMake for not providing that...)
# (NB: Copied from third_party/webgpu/cmake/utils.cmake)
if (NOT ARCH)
	set(SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
	if (SYSTEM_PROCESSOR STREQUAL "AMD64" OR SYSTEM_PROCESSOR STREQUAL "x86_64")
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(ARCH "x86_64")
		elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(ARCH "i686")
		endif()
	elseif (SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv8|arm)$")
		set(ARCH "aarch64")
	elseif(SYSTEM_PROCESSOR MATCHES "^(armv7|armv6|armhf)$")
		set(ARCH "arm")
	else()
		message(WARNING "Unknown architecture: ${SYSTEM_PROCESSOR}")
		set(ARCH "unknown")
	endif()
endif()

#############################################
# Build URL to fetch
set(URL_OS)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(URL_OS "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(URL_OS "linux")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(URL_OS "macos")
else()
	message(FATAL_ERROR "Platform system '${CMAKE_SYSTEM_NAME}' not supported by this release of WebGPU. You may consider building it yourself from its source (see https://dawn.googlesource.com/dawn)")
endif()
set(URL_NAME "slang-${SLANG_VERSION}-${URL_OS}-${ARCH}")
string(TOLOWER "${URL_NAME}" FC_NAME)
set(URL "${SLANG_MIRROR}/releases/download/v${SLANG_VERSION}/${URL_NAME}.zip")

#############################################
# Declare FetchContent, then make available
FetchContent_Declare(${FC_NAME}
	URL ${URL}
)
message(STATUS "Using Slang binaries from '${URL}'")
FetchContent_MakeAvailable(${FC_NAME})
set(Slang_ROOT "${${FC_NAME}_SOURCE_DIR}")

#############################################
# Import targets (ideally slang would provide a SlangConfig.cmake)

add_library(slang SHARED IMPORTED GLOBAL)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (MSVC)
		set_target_properties(
			slang
			PROPERTIES
				IMPORTED_IMPLIB "${Slang_ROOT}/lib/slang.lib"
				IMPORTED_LOCATION "${Slang_ROOT}/bin/slang.dll"
		)
	else()
		message(FATAL_ERROR "Sorry, Slang does not provide precompiled binaries for MSYS/MinGW")
	endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set_target_properties(
		slang
		PROPERTIES
			IMPORTED_LOCATION "${Slang_ROOT}/lib/libslang.so"
			IMPORTED_NO_SONAME TRUE
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set_target_properties(
		slang
		PROPERTIES
			IMPORTED_LOCATION "${Slang_ROOT}/lib/libslang.dylib"
	)
endif()

target_include_directories(slang INTERFACE
	"${Slang_ROOT}/include"
)

#############################################
# Set output variables

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(EXE_EXT ".exe")
else()
	set(EXE_EXT)
endif()
SET(SLANGC "${Slang_ROOT}/bin/slangc${EXE_EXT}" CACHE PATH "Path to the slangc executable, set by FetchSlang.cmake")

#############################################
# Utility function

# Get path to .dll/.so/.dylib, for target_copy_slang_binaries
get_target_property(SLANG_RUNTIME_LIB slang IMPORTED_LOCATION)
message(STATUS "Using Slang runtime '${SLANG_RUNTIME_LIB}'")
set(SLANG_RUNTIME_LIB ${SLANG_RUNTIME_LIB} CACHE INTERNAL "Path to the Slang library binary")

# The application's binary must find the .dll/.so/.dylib at runtime,
# so we automatically copy it next to the binary.
function(target_copy_slang_binaries TargetName)
	add_custom_command(
		TARGET ${TargetName} POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different
			${SLANG_RUNTIME_LIB}
			$<TARGET_FILE_DIR:${TargetName}>
		COMMENT
			"Copying '${SLANG_RUNTIME_LIB}' to '$<TARGET_FILE_DIR:${TargetName}>'..."
	)
endfunction(target_copy_slang_binaries)
