CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_test)
set(NAME_test test)

if (WIN32)
else ()
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -g -rdynamic -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -rdynamic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-optimize-sibling-calls")
endif ()

add_executable(${NAME_test} ${DIR_test})
target_link_libraries(${NAME_test}
	mediaxx
)
if (WIN32)
	target_compile_definitions(
		${NAME_test} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

if (DEFINED LUMENXX_BUILD_TYPE)
	install(TARGETS ${NAME_test} RUNTIME DESTINATION "${EXE_INSTALL_PREFIX}")
endif()