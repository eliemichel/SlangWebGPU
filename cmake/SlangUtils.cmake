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
# This target may then be used with target_link_slang_shaders
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
	set(WGSL_SHADER "${CMAKE_CURRENT_BINARY_DIR}/${parent}/${stem}.wgsl")

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
# TODO: document
function(generate_slang_webgpu_binding TargetBaseName)
	set(options)
	set(oneValueArgs NAME SOURCE)
	set(multiValueArgs ENTRY)
	cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# This function creates two targets based on the provided target base name
	set(WgslTarget generate_${TargetBaseName}_wgsl)
	set(BindingTarget generate_${TargetBaseName}_binding)

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
	set(GEN_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}Kernel.h")
	set(GEN_IMPLEM "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}Kernel.cpp")

	# Command that give the recipe to transpile slang into WGSL
	# i.e., internal behavior of the target ${TargetName} defined above
	add_custom_command(
		COMMENT
			"Generating Slang-WebGPU binding '${arg_NAME}Kernel' for shader \
			'${SLANG_SHADER}' into '${WGSL_SHADER}' and '${GEN_HEADER}' with \
			entry point '${arg_ENTRY}'..."
		OUTPUT
			${WGSL_SHADER}
			${GEN_HEADER}
			${GEN_IMPLEM}
		COMMAND
			${GENERATOR}
			--name ${arg_NAME}
			--input-slang ${SLANG_SHADER}
			--input-template ${TEMPLATE}
			--entrypoints ${arg_ENTRY}
			--output-wgsl ${WGSL_SHADER}
			--output-hpp ${GEN_HEADER}
			--output-cpp ${GEN_IMPLEM}
			--include-directories ${SLANG_SHADER_DIR}
		MAIN_DEPENDENCY
			${SLANG_SHADER}
		DEPENDS
			${GENERATOR}
			${TEMPLATE}
		CODEGEN
		# TODO: Add implicit dependencies by tracking includes
	)

	# Target that represents the generated WGSL file in the dependency graph.
	add_custom_target(${WgslTarget}
		DEPENDS
		${WGSL_SHADER}
	)
	set_target_properties(${WgslTarget}
		PROPERTIES
		FOLDER "SlangWebGPU/shaders"
	)

	# Target that builds the generated binding
	add_library(${BindingTarget} STATIC)
	set_common_target_properties(${BindingTarget})
	target_sources(${BindingTarget}
		PRIVATE
		${GEN_HEADER}
		${GEN_IMPLEM}
	)
	set_target_properties(${BindingTarget}
		PROPERTIES
		FOLDER "SlangWebGPU/codegen"
	)
	# To be able to include "generated/FooKernel.h"
	target_include_directories(${BindingTarget}
		PUBLIC
		${CMAKE_CURRENT_BINARY_DIR}
	)
endfunction(generate_slang_webgpu_binding)
