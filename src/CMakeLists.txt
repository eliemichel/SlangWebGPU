add_subdirectory(common)

if (SLANG_WEBGPU_BUILD_GENERATOR)
	add_subdirectory(generator)

	# Export the generator target so that cross-compilation builds can import it
	# by setting SlangWebGPU_Generator_DIR to the current build directory.
	export(
		TARGETS slang_webgpu_generator
		FILE "${CMAKE_BINARY_DIR}/SlangWebGPU_GeneratorConfig.cmake"
	)
else()
	# If we do not build the generator, we then need to get it from elsewere
	find_package(SlangWebGPU_Generator REQUIRED)
endif()
