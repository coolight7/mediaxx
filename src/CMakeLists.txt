# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
project(mediaxx_library VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_GENERATOR_PLATFORM x64)

if (WIN32)
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /utf-8 /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /wd4200 /Zc:__cplusplus /std:c++23preview")
    set(LINK_FLAGS "${LINK_FLAGS} /LIBPATH:'D:\\0Acoolight\\Program\\flutter\\mymusic\\package\\mediaxx\\src\\third_part\\ffmpeg\\libs'")
else ()
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fPIC -pipe -std=c17")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=c++23 -fno-exceptions -fno-rtti")
    if (DEBUG) 
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    endif ()
endif ()

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/third_part/ffmpeg/include/)

set(EXE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("EXE_INSTALL_PREFIX: ${EXE_INSTALL_PREFIX}")

add_library(mediaxx SHARED
  "mediaxx.cpp"
)

target_link_directories(mediaxx PUBLIC "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/")
target_link_libraries(mediaxx PUBLIC
  avcodec
  avdevice
  avformat
  avutil
  swresample
  swscale
)

ADD_SUBDIRECTORY(test)
add_dependencies(test mediaxx)

set_target_properties(mediaxx PROPERTIES
  PUBLIC_HEADER mediaxx.h
  OUTPUT_NAME "mediaxx"
)
message(${CMAKE_BUILD_TYPE})

target_compile_definitions(mediaxx PUBLIC DART_SHARED_LIB)

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(mediaxx PRIVATE "-Wl,-z,max-page-size=16384")
endif()

install(TARGETS mediaxx RUNTIME DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)

install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avcodec.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avdevice.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avfilter.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avformat.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avutil.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/swresample.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/swscale.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)

install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avcodec.dll" DESTINATION "${EXE_INSTALL_PREFIX}" RENAME "avcodec-61.dll" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avdevice.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avfilter.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avformat.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/avutil.dll" DESTINATION "${EXE_INSTALL_PREFIX}" RENAME "avutil-59.dll" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/swresample.dll" DESTINATION "${EXE_INSTALL_PREFIX}" RENAME "swresample-5.dll" COMPONENT Runtime)
install(FILES "${CMAKE_SOURCE_DIR}/third_part/ffmpeg/libs/swscale.dll" DESTINATION "${EXE_INSTALL_PREFIX}" COMPONENT Runtime)