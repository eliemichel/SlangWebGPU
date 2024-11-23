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
#  - Target 'slang_static' is the static version of slang library.
#  - Variable 'SLANGC' points to the slangc executable.

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

add_library(slang_static STATIC IMPORTED GLOBAL)

# TODO: This is Windows only!!!
set_target_properties(
	slang_static
	PROPERTIES
		IMPORTED_LOCATION "${Slang_ROOT}/lib/slang.lib"
)

target_include_directories(slang_static INTERFACE
	"${Slang_ROOT}/include"
)

#############################################
# Set output variables

# TODO: This is Windows only!!!
SET(SLANGC "${Slang_ROOT}/bin/slangc.exe" CACHE PATH "Path to the slangc executable, set by FetchSlang.cmake")
