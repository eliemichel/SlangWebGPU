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

# This provides utility CMake functions to use slang shaders in a WebGPU context.

#############################################
# Define a shader target, which is a custom target responsible for transpiling
# a slang shader into a WGSL shader.
# This target may then be used with target_link_slang_shaders.
#
# NB: This function only requires SLANGC to be set to the Slang compiler; it
# does not rely on any other mechanism specific to this repository so you may
# copy-paste it into your project.
#
# Example:
#   add_slang_shader(
#     hello_world
#     SOURCE shaders/hello-world.slang
#     ENTRY computeMain
#   )
function(add_slang_shader TargetName)
	set(options)
	set(oneValueArgs SOURCE ENTRY)
	set(multiValueArgs)
	cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	# TODO: Handle multiple entry points (generate one compute pipeline for each)

	# The input slang file
	set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/${arg_SOURCE}")

	# The generated WGSL file
	cmake_path(GET arg_SOURCE PARENT_PATH parent)
	cmake_path(GET arg_SOURCE STEM LAST_ONLY stem)
	set(WGSL_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/${parent}")
	set(WGSL_SHADER "${WGSL_SHADER_DIR}/${stem}.wgsl")

	# Target that represents the generated WGSL file in the dependency graph.
	add_custom_target(${TargetName}
		DEPENDS
			${WGSL_SHADER}
	)

	# Command that give the recipe to transpile slang into WGSL
	# i.e., internal behavior of the target ${TargetName} defined above
	add_custom_command(
		COMMENT
			"Transpiling shader '${SLANG_SHADER}' into '${WGSL_SHADER}' with entry point '${arg_ENTRY}'..."
		OUTPUT
			${WGSL_SHADER}
		COMMAND
			${CMAKE_COMMAND} -E make_directory ${WGSL_SHADER_DIR}
		COMMAND
			${SLANGC}
			${SLANG_SHADER}
			-entry ${arg_ENTRY}
			-target wgsl
			-o ${WGSL_SHADER}
		MAIN_DEPENDENCY
			${SLANG_SHADER}
		DEPENDS
			${SLANGC}
		# TODO: Add implicit dependencies by tracking includes
	)

	set_target_properties(${TargetName}
		PROPERTIES
		FOLDER "SlangWebGPU/shaders"
	)
endfunction(add_slang_shader)


#############################################
# Add a dependency link from a target to a shader target it relies on.
# The shader target must have been declared using add_slang_shader.
#
# Example:
#   target_link_slang_shaders(slang_webgpu_example
#     hello_world
#   )
function(target_link_slang_shaders TargetName)
	add_dependencies(
		${TargetName}
		${ARGN}
	)
endfunction(target_link_slang_shaders)


#############################################
# Create a target whose code is automatically generated from a Slang source
# file. This generates a class ${NAME}Kernel that targets which link to this
# target may use with include generated/${NAME}Kernel.h.
#
# NB: Contrary to 'add_slang_shader', this function is more tied to this
# repository's mechanism: it needs our code generator target to be defined.
#
# Example:
#   add_slang_webgpu_kernel(
#     generate_hello_world_kernel
#     NAME HelloWorld
#     SOURCE shaders/hello-world.slang
#     ENTRY computeMain
#   )
function(add_slang_webgpu_kernel TargetName)
	set(options)
	set(oneValueArgs NAME SOURCE)
	set(multiValueArgs ENTRY)
	cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# The input slang file
	set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/${arg_SOURCE}")
	cmake_path(GET SLANG_SHADER PARENT_PATH SLANG_SHADER_DIR)
	
	# Generator and template
	set(GENERATOR $<TARGET_FILE:slang_webgpu_generator>)
	set(TEMPLATE "${PROJECT_SOURCE_DIR}/src/generator/binding-template.tpl")

	# The generated WGSL file
	cmake_path(GET arg_SOURCE PARENT_PATH parent)
	cmake_path(GET arg_SOURCE STEM LAST_ONLY stem)
	set(WGSL_SHADER "${CMAKE_CURRENT_BINARY_DIR}/${parent}/${stem}.wgsl")

	# The generated C++ source
	set(KERNEL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/${arg_NAME}Kernel.h")
	set(KERNEL_IMPLEM "${CMAKE_CURRENT_BINARY_DIR}/generated/${arg_NAME}Kernel.cpp")

	# Command that give the recipe to transpile slang into WGSL
	# i.e., internal behavior of the target ${TargetName} defined above
	add_custom_command(
		COMMENT
"Generating Slang-WebGPU binding '${arg_NAME}Kernel' for shader \
'${SLANG_SHADER}' into '${WGSL_SHADER}' and '${KERNEL_HEADER}' with \
entry point '${arg_ENTRY}'..."
		OUTPUT
			${WGSL_SHADER}
			${KERNEL_HEADER}
			${KERNEL_IMPLEM}
		COMMAND
			${GENERATOR}
			--name ${arg_NAME}
			--input-slang ${SLANG_SHADER}
			--input-template ${TEMPLATE}
			--entrypoints ${arg_ENTRY}
			--output-wgsl ${WGSL_SHADER}
			--output-hpp ${KERNEL_HEADER}
			--output-cpp ${KERNEL_IMPLEM}
			--include-directories ${SLANG_SHADER_DIR}
		MAIN_DEPENDENCY
			${SLANG_SHADER}
		DEPENDS
			${GENERATOR}
			${TEMPLATE}
		# TODO: Add implicit dependencies by tracking includes
	)

	# Target that builds the generated binding
	add_library(${TargetName} STATIC)
	set_common_target_properties(${TargetName})
	target_sources(${TargetName}
		PRIVATE
		${KERNEL_HEADER}
		${KERNEL_IMPLEM}
	)
	set_target_properties(${TargetName}
		PROPERTIES
		FOLDER "SlangWebGPU/codegen"
	)
	# To be able to include "generated/FooKernel.h"
	target_include_directories(${TargetName}
		PUBLIC
		${CMAKE_CURRENT_BINARY_DIR}
	)
	target_link_libraries(${TargetName}
		PUBLIC
		webgpu
		slang_webgpu_common
	)
endfunction(add_slang_webgpu_kernel)
