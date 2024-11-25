
# For older versions of CMake that do not provide PROJECT_IS_TOP_LEVEL
if (CMAKE_VERSION VERSION_LESS "3.21.0")

	if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
		set(PROJECT_IS_TOP_LEVEL ON)
	else()
		set(PROJECT_IS_TOP_LEVEL OFF)
	endif()

endif()

# For older versions of CMake that do not provide cmake_path
if (CMAKE_VERSION VERSION_LESS "3.20.0")

	function(cmake_path_get path_var subcommand out_var)

		if (subcommand STREQUAL "PARENT_PATH")
			get_filename_component(parent_path ${${path_var}} DIRECTORY)
			set(${out_var} ${parent_path} PARENT_SCOPE)
		elseif (subcommand STREQUAL "STEM")
			if (out_var STREQUAL "LAST_ONLY")
				set(mode NAME_WLE)
				set(out_var ${ARGN})
			else()
				set(mode NAME_WL)
			endif()
			get_filename_component(stem ${${path_var}} ${mode})
			set(${out_var} ${stem} PARENT_SCOPE)
		else()
			message(FATAL_ERROR "cmake_path(GET <path-var> ${subcommand} <out-var>) is not supported by this version of CMake, nor by CMakePolyfill.cmake.")
		endif()

		set(out_var ${out_var} PARENT_SCOPE)

	endfunction(cmake_path_get)

	function(cmake_path command)

		if (command STREQUAL "GET")
			cmake_path_get(${ARGN})
		else()
			message(FATAL_ERROR "cmake_path(${command} ...) is not supported by this version of CMake, nor by CMakePolyfill.cmake.")
		endif()

		if (out_var)
			set(${out_var} ${${out_var}} PARENT_SCOPE)
		endif()

	endfunction(cmake_path)

endif()
