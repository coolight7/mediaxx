CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_test)
set(NAME_test test)

add_executable(${NAME_test} ${DIR_test})
target_link_libraries(${NAME_test}
	mediaxx
)
if (_ISMACOS)
  target_link_options(${NAME_test} PRIVATE 
    "-Wl,-rpath,@executable_path"
  )
endif()
if (_ISWIN)
	target_compile_definitions(
		${NAME_test} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

if (DEFINED LUMENXX_BUILD_TYPE)
	install(TARGETS ${NAME_test} DESTINATION "${EXE_INSTALL_PREFIX}")
endif()