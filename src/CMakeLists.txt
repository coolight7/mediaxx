# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0097 NEW)

project(mediaxx_library VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("[mediaxx] Current Configure Type: ${CMAKE_CONFIGURATION_TYPES}")
message("[mediaxx] Current Build Type: ${CMAKE_BUILD_TYPE}")

if (DEFINED WIN32)
    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /utf-8 /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /wd4200 /Zc:__cplusplus /std:c++23preview")
else ()
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fvisibility=hidden -fPIC -pipe -std=c17")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fvisibility=hidden -fPIC -pipe -std=c++23 -fno-exceptions -fno-rtti")
    if (DEBUG) 
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    endif ()
endif ()

set(EXE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("[mediaxx] CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("[mediaxx] EXE_INSTALL_PREFIX: ${EXE_INSTALL_PREFIX}")
message("[mediaxx] CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions(-D__STDC_CONSTANT_MACROS)

include(ExternalProject)
string(REPLACE ";" ";" MAIN_CONFIG_TYPES "${CMAKE_CONFIGURATION_TYPES}")
ExternalProject_Add(
  simdjson_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdjson/"
  UPDATE_COMMAND ""
  CMAKE_ARGS 
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
      "-DANDROID_ABI=${ANDROID_ABI}"
      "-DANDROID_NDK=${ANDROID_NDK}"
      "-DANDROID_STL=${ANDROID_STL}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
      "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"

      -DBUILD_SHARED_LIBS=OFF
      -DSIMDJSON_BUILD_STATIC_LIB=ON
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(simdjson_repo SOURCE_DIR)
set(simdjson_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(simdjson_repo BINARY_DIR)
set(simdjson_INSTALL_DIR "${BINARY_DIR}/")

# .vscode/c_cpp_properties.json 默认取 android
set(ffmpeg_INCLUDE_DIR ".")
set(ffmpeg_BINARY_DIR ".")
set(vulkan_INCLUDE_DIR ".")
set(ffmpeg_help_IMPL "")
if (DEFINED WIN32)
  set(ffmpeg_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-win/")
  set(ffmpeg_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-win/")
  set(vulkan_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-header/")
  set(ffmpeg_help_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win.cpp")
elseif (DEFINED ANDROID)
  message("[mediaxx] build for android | ANDROID_ABI: ${ANDROID_ABI}")
  set(ffmpeg_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-android/${ANDROID_ABI}/")
  set(ffmpeg_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-android/")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/)
include_directories(${ffmpeg_INCLUDE_DIR})
include_directories(${simdjson_SOURCE_DIR}/include/)
include_directories(${vulkan_INCLUDE_DIR})

aux_source_directory("./lib/util" DIR_UTIL)
aux_source_directory("./lib/analyse" DIR_ANALYSE)
aux_source_directory("./impl" DIR_IMPL)
aux_source_directory("./impl/android/" DIR_IMPL_ANDROID)

add_library(mediaxx SHARED
  ${DIR_IMPL}
  ${DIR_IMPL_ANDROID}
  ${DIR_UTIL}
  ${DIR_ANALYSE}
  ${ffmpeg_help_IMPL}
)

add_dependencies(mediaxx simdjson_repo)

target_link_directories(mediaxx PUBLIC 
  ${ffmpeg_BINARY_DIR}
  ${simdjson_INSTALL_DIR}
)

if (DEFINED ANDROID)
  find_library(log-lib log)
  find_library(android-lib android)
endif()

target_link_libraries(mediaxx PUBLIC
  ${log-lib}
  ${android-lib}
  simdjson
  avcodec
  avdevice
  avfilter
  avformat
  avutil
  swresample
  swscale
)

if (DEFINED LUMENXX_BUILD_TYPE)
  ADD_SUBDIRECTORY(test)
  add_dependencies(test mediaxx)
endif()

set_target_properties(mediaxx PROPERTIES
  PUBLIC_HEADER mediaxx.h
  OUTPUT_NAME "mediaxx"
)

target_compile_definitions(mediaxx PUBLIC DART_SHARED_LIB)

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(mediaxx PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  install(TARGETS mediaxx RUNTIME DESTINATION "${EXE_INSTALL_PREFIX}")

  install(FILES "${ffmpeg_BINARY_DIR}/avcodec.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avdevice.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avfilter.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avformat.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avutil.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/swresample.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/swscale.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
endif()
