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

#############################################
# Information

# Utility functions that set target properties, with choices specific to this
# repository.

#############################################
# This set the target properties that are common across all targets of this
# repository.
#
# Example:
#   set_common_target_properties(my_target)
function(set_common_target_properties TargetName)

	set_target_properties(${TargetName}
		PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
		COMPILE_WARNING_AS_ERROR ON
		FOLDER "SlangWebGPU"
	)

	# Stronger warning level
	if(MSVC)
		target_compile_options(${TargetName} PRIVATE /W4)
	else()
		target_compile_options(${TargetName} PRIVATE -Wall -Wextra -Wpedantic)
	endif()

endfunction(set_common_target_properties)

#############################################
# Variant of set_common_target_properties that add extra settings for
# executable targets in the examples directory.
#
# Example:
#   set_example_target_properties(my_target)
function(set_example_target_properties TargetName)

	set_common_target_properties(${TargetName})

	set_target_properties(${TargetName}
		PROPERTIES
		FOLDER "SlangWebGPU/examples"
	)

	# All examples use WebGPU
	target_copy_webgpu_binaries(${TargetName})

	if (EMSCRIPTEN)

		# Generate a full web page rather than a simple WebAssembly module
		set_target_properties(${TargetName} PROPERTIES SUFFIX ".html")

		target_link_options(${TargetName}
			PRIVATE
			# ASYNCIFY is required by examples/common/webgpu-utils.h. It is
			# easily possible to avoid it (and thus produce lighter builds),
			# though it would add too much boilerplate for this example.
			-sASYNCIFY
		)

	endif (EMSCRIPTEN)

endfunction(set_example_target_properties)
