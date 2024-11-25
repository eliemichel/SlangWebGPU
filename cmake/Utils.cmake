macro(set_default_target TargetName)
	set_property(
		DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		PROPERTY VS_STARTUP_PROJECT ${TargetName}
	)
endmacro()

